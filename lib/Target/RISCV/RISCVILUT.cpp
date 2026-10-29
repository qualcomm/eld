//===- RISCVILUT.cpp------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "RISCVILUT.h"
#include "RISCVHelper.h"
#include "RISCVLDBackend.h"
#include "RISCVRelocationInternal.h"
#include "eld/Core/Module.h"
#include "eld/Fragment/RegionFragmentEx.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Readers/Relocation.h"
#include "eld/SymbolResolver/LDSymbol.h"
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

static bool isUnsafeForILUT(llvm::StringRef Bytes, unsigned Length) {
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
    case 0x7b: {
      // Xqcibi uses CUSTOM_3 for its 32-bit branches. Other vendor
      // instructions also use this opcode, so recognize only the funct3
      // values assigned to QC_BEQI/QC_BNEI/QC_BLTI/QC_BGEI/QC_BLTUI/QC_BGEUI.
      unsigned Funct3 = (Word >> 12) & 7;
      return Funct3 == 0 || Funct3 == 1 || Funct3 == 4 || Funct3 == 5 ||
             Funct3 == 6 || Funct3 == 7;
    }
    default:
      return false;
    }
  }

  if (Length == 6 && (Word & 0x1f) == 0x1f) {
    // Xqci 48-bit conditional branches use bits 14:12 as funct3.
    // Other instructions, including qc.e.orai, must remain legal ILUT
    // payloads.
    unsigned Funct3 = (Word >> 12) & 7;
    return Funct3 == 4;
  }
  return true;
}

static bool isIlutInstruction(uint16_t Half) {
  // qc.cm.ilut itself must not be copied into the table.
  return (Half & 0xe003) == 0x2000;
}

static const Relocation *getRelocationAt(ELFSection &Sec, const Fragment *F,
                                         uint64_t Offset, unsigned Size) {
  for (const Relocation *R : Sec.getRelocations()) {
    if (R->type() == llvm::ELF::R_RISCV_RELAX ||
        R->type() == llvm::ELF::R_RISCV_NONE)
      continue;
    if (R->targetRef()->frag() != F)
      continue;
    uint64_t RelocOffset = R->targetRef()->offset();
    if (RelocOffset >= Offset && RelocOffset < Offset + Size)
      return R;
  }
  return nullptr;
}

static bool hasRelocationAt(ELFSection &Sec, const Fragment *F, uint64_t Offset,
                            unsigned Size) {
  return getRelocationAt(Sec, F, Offset, Size) != nullptr;
}

enum class DirectPCRelativeKind {
  Branch,
  QC_EBranch,
  Jump,
  RVCBranch,
  RVCJump
};

struct DirectPCRelativePatch {
  uint32_t Offset;
  uint64_t Instruction;
  unsigned Size;
};

static uint64_t readInstruction(llvm::StringRef Data, uint32_t Offset,
                                unsigned Size) {
  if (Size == 2)
    return llvm::support::endian::read16le(Data.data() + Offset);
  if (Size == 4)
    return llvm::support::endian::read32le(Data.data() + Offset);
  if (Size == 6) {
    uint64_t Low = llvm::support::endian::read32le(Data.data() + Offset);
    uint64_t High = llvm::support::endian::read16le(Data.data() + Offset + 4);
    return Low | (High << 32);
  }
  return 0;
}

static void writeInstruction(uint8_t *Bytes, uint64_t Instruction,
                             unsigned Size) {
  if (Size == 2) {
    llvm::support::endian::write16le(Bytes, static_cast<uint16_t>(Instruction));
    return;
  }
  if (Size == 4) {
    llvm::support::endian::write32le(Bytes, static_cast<uint32_t>(Instruction));
    return;
  }
  if (Size == 6) {
    llvm::support::endian::write32le(Bytes, static_cast<uint32_t>(Instruction));
    llvm::support::endian::write16le(Bytes + 4,
                                     static_cast<uint16_t>(Instruction >> 32));
  }
}

