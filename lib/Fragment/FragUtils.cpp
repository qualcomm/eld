//===- FragUtils.cpp-------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#include "eld/Fragment/FragUtils.h"
#include "eld/Fragment/RegionFragment.h"
#include "eld/Fragment/RegionFragmentEx.h"
#include "llvm/Support/Casting.h"

llvm::StringRef eld::getRegionFromFragment(const Fragment *frag) {
  assert(frag->getKind() == Fragment::Region ||
         frag->getKind() == Fragment::RegionFragmentEx);
  llvm::StringRef RegionStr;
  const RegionFragment *R = llvm::dyn_cast<const RegionFragment>(frag);
  if (!R) {
    const class RegionFragmentEx *RFEx =
        llvm::dyn_cast<const class RegionFragmentEx>(frag);
    RegionStr = RFEx->getRegion();
  } else {
    RegionStr = R->getRegion();
  }
  return RegionStr;
}
