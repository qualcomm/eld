//===- BranchIsland.h------------------------------------------------------===//
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
#ifndef ELD_BRANCHISLAND_BRANCHISLAND_H
#define ELD_BRANCHISLAND_BRANCHISLAND_H

#include "eld/Fragment/Stub.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/DataTypes.h"
#include <set>
#include <string>

namespace eld {

class Module;
class Stub;
class ELFSection;
class Relocation;

/** \class BranchIsland
 *  \brief BranchIsland is a collection of stubs
 *
 */
class BranchIsland {
public:
  typedef std::vector<Relocation *> RelocationListType;
  typedef RelocationListType::iterator reloc_iterator;

public:
  BranchIsland(Stub *Stub);

  ~BranchIsland();

  /// relocation iterators of the island
  reloc_iterator relocBegin() { return Relocations.begin(); }

  reloc_iterator relocEnd() { return Relocations.end(); }

  bool canReuseBranchIsland(ResolveInfo *PInfo, int64_t PAddend,
                            bool UseAddends, Stub *PS) {
    if (S && !S->isCompatible(PS))
      return false;
    // For local symbols, the filename should also match.
    if (PInfo->type() == ResolveInfo::Section ||
        PInfo->binding() == ResolveInfo::Local) {
      if (S->savedSymInfo() &&
          (PInfo->resolvedOrigin() != S->savedSymInfo()->resolvedOrigin()))
        return false;
    }
    if (!S->savedSymInfo() || S->savedSymInfo()->name() != PInfo->name())
      return false;
    if (!UseAddends)
      return true;
    if (PAddend == Addend)
      return true;
    return false;
  }

  /// addRelocation - add a relocation into island
  void addRelocation(Relocation &PReloc) { Relocations.push_back(&PReloc); }

  /// save relocation.
  bool saveTrampolineInfo(Relocation &R, int64_t PAddend) {
    Reloc = &R;
    Addend = PAddend;
    return true;
  }

  int64_t branchIslandAddr(Module &M);

  ResolveInfo *symInfo() const;

  Stub *stub() const { return S; }

  eld::Relocation *getOrigRelocation() const { return Reloc; }

  uint64_t getAddend() const { return Addend; }

  void addReuse(Relocation *R) { Reuse.insert(R); }

  const std::set<Relocation *> &getReuses() const { return Reuse; }

protected:
  Stub *S;
  eld::Relocation *Reloc;
  int64_t Addend;
  RelocationListType Relocations;
  std::set<Relocation *> Reuse;
};

} // namespace eld

#endif