static bool decodeDirectPCRelative(uint64_t Instruction, unsigned Size,
                                   DirectPCRelativeKind &Kind,
                                   int64_t &Immediate) {
  if (Size == 2) {
    uint16_t Half = static_cast<uint16_t>(Instruction);
    unsigned Quadrant = Half & 3;
    unsigned Funct3 = (Half >> 13) & 7;
    if (Quadrant != 1)
      return false;
    if (Funct3 == 6 || Funct3 == 7) {
      uint32_t Imm = ((Half >> 12) & 1) << 8;
      Imm |= ((Half >> 5) & 3) << 6;
      Imm |= ((Half >> 2) & 1) << 5;
      Imm |= ((Half >> 10) & 3) << 3;
      Imm |= ((Half >> 3) & 3) << 1;
      if (Imm & 0x100)
        Imm |= ~0x1ffu;
      Kind = DirectPCRelativeKind::RVCBranch;
      Immediate = static_cast<int32_t>(Imm);
      return true;
    }
    if (Funct3 == 1 || Funct3 == 5) {
      uint32_t Imm = ((Half >> 12) & 1) << 11;
      Imm |= ((Half >> 8) & 1) << 10;
      Imm |= ((Half >> 9) & 3) << 8;
      Imm |= ((Half >> 6) & 1) << 7;
      Imm |= ((Half >> 7) & 1) << 6;
      Imm |= ((Half >> 2) & 1) << 5;
      Imm |= ((Half >> 11) & 1) << 4;
      Imm |= ((Half >> 3) & 7) << 1;
      if (Imm & 0x800)
        Imm |= ~0xfffu;
      Kind = DirectPCRelativeKind::RVCJump;
      Immediate = static_cast<int32_t>(Imm);
      return true;
    }
    return false;
  }

  if (Size == 6) {
    // Xqcibi QC_E conditional branches are 48-bit instructions. Their branch
    // displacement is encoded as a signed 12-bit halfword offset using the
    // standard SB bit placement in the low 32 bits. The low bit is implicit,
    // so convert it to a byte displacement before using it as a section
    // offset.
    if ((Instruction & 0x1f) != 0x1f || ((Instruction >> 12) & 7) != 4)
      return false;
    uint32_t Imm = ((Instruction >> 31) & 1) << 11;
    Imm |= ((Instruction >> 25) & 0x3f) << 4;
    Imm |= ((Instruction >> 8) & 0xf);
    Imm |= ((Instruction >> 7) & 1) << 10;
    int32_t SignedImm = static_cast<int32_t>(Imm << 20) >> 20;
    Kind = DirectPCRelativeKind::QC_EBranch;
    Immediate = static_cast<int64_t>(SignedImm) << 1;
    return true;
  }

  if (Size != 4)
    return false;
  uint32_t Opcode = static_cast<uint32_t>(Instruction) & 0x7f;
  // Xqcibi 32-bit QC_* branches use the standard SB-type displacement
  // encoding with CUSTOM_3, so repair them like ordinary conditional branches.
  if (Opcode == 0x63 || Opcode == 0x7b) {
    uint32_t Imm = ((Instruction >> 31) & 1) << 12;
    Imm |= ((Instruction >> 25) & 0x3f) << 5;
    Imm |= ((Instruction >> 8) & 0xf) << 1;
    Imm |= ((Instruction >> 7) & 1) << 11;
    if (Imm & 0x1000)
      Imm |= ~0x1fffu;
    Kind = DirectPCRelativeKind::Branch;
    Immediate = static_cast<int32_t>(Imm);
    return true;
  }
  if (Opcode == 0x6f) {
    uint32_t Imm = ((Instruction >> 31) & 1) << 20;
    Imm |= ((Instruction >> 21) & 0x3ff) << 1;
    Imm |= ((Instruction >> 20) & 1) << 11;
    Imm |= ((Instruction >> 12) & 0xff) << 12;
    if (Imm & 0x100000)
      Imm |= ~0x1fffffu;
    Kind = DirectPCRelativeKind::Jump;
    Immediate = static_cast<int32_t>(Imm);
    return true;
  }
  return false;
}

static bool isUnsupportedPCRelative(uint64_t Instruction, unsigned Size) {
  // AUIPC encodes a PC-relative value rather than a branch displacement. If
  // it has no relocation and moves after the deletion, its immediate cannot
  // be repaired by the direct-branch fixup below.
  return Size == 4 && (Instruction & 0x7f) == 0x17;
}

