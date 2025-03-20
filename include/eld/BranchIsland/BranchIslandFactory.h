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
  BranchIslandFactory(bool UseAddends, LinkerConfig &Config);

  ~BranchIslandFactory();

  // Create a branch island.
  std::pair<BranchIsland *, bool>
  createBranchIsland(Relocation &PReloc, Stub *Stub, eld::IRBuilder &PBuilder,
                     const Relocator *PRelocator);

private:
  BranchIsland *findBranchIsland(Module &PModule, Relocation &PReloc,
                                 Stub *Stub, int64_t Addend);

  LDSymbol *createSymbol(Module &PModule, InputFile *PInput,
                         const std::string &Name, ResolveInfo::SizeType PSize,
                         LDSymbol::ValueType PValue, FragmentRef *PFragmentRef);

  int64_t NumBranchIsland;
  int64_t NumClone;
  bool UseAddends;
  std::mutex Mutex;
  LinkerConfig &Config;
};

} // namespace eld

#endif
