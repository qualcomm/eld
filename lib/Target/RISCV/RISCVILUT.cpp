//===- RISCVILUT.cpp------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "RISCVILUT.h"
#include "RISCVLDBackend.h"
#include "RISCVRelocationInternal.h"
#include "eld/Core/Module.h"
#include "eld/Fragment/RegionFragmentEx.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Readers/Relocation.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/MathExtras.h"
#include <algorithm>
#include <cstring>
#include <map>
#include <set>

using namespace eld;

namespace {

struct Instruction {
  const Fragment *F = nullptr;
  uint64_t Offset = 0;
  unsigned Length = 0;
  std::vector<uint8_t> Bytes;
};

struct Pattern {
  std::string Key;
  std::vector<uint8_t> Bytes;
  unsigned EntrySize = 0;
  unsigned SourceSize = 0;
  unsigned Saving = 0;
  std::vector<Instruction> Occurrences;
};

static unsigned getInstructionLength(llvm::StringRef Bytes) {
  if (Bytes.size() < 2)
    return 0;
  uint16_t Half = llvm::support::endian::read16le(Bytes.data());
  if ((Half & 3) != 3)
    return 2;
  if (Bytes.size() < 4)
    return 0;
  uint32_t Word = llvm::support::endian::read32le(Bytes.data());
  // RISC-V 48-bit encodings have bits[4:2] set and opcode 0x1f.
  if ((Word & 0x1f) == 0x1f)
    return 6;
  // Do not walk through longer encodings as if they were 32-bit ones.
  switch (Word & 0x7f) {
  case 0x3f:
    return 8;
  case 0x5f:
    return 10;
  case 0x7f:
    return 12;
  default:
    return 4;
  }
}

static bool isPCReferencingInstruction(llvm::StringRef Bytes, unsigned Length) {
  uint16_t Half = llvm::support::endian::read16le(Bytes.data());
  if (Length == 2) {
    unsigned Quadrant = Half & 3;
    unsigned Funct3 = (Half >> 13) & 7;
    if (Quadrant == 1 &&
        (Funct3 == 1 || Funct3 == 5 || Funct3 == 6 || Funct3 == 7))
      return true;
    if (Quadrant == 2) {
      unsigned Funct4 = (Half >> 12) & 0xf;
      unsigned Rs1 = (Half >> 7) & 0x1f;
      unsigned Rs2 = (Half >> 2) & 0x1f;
      // c.jr and c.jalr have a non-zero rs1 and rs2 == 0. The other
      // encodings in these funct4 groups are c.mv, c.add, and c.ebreak;
      // only the jump forms reference PC.
      if (Rs1 != 0 && Rs2 == 0 && (Funct4 == 8 || Funct4 == 9))
        return true;
      unsigned ReturnForm = (Half >> 8) & 0xff;
      if (ReturnForm == 0xbe || ReturnForm == 0xbc)
        return true;
      // Xqci interrupt-return instructions also write PC.
      if (Half == 0x1912 || Half == 0x1992 || Half == 0x1a12)
        return true;
    }
    return false;
  }

  uint32_t Word = llvm::support::endian::read32le(Bytes.data());
  if (Length == 4) {
    switch (Word & 0x7f) {
    case 0x17: // auipc
    case 0x63: // conditional branch
    case 0x67: // jalr
    case 0x6f: // jal
      return true;
    default:
      return false;
    }
  }

  if (Length == 6 && (Word & 0x1f) == 0x1f) {
    unsigned Funct3 = (Word >> 12) & 7;
    unsigned Bits24_20 = (Word >> 20) & 0x1f;
    unsigned Bits24_23 = (Word >> 23) & 3;
    // Xqci return-like instructions are PC-referencing for ILUT purposes.
    return (Funct3 == 0 && Bits24_20 == 0) || (Funct3 == 4 && Bits24_23 == 3);
  }
  return true;
}

static bool isIlutInstruction(uint16_t Half) {
  // qc.cm.ilut itself must not be copied into the table.
  return (Half & 0xe003) == 0x2000;
}

static bool hasRelocationAt(ELFSection &Sec, const Fragment *F, uint64_t Offset,
                            unsigned Size) {
  for (const Relocation *R : Sec.getRelocations()) {
    if (R->type() == llvm::ELF::R_RISCV_RELAX ||
        R->type() == llvm::ELF::R_RISCV_NONE)
      continue;
    if (R->targetRef()->frag() != F)
      continue;
    uint64_t RelocOffset = R->targetRef()->offset();
    if (RelocOffset >= Offset && RelocOffset < Offset + Size)
      return true;
  }
  return false;
}

static bool isLegalPayload(const std::vector<unsigned> &Lengths,
                           unsigned &EntrySize) {
  if (Lengths.size() == 1 && Lengths[0] == 4) {
    EntrySize = 4;
    return true;
  }
  if (Lengths.size() == 1 && Lengths[0] == 6) {
    EntrySize = 8;
    return true;
  }
  if (Lengths.size() != 2)
    return false;
  unsigned A = Lengths[0], B = Lengths[1];
  if (A == 2 && B == 2) {
    EntrySize = 4;
    return true;
  }
  if ((A == 2 && (B == 4 || B == 6)) || (A == 4 && (B == 2 || B == 4))) {
    EntrySize = 8;
    return true;
  }
  return false;
}

static std::string makeKey(const std::vector<uint8_t> &Bytes,
                           const std::vector<unsigned> &Lengths) {
  std::string Key;
  for (unsigned Length : Lengths)
    Key.push_back(static_cast<char>(Length));
  Key.append(reinterpret_cast<const char *>(Bytes.data()), Bytes.size());
  return Key;
}

} // namespace

