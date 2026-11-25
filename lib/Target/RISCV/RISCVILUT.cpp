//===- RISCVILUT.cpp------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//
// Builds and emits the RISC-V instruction lookup table (ILUT). Candidate
// discovery scans code regions for repeated, safe instruction sequences and
// excludes relocated bytes, existing ILUT instructions, and sequences whose
// deleted bytes are branch targets. Profitable sequences become fixed-size
// table entries, and each selected use is replaced by a qc.cm.ilut reference.
//
// ILUT replacements are queued as deferred relaxation edits so they can be
// combined with ordinary RISC-V relaxations. Before each edited region is
// compacted, surviving direct PC-relative branches are repaired against the
// combined edits. The finalized, 64-byte-aligned table is then emitted with
// its entries placed in fixed-size slots.
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
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/MathExtras.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <map>
#include <numeric>
#include <set>

using namespace eld;

namespace {

struct Instruction {
  // An ILUT-eligible instruction from fragment F. Bytes holds its original
  // little-endian encoding inline, so decoded instructions need no separate
  // payload allocation.
  const Fragment *F = nullptr;
  uint64_t Offset = 0;
  unsigned Length = 0;
  std::array<uint8_t, 6> Bytes{};
};

struct Pattern {
  // The key includes instruction lengths and bytes. This distinguishes
  // sequences with different instruction boundaries but identical byte runs.
  std::string Key;
  std::vector<uint8_t> Bytes;
  // EntrySize is the table cost. SourceSize is the source span replaced at
  // each occurrence. Each use retains a 2-byte qc.cm.ilut, so Saving is the
  // source size minus two bytes.
  unsigned EntrySize = 0;
  unsigned SourceSize = 0;
  unsigned Saving = 0;
  std::vector<Instruction> Occurrences;
};

// Examine the instruction prefix and return its encoded length. The ILUT
// scanner supports 16, 32, and 48-bit instructions. Unsupported longer
// encodings stop the scan rather than being treated as 32-bit instructions.
static unsigned getInstructionLength(llvm::StringRef Bytes) {
  if (Bytes.size() < 2)
    return 0;
  uint16_t Half = llvm::support::endian::read16le(Bytes.data());
  if ((Half & 3) != 3)
    return 2;
  if (Bytes.size() < 4)
    return 0;
  uint32_t Word = llvm::support::endian::read32le(Bytes.data());
  if ((Word & 0x1f) != 0x1f)
    return 4;
  // RISC-V 48-bit encodings have bits[5:0] equal to 0b011111.
  if ((Word & 0x3f) == 0x1f)
    return 6;
  return 0;
}

// Reject instructions whose meaning would change when executed from the table
// instead of their original address. This includes compressed control
// transfers, standard branches and jumps, auipc, Xqci branch forms, and
// unsupported instruction lengths.
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
      // encodings in these funct4 groups are c.mv, c.add, and c.ebreak.
      // Only the jump forms reference PC.
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

  if (Length == 6 && (Word & 0x3f) == 0x1f) {
    // Xqci 48-bit conditional branches use bits 14:12 as funct3 set to 4.
    unsigned Funct3 = (Word >> 12) & 7;
    return Funct3 == 4;
  }
  return true;
}

static bool isIlutInstruction(uint16_t Half) {
  // Existing qc.cm.ilut instructions must not be copied into the table.
  return (Half & 0xe003) == 0x2000;
}

// Find a non-marker relocation overlapping the instruction. Relocated bytes
// can be placeholders or need an address-specific fixup makin them unsafe
// table payloads. Ignore RELAX, NONE, and vendor marker records.
static const Relocation *getRelocationAt(ELFSection &Sec, const Fragment *F,
                                         uint64_t Offset, unsigned Size) {
  for (const Relocation *R : Sec.getRelocations()) {
    if (R->type() == llvm::ELF::R_RISCV_RELAX ||
        R->type() == llvm::ELF::R_RISCV_NONE ||
        R->type() == llvm::ELF::R_RISCV_VENDOR)
      continue;
    if (R->targetRef()->frag() != F)
      continue;
    uint64_t RelocOffset = R->targetRef()->offset();
    if (RelocOffset >= Offset && RelocOffset < Offset + Size)
      return R;
  }
  return nullptr;
}

