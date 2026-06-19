//===- RISCVTableJump.cpp-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//
// This file implements RISC-V Zcmt table-jump handling for linker relaxation:
// - scans call/jal relocations and selects profitable cm.jt/cm.jalt entries
// - materializes and emits the .riscv.jvt table
// - provides map-dump support for selected table-jump entries
//
// ABI reference:
// https://riscv-non-isa.github.io/riscv-elf-psabi-doc/#_table_jump_relaxation

#include "RISCVTableJump.h"
#include "RISCVLDBackend.h"
#include "RISCVRelocationInternal.h"
#include "eld/Core/Module.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Readers/Relocation.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/MathExtras.h"
#include <cassert>

using namespace eld;
RISCVTableJumpFragment::RISCVTableJumpFragment(RISCVLDBackend &B, ELFSection *O)
    : TargetFragment(TargetFragment::Kind::TargetSpecific, O, nullptr,
                     /*Align=*/64, /*Size=*/0),
      Backend(B) {}

size_t RISCVTableJumpFragment::size() const { return ThisSize; }

eld::Expected<void> RISCVTableJumpFragment::emit(MemoryRegion &Mr, Module &M) {
  if (size() == 0)
    return {};
  uint8_t *Buf = Mr.begin() + getOffset(M.getConfig().getDiagEngine());
  memset(Buf, 0, size());
  writeTo(Buf);
  return {};
}

static void
addEntry(llvm::DenseMap<const ResolveInfo *, RISCVTableJumpEntry> &M,
         const ResolveInfo *Sym, int Saved) {
  auto &E = M[Sym];
  E.Saved += Saved;
}

static int64_t getCallDisplace(RISCVLDBackend &Backend, const Relocation &R) {
  int64_t S = static_cast<int64_t>(Backend.getSymbolValuePLT(R));
  int64_t A = static_cast<int64_t>(R.addend());
  int64_t P = static_cast<int64_t>(R.place(Backend.getModule()));
  return S + A - P;
}

static uint64_t getTableJumpTargetVA(RISCVLDBackend &Backend,
                                     const ResolveInfo *Sym) {
  return Backend.getSymbolValuePLT(const_cast<ResolveInfo &>(*Sym));
}