static bool encodeDirectPCRelative(DirectPCRelativeKind Kind, int64_t Immediate,
                                   uint64_t &Instruction) {
  switch (Kind) {
  case DirectPCRelativeKind::Branch:
    if (!llvm::isInt<13>(Immediate) || (Immediate & 1))
      return false;
    Instruction =
        (Instruction & ~0xfe000f80u) |
        static_cast<uint32_t>(encodeSB(static_cast<int32_t>(Immediate)));
    return true;
  case DirectPCRelativeKind::Jump:
    if (!llvm::isInt<21>(Immediate) || (Immediate & 1))
      return false;
    Instruction =
        (Instruction & ~0xfffff000u) |
        static_cast<uint32_t>(encodeUJ(static_cast<int32_t>(Immediate)));
    return true;
  case DirectPCRelativeKind::QC_EBranch: {
    if (!llvm::isInt<13>(Immediate) || (Immediate & 1))
      return false;
    // QC_E branch offsets are stored in halfword units, with bit zero
    // implicit. The field layout below is therefore the inverse of the
    // decoding above after converting from a byte displacement.
    uint32_t Imm = static_cast<uint32_t>(Immediate >> 1);
    constexpr uint64_t OffsetMask = (uint64_t{1} << 31) |
                                    (uint64_t{0x3f} << 25) |
                                    (uint64_t{0xf} << 8) | (uint64_t{1} << 7);
    Instruction &= ~OffsetMask;
    Instruction |= (static_cast<uint64_t>(Imm >> 11) & 1) << 31;
    Instruction |= (static_cast<uint64_t>(Imm >> 4) & 0x3f) << 25;
    Instruction |= (static_cast<uint64_t>(Imm) & 0xf) << 8;
    Instruction |= (static_cast<uint64_t>(Imm >> 10) & 1) << 7;
    return true;
  }
  case DirectPCRelativeKind::RVCBranch:
    if (!llvm::isInt<9>(Immediate) || (Immediate & 1))
      return false;
    Instruction =
        (Instruction & ~0x1c7cu) |
        static_cast<uint32_t>(encodeCB(static_cast<int16_t>(Immediate)));
    return true;
  case DirectPCRelativeKind::RVCJump:
    if (!llvm::isInt<12>(Immediate) || (Immediate & 1))
      return false;
    Instruction =
        (Instruction & ~0x1ffcu) |
        static_cast<uint32_t>(encodeCJ(static_cast<int16_t>(Immediate)));
    return true;
  }
  llvm_unreachable("unknown direct PC-relative instruction");
}

