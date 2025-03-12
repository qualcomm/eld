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
  StubFactory(Stub *targetStub);
  StubFactory();
  ~StubFactory();

  /// create - create a stub if needed.
  std::pair<BranchIsland *, bool> create(Relocation &pReloc,
                                         eld::IRBuilder &pBuilder,
                                         BranchIslandFactory &pBRIslandFactory,
                                         GNULDBackend &pBackend);

  void registerStub(Stub *pStub);

  StubVector &getAllStubs() { return m_Stubs; }

private:
  std::vector<Stub *> m_Stubs;
  std::mutex Mutex;
};

} // namespace eld

#endif