void RISCVTableJumpFragment::scanTableJumpEntries(ELFSection &Sec) {
  assert(Sec.isCode() && "table jump scan expects a code section");

  auto RelocRange = Sec.getRelocations();
  llvm::SmallVector<const Relocation *, 0> Relocs(RelocRange.begin(),
                                                  RelocRange.end());
  for (size_t I = 0; I < Relocs.size(); ++I) {
    const Relocation *R = Relocs[I];
    if (R->type() != llvm::ELF::R_RISCV_JAL &&
        R->type() != llvm::ELF::R_RISCV_CALL &&
        R->type() != llvm::ELF::R_RISCV_CALL_PLT &&
        R->type() != eld::ELF::riscv::internal::R_RISCV_QC_E_CALL_PLT)
      continue;

    if (I + 1 >= Relocs.size() ||
        Relocs[I + 1]->type() != llvm::ELF::R_RISCV_RELAX)
      continue;

    if (R->type() == eld::ELF::riscv::internal::R_RISCV_QC_E_CALL_PLT &&
        (!Backend.config().options().getRISCVRelax() ||
         !Backend.config().targets().is32Bits() ||
         !Backend.config().options().getRISCVRelaxXqci() ||
         !Backend.config().options().getRISCVRelaxToC() ||
         !llvm::isUInt<32>(Backend.getSymbolValuePLT(*R) + R->addend())))
      continue;

    const ResolveInfo *Sym = R->symInfo();
    if (!Sym || Sym->isUndef())
      continue;

    uint8_t Rd = 0;
    if (R->type() == llvm::ELF::R_RISCV_JAL) {
      Rd = (R->target() >> 7) & 0x1f;
    } else if (R->type() == eld::ELF::riscv::internal::R_RISCV_QC_E_CALL_PLT) {
      // QC.E.J has func2=0b00 and QC.E.JAL has func2=0b01. Use this as
      // the rd-equivalent selector because QC.E.J does not write ra.
      Rd = (R->target() >> 15) & 0x3;
    } else {
      // CALL/CALL_PLT: read the following JALR to get rd.
      uint32_t Insn = 0;
      R->targetRef()->memcpy(&Insn, sizeof(Insn), 4);
      Rd = (Insn >> 7) & 0x1f;
    }

    // Only x0 (cm.jt) and ra (cm.jalt) are supported.
    if (Rd != 0 && Rd != 1)
      continue;

    const int64_t Displace = getCallDisplace(Backend, *R);

    // Skip the jal/j which can be potentially relaxed to c.jal/c.j.
    if (Backend.config().options().getRISCVRelaxToC() &&
        llvm::isInt<12>(Displace)) {
      if ((Rd == 0) || (Rd == 1 && Backend.config().targets().is32Bits()))
        continue;
    }

    int Saved = 0;
    if (R->type() == eld::ELF::riscv::internal::R_RISCV_QC_E_CALL_PLT) {
      // For qc.e.j/qc.e.jal, tbljal saves 4 bytes (6->2). If the instruction
      // can already relax to JAL, the incremental saving is only 2 bytes
      // (4->2), matching the profitability model used for CALL/JAL.
      const bool CanRelaxToJAL =
          Backend.config().targets().is32Bits() &&
          Backend.config().options().getRISCVRelaxXqci() &&
          llvm::isInt<21>(Displace);
      Saved = CanRelaxToJAL ? 2 : 4;
    } else {
      // If the jal/j can be relaxed to a 32-bit instruction, the saving
      // becomes actually 2 bytes (4->2), otherwise it's 6 bytes (8->2).
      Saved = llvm::isInt<21>(Displace) ? 2 : 6;
    }

    if (Rd == 0)
      addEntry(CMJTCandidates, Sym, Saved);
    else
      addEntry(CMJALTCandidates, Sym, Saved);
  }
}

static void selectEntries(
    RISCVLDBackend &Backend,
    llvm::DenseMap<const ResolveInfo *, RISCVTableJumpEntry> &Candidates,
    uint32_t MaxSize) {
  llvm::SmallVector<std::pair<const ResolveInfo *, RISCVTableJumpEntry>, 0>
      Entries(Candidates.begin(), Candidates.end());
  llvm::sort(Entries, [&Backend](const auto &A, const auto &B) {
    if (A.second.Saved != B.second.Saved)
      return A.second.Saved > B.second.Saved;
    const uint64_t AVA = getTableJumpTargetVA(Backend, A.first);
    const uint64_t BVA = getTableJumpTargetVA(Backend, B.first);
    if (AVA != BVA)
      return AVA < BVA;
    return A.first->name() < B.first->name();
  });

  if (Entries.size() > MaxSize)
    Entries.resize(MaxSize);

  const int WordSize = Backend.config().targets().is32Bits() ? 4 : 8;
  while (!Entries.empty() && Entries.back().second.Saved < WordSize)
    Entries.pop_back();

  for (size_t I = 0; I < Entries.size(); ++I)
    Candidates[Entries[I].first].Index = static_cast<int>(I);

  llvm::SmallVector<const ResolveInfo *, 0> ToErase;
  for (const auto &KV : Candidates)
    if (KV.second.Index < 0)
      ToErase.push_back(KV.first);

  for (const ResolveInfo *Sym : ToErase)
    Candidates.erase(Sym);
}

