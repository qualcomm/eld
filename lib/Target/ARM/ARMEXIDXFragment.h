//===- ARMEXIDXFragment.h--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef TARGET_ARM_ARMEXIDXFRAGMENT_H
#define TARGET_ARM_ARMEXIDXFRAGMENT_H

#include "eld/Fragment/RegionFragment.h"
#include "eld/Fragment/TargetFragment.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include <cstdint>

namespace eld {

class Relocation;

/// One logical entry (8 bytes) within a .ARM.exidx input section.
struct EXIDXPiece {
  uint32_t InputOffset = UINT32_MAX;
  uint32_t Size = 0;
};

/// A Fragment that owns the raw bytes for one .ARM.exidx input section and
/// tracks which 8-byte entries are live via a list of EXIDXPiece records.
///
/// The new model keeps one EXIDXFragment per input section.  Sorting
/// reorders the Pieces vector inside each fragment; the fragment list in
/// the output section is then stable-sorted by each fragment's minimum
/// sort key.  This preserves ownership and makes GC annotation (per-piece
/// <GC> tags in the map) straightforward.
class EXIDXFragment : public RegionFragment {
public:
  EXIDXFragment(llvm::StringRef Region, ELFSection *O, uint32_t Align = 1)
      : RegionFragment(Region, O, Fragment::Type::Region, Align) {}

  ~EXIDXFragment() override = default;

  void addPiece(EXIDXPiece P) { Pieces.push_back(P); }

  llvm::SmallVectorImpl<EXIDXPiece> &getPieces() { return Pieces; }
  const llvm::SmallVectorImpl<EXIDXPiece> &getPieces() const { return Pieces; }

  EXIDXPiece getPiece(uint32_t Offset) const;

  /// Translate an input-layout relocation offset to the corresponding
  /// offset in piece-layout space (i.e. after GC / sorting).
  /// Called after sortEXIDX to fix up relocation target offsets.
  uint32_t translateInputOffset(uint32_t InputOffset) const;

  bool getUnwindWord(const EXIDXPiece &Piece, uint32_t &Word) const;

  void removePieces(llvm::ArrayRef<uint32_t> InputOffsets);

  uint32_t getRelocationInputOffset(Relocation *R);

  /// Total live size: sum of all piece sizes (excludes GC'd entries).
  size_t size() const override;

  void dump(llvm::raw_ostream &OS) override;
  eld::Expected<void> emit(MemoryRegion &Mr, Module &M) override;

private:
  llvm::SmallVector<EXIDXPiece, 0> Pieces;
  llvm::DenseMap<Relocation *, uint32_t> RelocationInputOffsets;
};

// An 8-byte linker-generated CANTUNWIND entry placed at the end of the
// .ARM.exidx table.  Its first word is a PREL31 offset to the byte
// immediately past the last covered function; its second word is 0x1
// (CANTUNWIND compact model entry).  Lives in its own internal section so
// it is naturally placed last by any *(.ARM.exidx*) linker script rule.
class EXIDXSentinelFragment : public TargetFragment {
public:
  EXIDXSentinelFragment(ELFSection *O)
      : TargetFragment(TargetFragment::TargetSpecific, O, nullptr, 4, 0) {}

  ~EXIDXSentinelFragment() override = default;

  const std::string name() const override { return "EXIDXSentinel"; }
  // Size is 0 until setActive(true) is called; this keeps the fragment
  // invisible when no real .ARM.exidx input sections exist.
  size_t size() const override { return Active ? 8 : 0; }

  // Called by sortEXIDX() once the sentinel decision is known.
  void setActive(bool IsActive) { Active = IsActive; }

  // Set the target address (byte past the last covered function).
  // Called by sortEXIDX() once all output addresses are known.
  void setTargetAddr(uint64_t Addr) { TargetAddr = Addr; }

  eld::Expected<void> emit(MemoryRegion &Mr, Module &M) override;
  void dump(llvm::raw_ostream &OS) override;

private:
  uint64_t TargetAddr = 0;
  bool Active = false;
};

} // namespace eld

#endif