// Search the prebuilt fragment and offset index for a non-marker relocation
// overlapping the instruction. The half-open range keeps relocations on an
// adjacent instruction from being treated as overlapping. Ignore RELAX, NONE,
// and vendor marker records.
static const Relocation *
getRelocationAt(const RISCVILUTFragment::RelocationIndex &Relocations,
                const Fragment *F, uint64_t Offset, unsigned Size) {
  auto FragmentIt = Relocations.find(F);
  if (FragmentIt == Relocations.end())
    return nullptr;

  const auto &ByOffset = FragmentIt->second;
  for (auto It = ByOffset.lower_bound(Offset);
       It != ByOffset.end() && It->first < Offset + Size; ++It) {
    for (const Relocation *R : It->second) {
      if (R->type() == llvm::ELF::R_RISCV_RELAX ||
          R->type() == llvm::ELF::R_RISCV_NONE ||
          R->type() == llvm::ELF::R_RISCV_VENDOR)
        continue;
      return R;
    }
  }
  return nullptr;
}

// Convenience wrapper for candidate scanning when only relocation presence
// matters.
static bool
hasRelocationAt(const RISCVILUTFragment::RelocationIndex &Relocations,
                const Fragment *F, uint64_t Offset, unsigned Size) {
  return getRelocationAt(Relocations, F, Offset, Size) != nullptr;
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

// Decode direct PC-relative branches and jumps into a signed byte displacement
// from the instruction address. Supported forms include compressed branches
// and jumps, standard 32-bit branches and jal, Xqcibi 32-bit branches, and the
// 48-bit QC_E branch. Reassemble and sign-extend each format immediate.
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
  // auipc encodes a PC-relative value, not a direct branch displacement, so
  // this code cannot re-encode it. If it moves, the normal relocation pass can
  // repair it only when a relocation describes the value.
  return Size == 4 && (Instruction & 0x7f) == 0x17;
}

// Inverse of decodeDirectPCRelative(). Require an aligned displacement that
// fits the instruction range, then replace only its immediate bits. Return
// failure instead of truncating an out-of-range branch.
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

// Repair direct branches for one deletion in the current uncompacted buffer.
// Skip the preserved replacement prefix and the bytes being deleted. Map
// surviving sources and targets across this edit, then apply collected patches
// after the scan so instruction offsets remain those of the original buffer.
// Report incomplete repair when a target is deleted or the new displacement
// cannot be represented by the branch encoding.
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

    // Keep relocated direct branches in this pass. Prefer a resolved
    // same-region relocation target below. Otherwise use the encoded immediate
    // as a fallback. Later relocation application writes the final symbol
    // value.

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

// Repair direct branches after deferred relaxation has selected all edits for
// this region. Merge deleted ranges and ranges that must not be decoded as the
// original instruction stream, then map surviving sources and targets through
// Region.mapOffset. Handling edits together avoids results that depend on edit
// order. Report incomplete repairs when a target is deleted or a new
// displacement cannot be encoded.
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
  // Boundaries is currently not populated here, so the crossing check below
  // does not reject any instruction boundary crossings.
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

// Accept one 32 or 48-bit instruction, or a supported pair of 16, 32, and
// 48-bit instructions. Assign each payload a fixed 4 or 8-byte table slot.
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

// Include instruction lengths as well as payload bytes in the pattern key.
// Equal byte runs with different instruction boundaries remain distinct.
static std::string makeKey(llvm::ArrayRef<uint8_t> Bytes,
                           const std::vector<unsigned> &Lengths) {
  std::string Key;
  for (unsigned Length : Lengths)
    Key.push_back(static_cast<char>(Length));
  Key.append(reinterpret_cast<const char *>(Bytes.data()), Bytes.size());
  return Key;
}

} // namespace

// Check whether deferred edits for this region include an ILUT replacement.
bool RISCVILUTFragment::hasPendingILUTEdit(class RegionFragmentEx &Region) {
  for (const RelaxEdit &Edit : Region.pendingRelaxEdits())
    if (Edit.NewType &&
        *Edit.NewType == eld::ELF::riscv::internal::R_RISCV_ILUT)
      return true;
  return false;
}

// Expose single-deletion repair for callers that know the deletion range and
// the replacement prefix to preserve.
bool RISCVILUTFragment::fixupPCRelativeInstructions(
    class RegionFragmentEx &Region, uint32_t DeleteOffset, uint32_t DeleteSize,
    uint32_t PreservedPrefix) {
  return fixupPCRelativeInstructionsImpl(Region, DeleteOffset, DeleteSize,
                                         PreservedPrefix);
}

bool RISCVILUTFragment::fixupPCRelativeInstructions(
    class RegionFragmentEx &Region) {
  // Deferred commit uses this overload. Repair only regions with a pending
  // ILUT replacement so ordinary and RV64 relaxation output is not changed.
  if (!hasPendingILUTEdit(Region))
    return true;
  return fixupPCRelativeInstructionsImpl(Region);
}

