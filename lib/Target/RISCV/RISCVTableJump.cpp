//===- RISCVTableJump.cpp-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//
// This file implements RISC-V table-jump handling for linker relaxation:
// - scans call/jal relocations and selects profitable table entries
// - materializes and emits the .riscv.jvt table
// - provides map-dump support for selected table-jump entries
//
// Zcmt and Xqccmt use the same 16-bit instruction encoding. Zcmt calls the
// instructions cm.jt/cm.jalt. Xqccmt calls them qc.cm.jt/qc.cm.jalt.
// Xqccmt also lets qc.cm.jalt write the return address to t0 (x5). It does
// that by storing bit 0 as metadata in the JVT entry. A clear bit means ra
// (x1). A set bit means t0 (x5). The real jump target always has bit 0 clear.
//
// ABI reference:
// https://riscv-non-isa.github.io/riscv-elf-psabi-doc/#_table_jump_relaxation

#include "RISCVTableJump.h"
#include "RISCVLDBackend.h"
#include "RISCVRelocationHelper.h"
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
#include <tuple>

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

static unsigned getCallTableCandidateCount(
    const llvm::DenseMap<const ResolveInfo *, RISCVTableJumpEntry>
        &CMJALTCandidates,
    const llvm::DenseMap<const ResolveInfo *, RISCVTableJumpEntry>
        &QCCMJALTTCandidates) {
  return CMJALTCandidates.size() + QCCMJALTTCandidates.size();
}

void RISCVTableJumpFragment::scanTableJumpEntries(ELFSection &Sec) {
  assert(Sec.isCode() && "table jump scan expects a code section");

  const bool UseXqccmt =
      Backend.config().options().getRISCVRelaxTbljalToXqccmt();
  HasXqccmt |= UseXqccmt;

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

    if (!Backend.hasRelax(*R))
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
      // QC.E.J has func2=0b00 and QC.E.JAL has func2=0b01. Use this as the
      // rd-equivalent selector because QC.E.J does not write a link register.
      Rd = getQCEJumpRd(R->target());
      if (!isValidQCEJumpRd(Rd))
        continue;
    } else {
      // CALL/CALL_PLT: read the following JALR to get rd.
      uint32_t Insn = 0;
      R->targetRef()->memcpy(&Insn, sizeof(Insn), 4);
      Rd = (Insn >> 7) & 0x1f;
    }

    // x0 uses the jump-only instruction.
    // ra uses the normal jump-and-link instruction.
    // Xqccmt also supports t0 by setting bit 0 in the JVT entry.
    if (Rd != 0 && Rd != 1 && !(UseXqccmt && Rd == 5))
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
    else if (Rd == 5)
      addEntry(QCCMJALTTCandidates, Sym, Saved);
    else
      addEntry(CMJALTCandidates, Sym, Saved);
  }
}

static void selectEntries(
    RISCVLDBackend &Backend,
    llvm::DenseMap<const ResolveInfo *, RISCVTableJumpEntry> &Candidates,
    uint32_t MaxSize) {
  llvm::SmallVector<std::pair<const ResolveInfo *, RISCVTableJumpEntry>, 0>
      Entries;
  const int WordSize = Backend.config().targets().is32Bits() ? 4 : 8;
  for (const auto &KV : Candidates)
    if (KV.second.Saved >= WordSize)
      Entries.push_back(KV);

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

  Candidates.clear();
  int Index = 0;
  for (auto Entry : Entries) {
    Entry.second.Index = Index++;
    Candidates.insert(Entry);
  }
}

static void selectCallEntries(
    RISCVLDBackend &Backend,
    llvm::DenseMap<const ResolveInfo *, RISCVTableJumpEntry> &CMJALTCandidates,
    llvm::DenseMap<const ResolveInfo *, RISCVTableJumpEntry>
        &QCCMJALTTCandidates,
    uint32_t MaxSize) {
  struct CallEntry {
    const ResolveInfo *Sym = nullptr;
    RISCVTableJumpEntry Entry;
    bool UseT0 = false;
  };

  const int WordSize = Backend.config().targets().is32Bits() ? 4 : 8;
  llvm::SmallVector<CallEntry, 0> Entries;
  auto AddCandidate = [&](const auto &KV, bool UseT0) {
    if (KV.second.Saved >= WordSize)
      Entries.push_back({KV.first, KV.second, UseT0});
  };

  // Add the normal ra entries. They are valid for both Zcmt and Xqccmt.
  for (const auto &KV : CMJALTCandidates)
    AddCandidate(KV, /*UseT0=*/false);

  // Add the Xqccmt t0 entries. They need their own JVT slot even when the
  // symbol is the same as an ra entry, because bit 0 in the slot is different.
  for (const auto &KV : QCCMJALTTCandidates)
    AddCandidate(KV, /*UseT0=*/true);

  llvm::sort(Entries, [&Backend](const CallEntry &A, const CallEntry &B) {
    const uint64_t AVA = getTableJumpTargetVA(Backend, A.Sym);
    const uint64_t BVA = getTableJumpTargetVA(Backend, B.Sym);
    const int ASaved = -A.Entry.Saved;
    const int BSaved = -B.Entry.Saved;
    const llvm::StringRef AName = A.Sym->name();
    const llvm::StringRef BName = B.Sym->name();
    return std::tie(ASaved, AVA, AName, A.UseT0) <
           std::tie(BSaved, BVA, BName, B.UseT0);
  });

  if (Entries.size() > MaxSize)
    Entries.resize(MaxSize);

  CMJALTCandidates.clear();
  QCCMJALTTCandidates.clear();

  int Index = 0;
  for (CallEntry Entry : Entries) {
    Entry.Entry.Index = Index++;
    if (Entry.UseT0)
      QCCMJALTTCandidates.insert({Entry.Sym, Entry.Entry});
    else
      CMJALTCandidates.insert({Entry.Sym, Entry.Entry});
  }
}