RISCVILUTFragment::RISCVILUTFragment(RISCVLDBackend &B, ELFSection *O)
    : TargetFragment(TargetFragment::Kind::TargetSpecific, O, nullptr,
                     /*Align=*/64, /*Size=*/0) {
  (void)B;
}

size_t RISCVILUTFragment::size() const { return ThisSize; }

eld::Expected<void> RISCVILUTFragment::emit(MemoryRegion &Mr, Module &M) {
  if (size() == 0)
    return {};
  uint8_t *Buf = Mr.begin() + getOffset(M.getConfig().getDiagEngine());
  std::memset(Buf, 0, size());
  writeTo(Buf);
  return {};
}

void RISCVILUTFragment::scanCodeSection(ELFSection &Sec) {
  if (!Sec.isCode())
    return;

  for (Fragment *F : Sec.getFragmentList()) {
    auto *Region = llvm::dyn_cast<eld::RegionFragmentEx>(F);
    if (!Region)
      continue;
    llvm::StringRef Data = Region->getRegion();
    std::vector<Instruction> Instructions;
    for (uint64_t Offset = 0; Offset < Data.size();) {
      unsigned Length = getInstructionLength(Data.substr(Offset));
      if (!Length || Offset + Length > Data.size())
        break;
      std::vector<uint8_t> Bytes(Length);
      std::memcpy(Bytes.data(), Data.data() + Offset, Length);
      bool Valid = !isPCReferencingInstruction(
          llvm::StringRef(reinterpret_cast<const char *>(Bytes.data()),
                          Bytes.size()),
          Length);
      if (Length == 2 &&
          isIlutInstruction(llvm::support::endian::read16le(Bytes.data())))
        Valid = false;
      if (Length != 2 && Length != 4 && Length != 6)
        Valid = false;
      if (hasRelocationAt(*Region->getOwningSection(), F, Offset, Length))
        Valid = false;
      if (Valid)
        Instructions.push_back({F, Offset, Length, std::move(Bytes)});
      Offset += Length;
    }

    std::map<std::string, Pattern> Patterns;
    for (size_t I = 0; I < Instructions.size(); ++I) {
      const Instruction &A = Instructions[I];
      std::vector<unsigned> Lengths = {A.Length};
      unsigned EntrySize = 0;
      if (isLegalPayload(Lengths, EntrySize)) {
        Pattern &P = Patterns[makeKey(A.Bytes, Lengths)];
        P.Key = makeKey(A.Bytes, Lengths);
        P.Bytes = A.Bytes;
        P.EntrySize = EntrySize;
        P.SourceSize = A.Length;
        P.Saving = A.Length - 2;
        P.Occurrences.push_back(A);
      }

      if (I + 1 >= Instructions.size())
        continue;
      const Instruction &B = Instructions[I + 1];
      if (A.F != B.F || A.Offset + A.Length != B.Offset)
        continue;
      Lengths = {A.Length, B.Length};
      if (!isLegalPayload(Lengths, EntrySize))
        continue;
      std::vector<uint8_t> Bytes = A.Bytes;
      Bytes.insert(Bytes.end(), B.Bytes.begin(), B.Bytes.end());
      Pattern &P = Patterns[makeKey(Bytes, Lengths)];
      P.Key = makeKey(Bytes, Lengths);
      P.Bytes = std::move(Bytes);
      P.EntrySize = EntrySize;
      P.SourceSize = A.Length + B.Length;
      P.Saving = P.SourceSize - 2;
      P.Occurrences.push_back(A);
    }

    llvm::SmallVector<Pattern *, 0> Ordered;
    for (auto &KV : Patterns)
      if (KV.second.Occurrences.size() * KV.second.Saving > KV.second.EntrySize)
        Ordered.push_back(&KV.second);
    llvm::sort(Ordered, [](const Pattern *A, const Pattern *B) {
      uint64_t AP = A->Occurrences.size() * A->Saving - A->EntrySize;
      uint64_t BP = B->Occurrences.size() * B->Saving - B->EntrySize;
      if (AP != BP)
        return AP > BP;
      return A->Key < B->Key;
    });

    std::set<std::pair<const Fragment *, uint64_t>> Occupied;
    for (Pattern *P : Ordered) {
      if (Entries.size() >= 2048)
        break;
      std::vector<Instruction> Selected;
      std::set<std::pair<const Fragment *, uint64_t>> CandidateOccupied;
      for (const Instruction &I : P->Occurrences) {
        bool Conflict = false;
        for (unsigned Byte = 0; Byte < P->SourceSize; ++Byte) {
          auto Key = std::make_pair(I.F, I.Offset + Byte);
          if (Occupied.count(Key) || CandidateOccupied.count(Key)) {
            Conflict = true;
            break;
          }
        }
        if (Conflict)
          continue;
        Selected.push_back(I);
        for (unsigned Byte = 0; Byte < P->SourceSize; ++Byte)
          CandidateOccupied.insert({I.F, I.Offset + Byte});
      }
      if (Selected.size() * P->Saving <= P->EntrySize)
        continue;
      Occupied.insert(CandidateOccupied.begin(), CandidateOccupied.end());

      unsigned EntryIndex = 0;
      for (; EntryIndex < Entries.size(); ++EntryIndex)
        if (Entries[EntryIndex].Size == P->EntrySize &&
            Entries[EntryIndex].Bytes == P->Bytes)
          break;
      if (EntryIndex == Entries.size())
        Entries.push_back({P->Bytes, P->EntrySize});
      for (const Instruction &I : Selected) {
        Uses.insert(
            {{I.F, I.Offset}, {I.F, I.Offset, P->SourceSize, EntryIndex}});
      }
    }
  }
}

