//===- BranchIslandFactory.h-----------------------------------------------===//
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
#ifndef ELD_BRANCHISLAND_BRANCHISLANDFACTORY_H
#define ELD_BRANCHISLAND_BRANCHISLANDFACTORY_H

#include "eld/BranchIsland/BranchIsland.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include <mutex>

namespace eld {

class LinkerConfig;
class Fragment;
class Module;
class LDSymbol;
class FragmentRef;

/** \class BranchIslandFactory
 *  \brief
 *
 */
class BranchIslandFactory {
public:
  BranchIslandFactory(bool useAddends, LinkerConfig &config);

  ~BranchIslandFactory();

  // Create a branch island.
  std::pair<BranchIsland *, bool>
  createBranchIsland(Relocation &pReloc, Stub *stub, eld::IRBuilder &pBuilder,
                     const Relocator *pRelocator);

private:
  BranchIsland *findBranchIsland(Module &pModule, Relocation &pReloc,
                                 Stub *stub, int64_t addend);

  LDSymbol *createSymbol(Module &pModule, InputFile *pInput,
                         const std::string &Name, ResolveInfo::SizeType pSize,
                         LDSymbol::ValueType pValue, FragmentRef *pFragmentRef);

  int64_t numBranchIsland;
  int64_t numClone;
  bool _useAddends;
  std::mutex Mutex;
  LinkerConfig &m_Config;
};

} // namespace eld

#endif
