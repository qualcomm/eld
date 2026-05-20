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

struct EXIDXPiece {
  uint32_t InputOffset = UINT32_MAX;
  uint32_t Size = 0;
};

class EXIDXFragment : public RegionFragment {
public:
  EXIDXFragment(llvm::StringRef Region, ELFSection *O, uint32_t Align = 1)
      : RegionFragment(Region, O, Fragment::Type::Region, Align) {}

  ~EXIDXFragment() override = default;

  void addPiece(EXIDXPiece P) { Pieces.push_back(P); }

  llvm::SmallVectorImpl<EXIDXPiece> &getPieces() { return Pieces; }
  const llvm::SmallVectorImpl<EXIDXPiece> &getPieces() const { return Pieces; }

  EXIDXPiece getPiece(uint32_t Offset) const;
  uint32_t translateInputOffset(uint32_t InputOffset) const;
  size_t size() const override;
  void dump(llvm::raw_ostream &OS) override;
  eld::Expected<void> emit(MemoryRegion &Mr, Module &M) override;

private:
  llvm::SmallVector<EXIDXPiece, 0> Pieces;
};

} // namespace eld

#endif
