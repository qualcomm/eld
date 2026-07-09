//===- ARMEXIDXFragment.h--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef TARGET_ARM_ARMEXIDXFRAGMENT_H
#define TARGET_ARM_ARMEXIDXFRAGMENT_H

#include "eld/Fragment/RegionFragment.h"
#include "llvm/ADT/SmallVector.h"
#include <cstdint>

namespace eld {

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

  /// Total live size: sum of all piece sizes (excludes GC'd entries).
  size_t size() const override;

  void dump(llvm::raw_ostream &OS) override;
  eld::Expected<void> emit(MemoryRegion &Mr, Module &M) override;

private:
  llvm::SmallVector<EXIDXPiece, 0> Pieces;
};

} // namespace eld

#endif