void RISCVTableJumpFragment::finalizeContents() {
  selectEntries(Backend, CMJTCandidates, MaxCMJTEntrySize);
  selectCallEntries(Backend, CMJALTCandidates, QCCMJALTTCandidates,
                    MaxCMJALTEntrySize);

  const int WordSize = Backend.config().targets().is32Bits() ? 4 : 8;
  const unsigned CallEntryCount =
      getCallTableCandidateCount(CMJALTCandidates, QCCMJALTTCandidates);

  // Profitability starts negative with the table emission cost. Candidate
  // savings are added to this and if the result stays negative we discard the
  // candidates since the jump table would increase code size.
  int SavedBoth =
      -static_cast<int>(StartCMJALTEntryIdx + CallEntryCount) * WordSize;
  int SavedCMJTOnly = -static_cast<int>(CMJTCandidates.size()) * WordSize;

  for (auto &KV : CMJTCandidates) {
    SavedCMJTOnly += KV.second.Saved;
    SavedBoth += KV.second.Saved;
  }
  for (auto &KV : CMJALTCandidates)
    SavedBoth += KV.second.Saved;
  for (auto &KV : QCCMJALTTCandidates)
    SavedBoth += KV.second.Saved;

  // Using the call table requires padding the jump table to 32 entries.
  // If that padding loses more bytes than the calls save, keep only cm.jt.
  if (CallEntryCount != 0 && SavedBoth < SavedCMJTOnly) {
    CMJALTCandidates.clear();
    QCCMJALTTCandidates.clear();
  }

  if (SavedCMJTOnly < 0) {
    CMJTCandidates.clear();
    CMJALTCandidates.clear();
    QCCMJALTTCandidates.clear();
  }

  const unsigned FinalCallEntryCount =
      getCallTableCandidateCount(CMJALTCandidates, QCCMJALTTCandidates);
  if (FinalCallEntryCount != 0)
    ThisSize = (StartCMJALTEntryIdx + FinalCallEntryCount) * WordSize;
  else
    ThisSize = CMJTCandidates.size() * WordSize;
}

int RISCVTableJumpFragment::getCMJTEntryIndex(const ResolveInfo *Sym) const {
  auto It = CMJTCandidates.find(Sym);
  return It != CMJTCandidates.end() ? It->second.Index : -1;
}

int RISCVTableJumpFragment::getCMJALTEntryIndex(const ResolveInfo *Sym,
                                                unsigned Rd) const {
  // ra and t0 call entries share one JVT index space, but are tracked in
  // separate maps because Xqccmt encodes the link-register choice in bit 0 of
  // the table entry rather than in the instruction.
  const auto &Candidates = Rd == 5 ? QCCMJALTTCandidates : CMJALTCandidates;
  auto It = Candidates.find(Sym);
  return It != Candidates.end()
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
  const char *JumpInsn = HasXqccmt ? "qc.cm.jt" : "cm.jt";
  const char *CallInsn = HasXqccmt ? "qc.cm.jalt" : "cm.jalt";
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
  AddEntries(CMJTCandidates, /*Bias=*/0, JumpInsn);
  AddEntries(CMJALTCandidates, static_cast<int>(StartCMJALTEntryIdx), CallInsn);
  AddEntries(QCCMJALTTCandidates, static_cast<int>(StartCMJALTEntryIdx),
             CallInsn);
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

static void
writeCallEntries(RISCVLDBackend &Backend, uint8_t *Buf,
                 unsigned StartCallEntryIdx,
                 const llvm::DenseMap<const ResolveInfo *, RISCVTableJumpEntry>
                     &CMJALTCandidates,
                 const llvm::DenseMap<const ResolveInfo *, RISCVTableJumpEntry>
                     &QCCMJALTTCandidates) {
  const int WordSize = Backend.config().targets().is32Bits() ? 4 : 8;

  auto WriteOne = [&](const ResolveInfo *Sym, const RISCVTableJumpEntry &Entry,
                      bool SetEntryBit0) {
    uint64_t VA = getTableJumpTargetVA(Backend, Sym);
    if (SetEntryBit0)
      VA |= 1;
    uint8_t *EntryBuf = Buf + (StartCallEntryIdx + Entry.Index) * WordSize;
    if (Backend.config().targets().is32Bits())
      llvm::support::endian::write32le(EntryBuf, static_cast<uint32_t>(VA));
    else
      llvm::support::endian::write64le(EntryBuf, VA);
  };

  // Call entries from both maps share one index space. The maps are not laid
  // out in separate ra-then-t0 ranges, so write every entry to its exact index.
  for (auto &KV : CMJALTCandidates)
    WriteOne(KV.first, KV.second, /*SetEntryBit0=*/false);
  for (auto &KV : QCCMJALTTCandidates)
    WriteOne(KV.first, KV.second, /*SetEntryBit0=*/true);
}

void RISCVTableJumpFragment::writeTo(uint8_t *Buf) {
  if (!CMJTCandidates.empty())
    writeEntries(Backend, Buf, CMJTCandidates);
  if (!CMJALTCandidates.empty() || !QCCMJALTTCandidates.empty())
    writeCallEntries(Backend, Buf, StartCMJALTEntryIdx, CMJALTCandidates,
                     QCCMJALTTCandidates);
}
