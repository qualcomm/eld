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
  BranchIsland(Stub *stub);

  ~BranchIsland();

  /// relocation iterators of the island
  reloc_iterator reloc_begin() { return m_Relocations.begin(); }

  reloc_iterator reloc_end() { return m_Relocations.end(); }

  bool canReuseBranchIsland(ResolveInfo *pInfo, int64_t addend, bool useAddends,
                            Stub *S) {
    if (m_Stub && !m_Stub->isCompatible(S))
      return false;
    // For local symbols, the filename should also match.
    if (pInfo->type() == ResolveInfo::Section ||
        pInfo->binding() == ResolveInfo::Local) {
      if (m_Stub->savedSymInfo() &&
          (pInfo->resolvedOrigin() != m_Stub->savedSymInfo()->resolvedOrigin()))
        return false;
    }
    if (!m_Stub->savedSymInfo() ||
        m_Stub->savedSymInfo()->name() != pInfo->name())
      return false;
    if (!useAddends)
      return true;
    if (addend == m_Addend)
      return true;
    return false;
  }

  /// addRelocation - add a relocation into island
  void addRelocation(Relocation &pReloc) { m_Relocations.push_back(&pReloc); }

  /// save relocation.
  bool saveTrampolineInfo(Relocation &R, int64_t addend) {
    m_Reloc = &R;
    m_Addend = addend;
    return true;
  }

  int64_t branchIslandAddr(Module &M);

  ResolveInfo *symInfo() const;

  Stub *stub() const { return m_Stub; }

  eld::Relocation *getOrigRelocation() const { return m_Reloc; }

  uint64_t getAddend() const { return m_Addend; }

  void addReuse(Relocation *R) { m_Reuse.insert(R); }

  const std::set<Relocation *> &getReuses() const { return m_Reuse; }

protected:
  Stub *m_Stub;
  eld::Relocation *m_Reloc;
  int64_t m_Addend;
  RelocationListType m_Relocations;
  std::set<Relocation *> m_Reuse;
};

} // namespace eld

#endif