static bool fixupPCRelativeInstructionsImpl(RegionFragmentEx &Region,
                                            uint32_t DeleteOffset,
                                            uint32_t DeleteSize,
                                            uint32_t PreservedPrefix) {
  if (!DeleteSize)
    return true;

  const uint32_t Size = static_cast<uint32_t>(Region.size());
  if (DeleteOffset > Size || DeleteSize > Size - DeleteOffset ||
      PreservedPrefix > DeleteOffset)
    return false;

  const uint32_t DeleteEnd = DeleteOffset + DeleteSize;
  const uint32_t ProtectedBegin = DeleteOffset - PreservedPrefix;
  llvm::StringRef Data = Region.getRegion();
  std::vector<DirectPCRelativePatch> Patches;
  bool Complete = true;

  for (uint32_t Offset = 0; Offset < Size;) {
    if (Offset >= ProtectedBegin && Offset < DeleteEnd) {
      Offset = DeleteEnd;
      continue;
    }

    unsigned Length = getInstructionLength(Data.substr(Offset));
    if (!Length || Offset + Length > Size) {
      Complete = false;
      break;
    }
    if (Offset < ProtectedBegin && Offset + Length > ProtectedBegin) {
      Complete = false;
      break;
    }

    // Keep relocated direct branches in this pass. Prefer their same-region
    // symbol target below; the instruction immediate is only a fallback for
    // relocations whose target cannot be resolved locally. The later
    // relocation application writes the final symbol-based value.

    uint64_t Instruction = readInstruction(Data, Offset, Length);

    const bool SourceMoves = Offset >= DeleteEnd;
    if (isUnsupportedPCRelative(Instruction, Length)) {
      // A relocated AUIPC is fixed by the normal relocation pass after the
      // source offset has been compacted. An unrelocated AUIPC cannot be
      // repaired here, but it must not prevent unrelated direct branches
      // from being patched below.
      if (SourceMoves &&
          !getRelocationAt(*Region.getOwningSection(), &Region, Offset, Length))
        Complete = false;
      Offset += Length;
      continue;
    }

    DirectPCRelativeKind Kind;
    int64_t Immediate = 0;
    if (!decodeDirectPCRelative(Instruction, Length, Kind, Immediate)) {
      Offset += Length;
      continue;
    }

    int64_t Target = static_cast<int64_t>(Offset) + Immediate;
    if (const Relocation *Reloc = getRelocationAt(*Region.getOwningSection(),
                                                  &Region, Offset, Length)) {
      ResolveInfo *Sym = Reloc->symInfo();
      FragmentRef *TargetRef = Reloc->targetFragRef();
      if (!TargetRef && Sym && Sym->outSymbol())
        TargetRef = Sym->outSymbol()->fragRef();
      if (TargetRef && TargetRef->frag() == &Region) {
        Target = static_cast<int64_t>(TargetRef->offset()) +
                 static_cast<int64_t>(Reloc->addend());
      }
    }
    if (Target >= DeleteOffset && Target < DeleteEnd) {
      Complete = false;
      Offset += Length;
      continue;
    }
    if (Target < 0 || Target >= Size) {
      if (SourceMoves)
        Complete = false;
      Offset += Length;
      continue;
    }

    const uint32_t NewOffset = SourceMoves ? Offset - DeleteSize : Offset;
    const int64_t NewTarget =
        Target >= DeleteEnd ? Target - DeleteSize : Target;
    uint64_t NewInstruction = Instruction;
    if (!encodeDirectPCRelative(Kind, NewTarget - NewOffset, NewInstruction)) {
      Complete = false;
      Offset += Length;
      continue;
    }
    if (NewInstruction != Instruction)
      Patches.push_back({Offset, NewInstruction, Length});
    Offset += Length;
  }

  for (const DirectPCRelativePatch &Patch : Patches) {
    uint8_t Bytes[8] = {};
    writeInstruction(Bytes, Patch.Instruction, Patch.Size);
    Region.replaceInstruction(Patch.Offset, nullptr, Bytes, Patch.Size);
  }
  return Complete;
}

