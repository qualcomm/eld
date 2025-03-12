//===- AArch64ErrataIslandFactory.h----------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef AARCH64_ERRATA_ISLANDFACTORY_H
#define AARCH64_ERRATA_ISLANDFACTORY_H

#include "eld/BranchIsland/BranchIsland.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "llvm/Support/DataTypes.h"

namespace eld {

class BranchIsland;
class Fragment;
class Module;

/** \class AArch64ErrataIslandFactory
 *  \brief
 *
 */
class AArch64ErrataIslandFactory {
public:
  AArch64ErrataIslandFactory();

  ~AArch64ErrataIslandFactory();

  // Create a errata island.
  BranchIsland *createAArch64ErrataIsland(Fragment *frag, uint32_t pOffset,
                                          Stub *stub, eld::IRBuilder &pBuilder);

private:
  int64_t numAArch64ErrataIsland;
};

} // namespace eld

#endif
