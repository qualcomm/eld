//===- StubFactory.h-------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#ifndef ELD_BRANCHISLAND_STUBFACTORY_H
#define ELD_BRANCHISLAND_STUBFACTORY_H

#include "eld/Target/GNULDBackend.h"
#include "llvm/Support/DataTypes.h"
#include <mutex>
#include <vector>

namespace eld {

class Stub;
class Relocation;
class BranchIsland;
class BranchIslandFactory;
class IRBuilder;

/** \class StubFactory
 *  \brief the clone factory of Stub
 *
 */
class StubFactory {
public:
  typedef std::vector<Stub *> StubVector;
  StubFactory(Stub *TargetStub);
  StubFactory();
  ~StubFactory();

  /// create - create a stub if needed.
  std::pair<BranchIsland *, bool> create(Relocation &PReloc,
                                         eld::IRBuilder &PBuilder,
                                         BranchIslandFactory &PBrIslandFactory,
                                         GNULDBackend &PBackend);

  void registerStub(Stub *PStub);

  StubVector &getAllStubs() { return Stubs; }

private:
  std::vector<Stub *> Stubs;
  std::mutex Mutex;
};

} // namespace eld

#endif