// The target-specific table fragment is 64-byte aligned and retains a
// reference to the backend for registering deferred edits during relaxation.
RISCVILUTFragment::RISCVILUTFragment(RISCVLDBackend &B, ELFSection *O)
    : TargetFragment(TargetFragment::Kind::TargetSpecific, O, nullptr,
                     /*Align=*/64, /*Size=*/0),
      Backend(B) {}

// Return the table size calculated by finalizeContents().
size_t RISCVILUTFragment::size() const { return ThisSize; }

// Emit nothing for an empty table. Otherwise clear the reserved table bytes
// at the assigned output offset before serializing the entries.
eld::Expected<void> RISCVILUTFragment::emit(MemoryRegion &Mr, Module &M) {
  if (size() == 0)
    return {};
  uint8_t *Buf = Mr.begin() + getOffset(M.getConfig().getDiagEngine());
  std::memset(Buf, 0, size());
  writeTo(Buf);
  return {};
}

// Record direct branch destinations before candidate selection. Prefer a
// resolved relocation target and addend. If no target is available, decode the
// instruction immediate. scanCodeSection uses these destinations to protect
// bytes that a replacement would delete.
void RISCVILUTFragment::collectBranchTargets(
    ELFSection &Sec, const RelocationIndex &Relocations) {
  if (!Sec.isCode())
    return;

  for (Fragment *F : Sec.getFragmentList()) {
    auto *Region = llvm::dyn_cast<eld::RegionFragmentEx>(F);
    if (!Region)
      continue;
    llvm::StringRef Data = Region->getRegion();
    for (uint64_t Offset = 0; Offset < Data.size();) {
      unsigned Length = getInstructionLength(Data.substr(Offset));
      if (!Length || Offset + Length > Data.size())
        break;

      uint64_t Instruction = readInstruction(Data, Offset, Length);
      DirectPCRelativeKind Kind;
      int64_t Immediate = 0;
      if (decodeDirectPCRelative(Instruction, Length, Kind, Immediate)) {
        FragmentRef *TargetRef = nullptr;
        const Relocation *TargetReloc = nullptr;
        auto FragmentIt = Relocations.find(F);
        if (FragmentIt != Relocations.end()) {
          const auto &ByOffset = FragmentIt->second;
          for (auto It = ByOffset.lower_bound(Offset);
               It != ByOffset.end() && It->first < Offset + Length; ++It) {
            for (const Relocation *R : It->second) {
              if (R->type() == llvm::ELF::R_RISCV_RELAX ||
                  R->type() == llvm::ELF::R_RISCV_NONE ||
                  R->type() == llvm::ELF::R_RISCV_VENDOR)
                continue;
              FragmentRef *Candidate = R->targetFragRef();
              if (!Candidate && R->symInfo() && R->symInfo()->outSymbol())
                Candidate = R->symInfo()->outSymbol()->fragRef();
              if (!Candidate)
                continue;
              TargetRef = Candidate;
              TargetReloc = R;
              break;
            }
            if (TargetRef)
              break;
          }
        }

        if (TargetRef) {
          int64_t TargetOffset =
              static_cast<int64_t>(TargetRef->offset()) + TargetReloc->addend();
          if (TargetOffset >= 0)
            BranchTargets[TargetRef->frag()].insert(TargetOffset);
        } else {
          int64_t Target = static_cast<int64_t>(Offset) + Immediate;
          if (Target >= 0 && Target < static_cast<int64_t>(Data.size()))
            BranchTargets[Region].insert(static_cast<uint64_t>(Target));
        }
      }
      Offset += Length;
    }
  }
}