void RISCVTableJumpFragment::finalizeContents() {
  selectEntries(Backend, CMJTCandidates, MaxCMJTEntrySize);
  selectEntries(Backend, CMJALTCandidates, MaxCMJALTEntrySize);

  const int WordSize = Backend.config().targets().is32Bits() ? 4 : 8;

  int SavedBoth = -static_cast<int>(
      (StartCMJALTEntryIdx + CMJALTCandidates.size()) * WordSize);
  int SavedCMJTOnly = -static_cast<int>(CMJTCandidates.size() * WordSize);

  for (auto &KV : CMJTCandidates) {
    SavedCMJTOnly += KV.second.Saved;
    SavedBoth += KV.second.Saved;
  }
  for (auto &KV : CMJALTCandidates)
    SavedBoth += KV.second.Saved;

  // Using cm.jalt requires padding the cm.jt region to 32 entries.
  if (!CMJALTCandidates.empty() && SavedBoth < SavedCMJTOnly)
    CMJALTCandidates.clear();

  if (SavedCMJTOnly < 0) {
    CMJTCandidates.clear();
    CMJALTCandidates.clear();
  }

  if (!CMJALTCandidates.empty())
    ThisSize = (StartCMJALTEntryIdx + CMJALTCandidates.size()) * WordSize;
  else
    ThisSize = CMJTCandidates.size() * WordSize;
}

int RISCVTableJumpFragment::getCMJTEntryIndex(const ResolveInfo *Sym) const {
  auto It = CMJTCandidates.find(Sym);
  return It != CMJTCandidates.end() ? It->second.Index : -1;
}

int RISCVTableJumpFragment::getCMJALTEntryIndex(const ResolveInfo *Sym) const {
  auto It = CMJALTCandidates.find(Sym);
  return It != CMJALTCandidates.end()
             ? static_cast<int>(StartCMJALTEntryIdx) + It->second.Index
             : -1;
}

void RISCVTableJumpFragment::dump(llvm::raw_ostream &OS) {
  struct DumpEntry {
    int Index = -1;
    const ResolveInfo *Sym = nullptr;
    uint64_t VA = 0;
    const char *Insn = "";
  };

  llvm::SmallVector<DumpEntry, 0> Entries;
  auto AddEntries = [&](const llvm::DenseMap<const ResolveInfo *,
                                             RISCVTableJumpEntry> &Candidates,
                        int Bias, const char *Insn) {
    for (auto &KV : Candidates) {
      if (KV.second.Index < 0)
        continue;
      Entries.push_back({Bias + KV.second.Index, KV.first,
                         getTableJumpTargetVA(Backend, KV.first), Insn});
    }
  };
  AddEntries(CMJTCandidates, /*Bias=*/0, "cm.jt");
  AddEntries(CMJALTCandidates, static_cast<int>(StartCMJALTEntryIdx),
             "cm.jalt");
  if (Entries.empty())
    return;

  llvm::sort(Entries, [](const DumpEntry &A, const DumpEntry &B) {
    return A.Index < B.Index;
  });
  OS << "#\t.riscv.jvt entries:\n";
  for (const auto &Entry : Entries) {
    OS << "#\t  [" << Entry.Index << "] " << Entry.Insn << "\t"
       << Entry.Sym->name() << "\t0x";
    OS.write_hex(Entry.VA);
    OS << "\n";
  }
}

static void
writeEntries(RISCVLDBackend &Backend, uint8_t *Buf,
             const llvm::DenseMap<const ResolveInfo *, RISCVTableJumpEntry>
                 &Candidates) {
  llvm::SmallVector<std::pair<const ResolveInfo *, RISCVTableJumpEntry>, 0>
      Entries(Candidates.begin(), Candidates.end());
  llvm::sort(Entries, [](const auto &A, const auto &B) {
    return A.second.Index < B.second.Index;
  });

  for (auto &KV : Entries) {
    uint64_t VA = getTableJumpTargetVA(Backend, KV.first);
    if (Backend.config().targets().is32Bits())
      llvm::support::endian::write32le(Buf, static_cast<uint32_t>(VA));
    else
      llvm::support::endian::write64le(Buf, VA);
    Buf += Backend.config().targets().is32Bits() ? 4 : 8;
  }
}

void RISCVTableJumpFragment::writeTo(uint8_t *Buf) {
  if (!CMJTCandidates.empty())
    writeEntries(Backend, Buf, CMJTCandidates);
  if (!CMJALTCandidates.empty()) {
    const int WordSize = Backend.config().targets().is32Bits() ? 4 : 8;
    writeEntries(Backend, Buf + StartCMJALTEntryIdx * WordSize,
                 CMJALTCandidates);
  }
}
