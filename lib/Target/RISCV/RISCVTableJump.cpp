//===- RISCVTableJump.cpp-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "RISCVTableJump.h"
#include "RISCVLDBackend.h"
#include "eld/Core/Module.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Readers/Relocation.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/MathExtras.h"
#include <cstring>

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

void RISCVTableJumpFragment::scanTableJumpEntries(ELFSection &Sec) {
  if (!Sec.isCode())
    return;

  auto &Relocs = Sec.getRelocations();
  for (size_t I = 0; I < Relocs.size(); ++I) {
    const Relocation *R = Relocs[I];
    if (R->type() != llvm::ELF::R_RISCV_JAL &&
        R->type() != llvm::ELF::R_RISCV_CALL &&
        R->type() != llvm::ELF::R_RISCV_CALL_PLT)
      continue;

    if (I + 1 >= Relocs.size() ||
        Relocs[I + 1]->type() != llvm::ELF::R_RISCV_RELAX)
      continue;

    const ResolveInfo *Sym = R->symInfo();
    if (!Sym || Sym->isUndef())
      continue;

    uint32_t Insn = 0;
    uint8_t Rd = 0;
    if (R->type() == llvm::ELF::R_RISCV_JAL) {
      R->targetRef()->memcpy(&Insn, sizeof(Insn), 0);
      Rd = (Insn >> 7) & 0x1f;
    } else {
      // CALL/CALL_PLT: read the following JALR instruction to get rd.
      uint32_t Jalr = 0;
      R->targetRef()->memcpy(&Jalr, sizeof(Jalr), 4);
      Rd = (Jalr >> 7) & 0x1f;
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

    // If the jal/j can be relaxed to a 32-bit instruction, the saving becomes
    // actually 2 bytes (4->2), otherwise it's 6 bytes (8->2).
    const int Saved = llvm::isInt<21>(Displace) ? 2 : 6;

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
  llvm::sort(Entries, [](const auto &A, const auto &B) {
    return A.second.Saved > B.second.Saved;
  });

  if (Entries.size() > MaxSize)
    Entries.resize(MaxSize);

  const int WordSize = Backend.config().targets().is32Bits() ? 4 : 8;
  while (!Entries.empty() && Entries.back().second.Saved < WordSize)
    Entries.pop_back();

  for (size_t I = 0; I < Entries.size(); ++I)
    Candidates[Entries[I].first].Index = static_cast<int>(I);

  for (auto It = Candidates.begin(), End = Candidates.end(); It != End;) {
    if (It->second.Index >= 0)
      ++It;
    else
      Candidates.erase(It++);
  }
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

  if (SavedCMJTOnly <= 0) {
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
    uint64_t VA = KV.first->outSymbol()->value();
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
