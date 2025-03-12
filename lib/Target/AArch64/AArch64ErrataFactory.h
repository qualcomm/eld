//===- AArch64ErrataFactory.h----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef AARCH64_ERRATAFACTORY_H
#define AARCH64_ERRATAFACTORY_H

#include "eld/BranchIsland/BranchIsland.h"
#include "llvm/Support/DataTypes.h"
#include <vector>

namespace eld {

class Stub;
class Relocation;
class BranchIsland;
class AArch64ErrataIslandFactory;
class IRBuilder;

/** \class AArch64ErrataFactory
 *  \brief the clone factory of Stub
 *
 */
class AArch64ErrataFactory {
public:
  AArch64ErrataFactory(Stub *targetStub);

  ~AArch64ErrataFactory();

  /// create - create a stub if needed.
  BranchIsland *create(Fragment *frag, uint32_t pOffset,
                       eld::IRBuilder &pBuilder,
                       AArch64ErrataIslandFactory &pBRIslandFactory);

  Stub *targetStub() const { return m_Stub; }

private:
  Stub *m_Stub;
};

} // namespace eld

#endif