void RISCVILUTFragment::finalizeContents() {
  // Put all double entries first so qc.itdec directly describes their count.
  std::vector<Entry> OldEntries = Entries;
  llvm::stable_sort(
      Entries, [](const Entry &A, const Entry &B) { return A.Size > B.Size; });

  // Rebuild source indices after the size-based ordering. Payloads are
  // deduplicated, making the old index unambiguous.
  std::vector<unsigned> Remap;
  for (const Entry &E : Entries) {
    unsigned Old = 0;
    for (; Old < OldEntries.size(); ++Old)
      if (OldEntries[Old].Size == E.Size && OldEntries[Old].Bytes == E.Bytes)
        break;
    Remap.push_back(Old);
  }
  for (auto &KV : Uses) {
    unsigned Old = KV.second.EntryIndex;
    for (unsigned New = 0; New < Remap.size(); ++New)
      if (Remap[New] == Old) {
        KV.second.EntryIndex = New;
        break;
      }
  }

  DEC = 0;
  while (DEC < Entries.size() && Entries[DEC].Size == 8)
    ++DEC;
  ThisSize = DEC * 8 + (Entries.size() - DEC) * 4;
}

bool RISCVILUTFragment::getRelaxation(const Fragment *F, uint64_t Offset,
                                      unsigned &Index,
                                      unsigned &SourceSize) const {
  auto It = Uses.find({F, Offset});
  if (It == Uses.end())
    return false;
  Index = It->second.EntryIndex;
  SourceSize = It->second.SourceSize;
  return true;
}