// Apply the same repair after deferred relaxation has selected its complete
// set of deletions. Working from the original buffer is important: offsets
// and branch targets must be translated through every pending edit together,
// rather than through whichever deletion happened to be considered first.
static bool fixupPCRelativeInstructionsImpl(RegionFragmentEx &Region) {
  llvm::ArrayRef<RelaxEdit> Edits = Region.pendingRelaxEdits();
  if (Edits.empty())
    return true;

  llvm::StringRef Data = Region.getRegion();
  const uint32_t Size = static_cast<uint32_t>(Data.size());
  struct OffsetRange {
    uint32_t Begin;
    uint32_t End;
  };
  std::vector<OffsetRange> Deleted;
  std::vector<OffsetRange> Skipped;
  std::vector<uint32_t> Boundaries;
  for (const RelaxEdit &E : Edits) {
    if (E.DeleteBytes) {
      Deleted.push_back({E.DeleteOffset, E.DeleteOffset + E.DeleteBytes});
      Skipped.push_back({E.DeleteOffset, E.DeleteOffset + E.DeleteBytes});
    }
    if (E.NewType && *E.NewType == eld::ELF::riscv::internal::R_RISCV_ILUT &&
        E.NewInstr && E.NewInstrSize)
      Skipped.push_back({E.OrigOffset, E.DeleteOffset + E.DeleteBytes});
  }

  auto sortAndMerge = [](std::vector<OffsetRange> &Ranges) {
    std::sort(Ranges.begin(), Ranges.end(),
              [](const OffsetRange &L, const OffsetRange &R) {
                return L.Begin < R.Begin;
              });
    std::vector<OffsetRange> Merged;
    for (const OffsetRange &R : Ranges) {
      if (R.Begin == R.End)
        continue;
      if (!Merged.empty() && R.Begin <= Merged.back().End) {
        Merged.back().End = std::max(Merged.back().End, R.End);
        continue;
      }
      Merged.push_back(R);
    }
    Ranges.swap(Merged);
  };
  sortAndMerge(Deleted);
  sortAndMerge(Skipped);
  std::sort(Boundaries.begin(), Boundaries.end());

  auto contains = [](const std::vector<OffsetRange> &Ranges, int64_t Offset) {
    if (Offset < 0 || Offset > UINT32_MAX)
      return false;
    auto It = std::upper_bound(
        Ranges.begin(), Ranges.end(), static_cast<uint32_t>(Offset),
        [](uint32_t Value, const OffsetRange &R) { return Value < R.Begin; });
    if (It == Ranges.begin())
      return false;
    --It;
    return static_cast<uint32_t>(Offset) < It->End;
  };

  // getRelocationAt() scans the complete section relocation list. Build a
  // per-region index once so the instruction walk below does not multiply
  // that scan by the number of instructions.
  std::vector<Relocation *> Relocations;
  for (Relocation *R : Region.getOwningSection()->getRelocations())
    if (R->targetRef()->frag() == &Region &&
        R->type() != llvm::ELF::R_RISCV_RELAX &&
        R->type() != llvm::ELF::R_RISCV_NONE)
      Relocations.push_back(R);
  std::sort(Relocations.begin(), Relocations.end(),
            [](const Relocation *L, const Relocation *R) {
              return L->targetRef()->offset() < R->targetRef()->offset();
            });
  auto relocationAt = [&](uint32_t Offset, unsigned Length) {
    auto It = std::lower_bound(Relocations.begin(), Relocations.end(), Offset,
                               [](const Relocation *R, uint32_t Value) {
                                 return R->targetRef()->offset() < Value;
                               });
    if (It != Relocations.end() &&
        (*It)->targetRef()->offset() < Offset + Length)
      return *It;
    return static_cast<Relocation *>(nullptr);
  };

  std::vector<DirectPCRelativePatch> Patches;
  bool Complete = true;
  size_t SkipIndex = 0;
  for (uint32_t Offset = 0; Offset < Size;) {
    while (SkipIndex < Skipped.size() && Skipped[SkipIndex].End <= Offset)
      ++SkipIndex;
    if (SkipIndex < Skipped.size() && Offset >= Skipped[SkipIndex].Begin &&
        Offset < Skipped[SkipIndex].End) {
      Offset = Skipped[SkipIndex].End;
      continue;
    }

    unsigned Length = getInstructionLength(Data.substr(Offset));
    if (!Length || Offset + Length > Size) {
      Complete = false;
      break;
    }
    auto NextBoundary =
        std::upper_bound(Boundaries.begin(), Boundaries.end(), Offset);
    if (NextBoundary != Boundaries.end() && Offset + Length > *NextBoundary) {
      Complete = false;
      break;
    }

    uint64_t Instruction = readInstruction(Data, Offset, Length);

    if (isUnsupportedPCRelative(Instruction, Length)) {
      bool SourceMoves = Region.mapOffset(Offset) != Offset;
      if (SourceMoves && !relocationAt(Offset, Length))
        Complete = false;
      Offset += Length;
      continue;
    }

    DirectPCRelativeKind Kind;
    int64_t Immediate = 0;
    if (!decodeDirectPCRelative(Instruction, Length, Kind, Immediate)) {
      Offset += Length;
      continue;
    }

    int64_t Target = static_cast<int64_t>(Offset) + Immediate;
    if (Relocation *Reloc = relocationAt(Offset, Length)) {
      ResolveInfo *Sym = Reloc->symInfo();
      FragmentRef *TargetRef = Reloc->targetFragRef();
      if (!TargetRef && Sym && Sym->outSymbol())
        TargetRef = Sym->outSymbol()->fragRef();
      if (TargetRef && TargetRef->frag() == &Region)
        Target = static_cast<int64_t>(TargetRef->offset()) + Reloc->addend();
    }

    if (contains(Deleted, Target)) {
      Complete = false;
      Offset += Length;
      continue;
    }
    int64_t NewTarget = Target;
    if (Target >= 0 && Target <= UINT32_MAX)
      NewTarget = Region.mapOffset(static_cast<uint32_t>(Target));
    uint32_t NewOffset = Region.mapOffset(Offset);
    uint64_t NewInstruction = Instruction;
    if (!encodeDirectPCRelative(Kind, NewTarget - NewOffset, NewInstruction)) {
      Complete = false;
      Offset += Length;
      continue;
    }
    if (NewInstruction != Instruction)
      Patches.push_back({Offset, NewInstruction, Length});
    Offset += Length;
  }

  for (const DirectPCRelativePatch &Patch : Patches) {
    uint8_t Bytes[8] = {};
    writeInstruction(Bytes, Patch.Instruction, Patch.Size);
    Region.replaceInstruction(Patch.Offset, nullptr, Bytes, Patch.Size);
  }
  return Complete;
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

bool RISCVILUTFragment::hasPendingILUTEdit(class RegionFragmentEx &Region) {
  for (const RelaxEdit &Edit : Region.pendingRelaxEdits())
    if (Edit.NewType &&
        *Edit.NewType == eld::ELF::riscv::internal::R_RISCV_ILUT)
      return true;
  return false;
}

bool RISCVILUTFragment::fixupPCRelativeInstructions(
    class RegionFragmentEx &Region, uint32_t DeleteOffset, uint32_t DeleteSize,
    uint32_t PreservedPrefix) {
  return fixupPCRelativeInstructionsImpl(Region, DeleteOffset, DeleteSize,
                                         PreservedPrefix);
}

bool RISCVILUTFragment::fixupPCRelativeInstructions(
    class RegionFragmentEx &Region) {
  // This entry point is used at commit time. Keep the branch repair strictly
  // tied to ILUT edits so an accidental caller cannot change ordinary or
  // RV64 relaxation output.
  if (!hasPendingILUTEdit(Region))
    return true;
  return fixupPCRelativeInstructionsImpl(Region);
}

RISCVILUTFragment::RISCVILUTFragment(RISCVLDBackend &B, ELFSection *O)
    : TargetFragment(TargetFragment::Kind::TargetSpecific, O, nullptr,
                     /*Align=*/64, /*Size=*/0),
      Backend(B) {}

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
    // A two-instruction ILUT payload has one architectural entry point: the
    // qc.cm.ilut instruction.  A branch into the second source instruction
    // cannot be redirected to that entry point without also executing the
    // first payload instruction.  Such an occurrence must therefore remain
    // in line.  This is common in compiler-generated epilogues where a jump
    // skips a value-initialization instruction and enters the following move.
    std::set<uint64_t> DirectBranchTargets;
    for (uint64_t Offset = 0; Offset < Data.size();) {
      unsigned Length = getInstructionLength(Data.substr(Offset));
      if (!Length || Offset + Length > Data.size())
        break;

      uint64_t Instruction = readInstruction(Data, Offset, Length);

      DirectPCRelativeKind Kind;
      int64_t Immediate = 0;
      if (decodeDirectPCRelative(Instruction, Length, Kind, Immediate)) {
        int64_t Target = static_cast<int64_t>(Offset) + Immediate;
        if (const Relocation *Reloc = getRelocationAt(
                *Region->getOwningSection(), F, Offset, Length)) {
          FragmentRef *TargetRef = Reloc->targetFragRef();
          if (!TargetRef && Reloc->symInfo() && Reloc->symInfo()->outSymbol())
            TargetRef = Reloc->symInfo()->outSymbol()->fragRef();
          if (TargetRef && TargetRef->frag() == Region)
            Target =
                static_cast<int64_t>(TargetRef->offset()) + Reloc->addend();
        }
        if (Target >= 0 && Target < static_cast<int64_t>(Data.size()))
          DirectBranchTargets.insert(static_cast<uint64_t>(Target));
      }
      Offset += Length;
    }

    std::vector<Instruction> Instructions;
    for (uint64_t Offset = 0; Offset < Data.size();) {
      unsigned Length = getInstructionLength(Data.substr(Offset));
      if (!Length || Offset + Length > Data.size())
        break;
      std::vector<uint8_t> Bytes(Length);
      std::memcpy(Bytes.data(), Data.data() + Offset, Length);
      bool Valid = !isUnsafeForILUT(
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
        if (P->SourceSize > I.Length) {
          const uint64_t InternalBegin = I.Offset + I.Length;
          const uint64_t InternalEnd = I.Offset + P->SourceSize;
          bool HasInternalTarget = false;
          for (uint64_t Target : DirectBranchTargets) {
            if (Target >= InternalBegin && Target < InternalEnd) {
              HasInternalTarget = true;
              break;
            }
          }
          if (HasInternalTarget)
            continue;
        }
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

  // The uses are collected from the original instruction stream. Grouping
  // them by region lets us build one RelaxPlan and perform one branch-fixup
  // and compaction per region instead of doing all three operations once per
  // use.
  std::map<eld::RegionFragmentEx *, std::vector<Use>> UsesByRegion;
  for (const auto &KV : Uses) {
    auto *Region = const_cast<eld::RegionFragmentEx *>(
        llvm::dyn_cast<eld::RegionFragmentEx>(KV.second.F));
    if (Region)
      UsesByRegion[Region].push_back(KV.second);
  }

  for (auto &RegionUses : UsesByRegion) {
    eld::RegionFragmentEx *Region = RegionUses.first;
    std::vector<Use> &UsesInRegion = RegionUses.second;
    std::vector<Relocation *> Relocations;
    for (Relocation *R : Region->getOwningSection()->getRelocations())
      if (R->targetRef()->frag() == Region)
        Relocations.push_back(R);
    llvm::sort(Relocations, [](const Relocation *A, const Relocation *B) {
      return A->targetRef()->offset() < B->targetRef()->offset();
    });

    std::vector<std::pair<uint32_t, uint32_t>> DeletedRanges;
    for (const Use &U : UsesInRegion) {
      uint16_t Ilut = static_cast<uint16_t>(0x2000 | (U.EntryIndex << 2));
      uint32_t DeleteOffset = U.Offset + 2;
      uint32_t DeleteSize = U.SourceSize - 2;
      if (DeleteSize)
        DeletedRanges.push_back({DeleteOffset, DeleteOffset + DeleteSize});

      Relocation *IlutReloc = nullptr;
      auto It = llvm::lower_bound(Relocations, U.Offset,
                                  [](const Relocation *R, uint64_t Offset) {
                                    return R->targetRef()->offset() < Offset;
                                  });
      for (;
           It != Relocations.end() && It[0]->targetRef()->offset() == U.Offset;
           ++It) {
        if (It[0]->type() == llvm::ELF::R_RISCV_RELAX) {
          IlutReloc = It[0];
          break;
        }
      }
      // Most ordinary instructions have no R_RISCV_RELAX relocation. RelaxPlan
      // uses a relocation as a stable edit key, so create a private,
      // non-emitted key for those ILUT-only edits.
      if (!IlutReloc)
        IlutReloc = Relocation::Create(llvm::ELF::R_RISCV_RELAX, 16,
                                       FragmentRef::null());
      Region->recordDelete(IlutReloc, DeleteOffset, DeleteSize);
      Region->attachDecision(IlutReloc, eld::ELF::riscv::internal::R_RISCV_ILUT,
                             Ilut, U.Offset, 2);
    }

    llvm::sort(DeletedRanges);
    for (Relocation *R : Relocations) {
      uint32_t Offset = R->targetRef()->offset();
      auto It = llvm::upper_bound(
          DeletedRanges, Offset,
          [](uint32_t Value, const std::pair<uint32_t, uint32_t> &Range) {
            return Value < Range.first;
          });
      if (It != DeletedRanges.begin()) {
        --It;
        if (Offset >= It->first && Offset < It->second)
          R->setType(llvm::ELF::R_RISCV_NONE);
      }
    }

    // Keep ILUT edits deferred until postRelax(). Ordinary RISC-V
    // relaxations are selected after the ILUT pass and may delete additional
    // bytes in this region. postRelax() repairs direct branches against the
    // complete combined edit set before compacting the buffer.
    Backend.notePendingRelaxation(*Region);
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
