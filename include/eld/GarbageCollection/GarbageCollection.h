//===- GarbageCollection.h-------------------------------------------------===//
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

#ifndef ELD_GARBAGECOLLECTION_GARBAGECOLLECTION_H
#define ELD_GARBAGECOLLECTION_GARBAGECOLLECTION_H

#include "eld/Readers/ELFSection.h"
#include "llvm/ADT/DenseSet.h"
#include <queue>
#include <set>
#include <vector>

namespace eld {

class LDSymbol;
class LinkerConfig;
class Module;
class GNULDBackend;

/** \class GarbageCollection
 *  \brief Implementation of garbage collection for --gc-section.
 *  @ref GNU gold, gc.
 */
class GarbageCollection {
public:
  typedef llvm::DenseSet<Section *> SectionListTy;
  typedef llvm::DenseSet<LDSymbol *> SymbolListTy;
  typedef llvm::DenseSet<Section *> SectionSetTy;

  /** \class SectionReachedListMap
   *  \brief Map the section to the list of sections which it can reach directly
   */
  class SectionReachedListMap {
  public:
    SectionReachedListMap() {}

    /// addReference - add a reference from pFrom to pTo
    void addReference(Section &From, Section &To);

    /// getReachedList - get the list of sections which can be reached by
    /// pSection, create one if the list has not existed
    SectionListTy &getReachedList(Section &CurSection);

    // Like wise for symbols (commons)
    SymbolListTy &getReachedSymbolList(Section &CurSection);

    /// findReachedList - find the list of sections which can be reached by
    /// pSection, return nullptr if the list not exists
    SectionListTy *findReachedList(Section &CurSection);

    // Like wise for symbols (commons)
    SymbolListTy *findReachedSymbolList(Section &CurSection);

    // For bit code sections
    void findReachedBitCodeSectionsAndSymbols(Module &CurModule);

    void addToWorkQ(Section *W) { InputBitcodeSections.push(W); }

  private:
    typedef llvm::DenseMap<Section *, SectionListTy> ReachedSectionsTy;
    typedef llvm::DenseMap<Section *, SymbolListTy> ReachedSymbolsTy;

  private:
    /// m_ReachedSections - map a section to the reachable sections list
    ReachedSectionsTy ReachedSections;
    // Common symbols dot reside in any sections, hence we need to have
    // a reachibility map for them too
    ReachedSymbolsTy ReachedSymbols;
    // A queue of sections that needs to be postprocessed.
    std::queue<Section *> InputBitcodeSections;
  };

public:
  GarbageCollection(LinkerConfig &PConfig, const GNULDBackend &PBackend,
                    Module &CurModule);
  ~GarbageCollection();

  /// run - do garbage collection
  bool run(const std::string &Phase, bool CommonSectionsOnly = false);

  // is section part of garbage collection ?
  bool mayProcessGC(ELFSection &CurSection);

private:
  void setUpReachedSectionsAndSymbols();
  void findReferencedSectionsAndSymbols(SectionSetTy &PEntry,
                                        SectionSetTy &LiveSet);
  bool getEntrySections(SectionSetTy &PEntry);
  void stripSections(SectionSetTy &S, bool CommonSectionsOnly);
  bool treatSymbolAsEntry(ResolveInfo *R) const;

  // Add sections marked with SHF_GNU_RETAIN flag to entrySections.
  // Sections marked with SHF_GNU_RETAIN flag should be treated
  // as an entry section.
  void addRetainSections(SectionSetTy &EntrySections);

private:
  /// m_SectionReachedListMap - map the section to the list of sections which it
  /// can reach directly
  SectionReachedListMap MSectionReachedListMap;

  /// m_ReferencedSections - a list of sections which can be reached from entry
  SectionListTy MReferencedSections;

  LinkerConfig &ThisConfig;
  const GNULDBackend &MBackend;
  Module &ThisModule;
};

} // namespace eld

#endif