// Discover candidates across all code regions before checking profitability:
// an ILUT entry can be shared by uses in different sections. Keep supported,
// safe, unrelocated instructions and form legal single-instruction patterns
// or physically adjacent instruction pairs. Rank patterns by estimated net
// savings with a deterministic key tie-break. Filter targets inside deleted
// bytes and overlapping source spans, then recheck global profitability.
void RISCVILUTFragment::scanCodeSections(
    const std::vector<ELFSection *> &Sections,
    const RelocationIndex &Relocations) {
  std::map<std::string, Pattern> Patterns;

  for (ELFSection *Sec : Sections) {
    for (Fragment *F : Sec->getFragmentList()) {
      auto *Region = llvm::dyn_cast<eld::RegionFragmentEx>(F);
      if (!Region)
        continue;
      llvm::StringRef Data = Region->getRegion();

      // R_RISCV_ALIGN reserves NOP bytes that the later alignment pass may
      // shrink. Keep the entire reserved range out of ILUT candidates so
      // earlier ILUT deletions cannot move the aligned target off its boundary.
      std::vector<std::pair<uint64_t, uint64_t>> AlignRanges;
      auto FragmentIt = Relocations.find(F);
      if (FragmentIt != Relocations.end()) {
        for (const auto &RelocsAtOffset : FragmentIt->second) {
          const uint64_t Begin = RelocsAtOffset.first;
          for (const Relocation *R : RelocsAtOffset.second) {
            if (R->type() != llvm::ELF::R_RISCV_ALIGN || !R->addend() ||
                Begin >= Data.size())
              continue;
            const uint64_t ReservedBytes = std::min<uint64_t>(
                static_cast<uint64_t>(R->addend()), Data.size() - Begin);
            AlignRanges.emplace_back(Begin, Begin + ReservedBytes);
          }
        }
      }
      llvm::sort(AlignRanges);

      std::vector<Instruction> Instructions;
      size_t AlignIndex = 0;
      for (uint64_t Offset = 0; Offset < Data.size();) {
        while (AlignIndex < AlignRanges.size() &&
               AlignRanges[AlignIndex].second <= Offset)
          ++AlignIndex;
        if (AlignIndex < AlignRanges.size() &&
            Offset >= AlignRanges[AlignIndex].first &&
            Offset < AlignRanges[AlignIndex].second) {
          Offset = AlignRanges[AlignIndex].second;
          continue;
        }

        unsigned Length = getInstructionLength(Data.substr(Offset));
        if (!Length || Offset + Length > Data.size())
          break;
        llvm::StringRef InstructionBytes = Data.substr(Offset, Length);
        bool Valid = !isUnsafeForILUT(InstructionBytes, Length);
        if (Length == 2 && isIlutInstruction(llvm::support::endian::read16le(
                               InstructionBytes.data())))
          Valid = false;
        if (Length != 2 && Length != 4 && Length != 6)
          Valid = false;
        // Relocated bytes may be placeholders or need address-dependent
        // fixups. Copying them would lose or misapply the relocation.
        if (hasRelocationAt(Relocations, F, Offset, Length))
          Valid = false;
        if (Valid) {
          Instruction Decoded;
          Decoded.F = F;
          Decoded.Offset = Offset;
          Decoded.Length = Length;
          std::memcpy(Decoded.Bytes.data(), InstructionBytes.data(), Length);
          Instructions.push_back(Decoded);
        }
        Offset += Length;
      }

      for (size_t I = 0; I < Instructions.size(); ++I) {
        const Instruction &A = Instructions[I];
        std::vector<unsigned> Lengths = {A.Length};
        unsigned EntrySize = 0;
        if (isLegalPayload(Lengths, EntrySize)) {
          llvm::ArrayRef<uint8_t> ABytes(A.Bytes.data(), A.Length);
          Pattern &P = Patterns[makeKey(ABytes, Lengths)];
          P.Key = makeKey(ABytes, Lengths);
          P.Bytes.assign(ABytes.begin(), ABytes.end());
          P.EntrySize = EntrySize;
          P.SourceSize = A.Length;
          P.Saving = A.Length - 2;
          P.Occurrences.push_back(A);
        }

        if (I + 1 >= Instructions.size())
          continue;
        const Instruction &B = Instructions[I + 1];
        // A pair must be physically adjacent in the same fragment. Skipped or
        // unsafe instructions therefore break a candidate sequence.
        if (A.F != B.F || A.Offset + A.Length != B.Offset)
          continue;
        Lengths = {A.Length, B.Length};
        if (!isLegalPayload(Lengths, EntrySize))
          continue;
        std::vector<uint8_t> Bytes(A.Bytes.begin(), A.Bytes.begin() + A.Length);
        Bytes.insert(Bytes.end(), B.Bytes.begin(), B.Bytes.begin() + B.Length);
        Pattern &P = Patterns[makeKey(Bytes, Lengths)];
        P.Key = makeKey(Bytes, Lengths);
        P.Bytes = std::move(Bytes);
        P.EntrySize = EntrySize;
        P.SourceSize = A.Length + B.Length;
        P.Saving = P.SourceSize - 2;
        P.Occurrences.push_back(A);
      }
    }
  }

  llvm::SmallVector<Pattern *, 0> Ordered;
  // Discard patterns whose aggregate savings across all code regions do not
  // exceed the table slot cost. Check profitability again after filtering.
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
  // qc.cm.ilut has an 11-bit index, limiting the table to 2048 entries.
  for (Pattern *P : Ordered) {
    if (Entries.size() >= 2048)
      break;
    std::vector<Instruction> Selected;
    std::set<std::pair<const Fragment *, uint64_t>> CandidateOccupied;
    for (const Instruction &I : P->Occurrences) {
      if (P->SourceSize > I.Length) {
        // The replacement occupies the first two bytes. A branch into the
        // rest of this source span would target bytes that are deleted.
        const uint64_t InternalBegin = I.Offset + I.Length;
        const uint64_t InternalEnd = I.Offset + P->SourceSize;
        bool HasInternalTarget = false;
        auto Targets = BranchTargets.find(I.F);
        if (Targets != BranchTargets.end()) {
          auto Target = Targets->second.lower_bound(InternalBegin);
          HasInternalTarget =
              Target != Targets->second.end() && *Target < InternalEnd;
        }
        if (HasInternalTarget)
          continue;
      }
      // Track ownership by byte so selected patterns cannot delete
      // overlapping source ranges.
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
    // Reserve bytes only after safety and overlap filtering, and after a fresh
    // profitability check on the surviving occurrences.
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
    // Each use records its source fragment, starting offset, source span, and
    // table entry index.
    for (const Instruction &I : Selected)
      Uses.insert(
          {{I.F, I.Offset}, {I.F, I.Offset, P->SourceSize, EntryIndex}});
  }
}

void RISCVILUTFragment::finalizeContents() {
  // Put 8-byte entries first because qc.itdec represents their count as a
  // leading run. Remap every selected use to the reordered entry indices.
  std::vector<Entry> OldEntries = Entries;
  std::vector<unsigned> Order(Entries.size());
  std::iota(Order.begin(), Order.end(), 0);
  std::stable_sort(Order.begin(), Order.end(), [&](unsigned A, unsigned B) {
    return OldEntries[A].Size > OldEntries[B].Size;
  });

  std::vector<unsigned> OldToNew(Entries.size());
  for (unsigned New = 0; New < Order.size(); ++New) {
    Entries[New] = OldEntries[Order[New]];
    OldToNew[Order[New]] = New;
  }
  for (auto &KV : Uses)
    KV.second.EntryIndex = OldToNew[KV.second.EntryIndex];

  // DEC counts the leading 8-byte entries. The final table size is the sum
  // of their slot widths and those of the remaining entries.
  DEC = 0;
  while (DEC < Entries.size() && Entries[DEC].Size == 8)
    ++DEC;
  ThisSize = DEC * 8 + (Entries.size() - DEC) * 4;
}

// Look up a selected use by its exact fragment and starting offset. Return
// its reordered table index and the number of source bytes to replace.
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

// Convert selected uses into deferred region edits. This queues the work and
// does not compact the buffer here. Ordinary relaxations can join the same
// edit set before branch repair and final compaction.
bool RISCVILUTFragment::applyRelaxations() {
  if (Applied)
    return false;
  Applied = true;
  bool Changed = false;

  // Uses refer to the original instruction stream. Group them by region so
  // each region receives one combined edit set and one later branch repair and
  // compaction.
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
      // Retain the first halfword for qc.cm.ilut and queue deletion of the
      // remaining source bytes. Encode this use table index in the replacement.
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
      // Reuse an R_RISCV_RELAX relocation as the stable edit key when one is
      // present. Otherwise create a private, non-emitted key for this ILUT
      // edit.
      if (!IlutReloc)
        IlutReloc = Relocation::Create(llvm::ELF::R_RISCV_RELAX, 16,
                                       FragmentRef::null());
      Region->recordDelete(IlutReloc, DeleteOffset, DeleteSize);
      Region->attachDecision(IlutReloc, eld::ELF::riscv::internal::R_RISCV_ILUT,
                             Ilut, U.Offset, 2);
    }

    llvm::sort(DeletedRanges);
    for (Relocation *R : Relocations) {
      // A relocation placed in deleted bytes must not be applied to the
      // compacted buffer. Convert it to a NONE marker.
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

    // Keep ILUT edits deferred until postRelax(). Ordinary RISC-V relaxations
    // may add more deletions after the ILUT pass. postRelax() repairs direct
    // branches against the complete edit set before compacting the region.
    Backend.notePendingRelaxation(*Region);
    Changed = true;
  }
  return Changed;
}

// Serialize entries into fixed-size slots. Clear each slot, copy its payload,
// and fill the unused halfword of an 8-byte slot containing a 48-bit
// instruction with c.nop.
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

// Print no listing for an empty table. Otherwise report each payload and DEC.
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