bool RISCVILUTFragment::applyRelaxations() {
  if (Applied)
    return false;
  Applied = true;
  bool Changed = false;

  std::vector<Use> Ordered;
  for (const auto &KV : Uses)
    Ordered.push_back(KV.second);
  llvm::stable_sort(Ordered, [](const Use &A, const Use &B) {
    if (A.F != B.F)
      return A.F < B.F;
    return A.Offset > B.Offset;
  });

  for (const Use &U : Ordered) {
    auto *Region = const_cast<eld::RegionFragmentEx *>(
        llvm::dyn_cast<eld::RegionFragmentEx>(U.F));
    if (!Region)
      continue;
    uint16_t Ilut = static_cast<uint16_t>(0x2000 | (U.EntryIndex << 2));
    Region->replaceInstruction(U.Offset, nullptr,
                               reinterpret_cast<uint8_t *>(&Ilut), 2);
    uint32_t DeleteOffset = U.Offset + 2;
    uint32_t DeleteSize = U.SourceSize - 2;
    for (Relocation *R : Region->getOwningSection()->getRelocations()) {
      if (R->targetRef()->frag() == U.F &&
          R->targetRef()->offset() == U.Offset &&
          R->type() == llvm::ELF::R_RISCV_RELAX) {
        R->setTargetData(Ilut);
        R->setType(eld::ELF::riscv::internal::R_RISCV_ILUT);
      }
      if (R->targetRef()->frag() == U.F &&
          R->targetRef()->offset() >= DeleteOffset &&
          R->targetRef()->offset() < DeleteOffset + DeleteSize)
        R->setType(llvm::ELF::R_RISCV_NONE);
    }
    if (DeleteSize)
      Region->deleteInstruction(DeleteOffset, DeleteSize);
    Changed = true;
  }
  return Changed;
}

void RISCVILUTFragment::writeTo(uint8_t *Buf) {
  size_t Offset = 0;
  for (const Entry &E : Entries) {
    std::memset(Buf + Offset, 0, E.Size);
    std::memcpy(Buf + Offset, E.Bytes.data(), E.Bytes.size());
    if (E.Size == 8 && E.Bytes.size() == 6) {
      // Unused upper half of a 48-bit instruction is c.nop (0x0001).
      Buf[Offset + 6] = 0x01;
      Buf[Offset + 7] = 0x00;
    }
    Offset += E.Size;
  }
}

void RISCVILUTFragment::dump(llvm::raw_ostream &OS) {
  if (Entries.empty())
    return;
  OS << "#\t.riscv.ilut entries (DEC=" << DEC << "):\n";
  for (size_t I = 0; I < Entries.size(); ++I) {
    OS << "#\t  [" << I << "] ";
    for (uint8_t Byte : Entries[I].Bytes)
      OS << llvm::format_hex_no_prefix(Byte, 2, true);
    OS << "\n";
  }
}
