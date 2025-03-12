//===- OutputSectionEntry.h------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_OBJECT_OUTPUTSECTIONENTRY_H
#define ELD_OBJECT_OUTPUTSECTIONENTRY_H

#include "eld/Fragment/MergeStringFragment.h"
#include "eld/Object/SectionMap.h"
#include "eld/Script/Assignment.h"
#include "eld/Script/OutputSectDesc.h"
#include "eld/Target/LDFileFormat.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/DataTypes.h"
#include <atomic>
#include <chrono>
#include <string>
#include <vector>

namespace eld {

class BranchIsland;
class ELFSection;
class ELFSegment;
class OutputSectionEntry;
class RuleContainer;
class SectionMap;

class OutputSectionEntry {
public:
  typedef std::vector<RuleContainer *> InputList;
  typedef std::vector<BranchIsland *>::iterator BranchIslandsIter;
  typedef InputList::iterator iterator;
  typedef InputList::const_iterator const_iterator;
  typedef InputList::const_reference const_reference;
  typedef InputList::reference reference;

  typedef std::vector<Assignment *> SymbolAssignments;
  typedef SymbolAssignments::iterator sym_iterator;

  OutputSectionEntry(SectionMap *, std::string pName);
  OutputSectionEntry(SectionMap *, ELFSection *);
  OutputSectionEntry(SectionMap *, OutputSectDesc &pOutputDesc);
  OutputSectionEntry(SectionMap *parent, std::string pName,
                     LDFileFormat::Kind pKind, uint32_t pType, uint32_t pFlag,
                     uint32_t pAlign);

  llvm::StringRef name() const { return m_Name; }

  const OutputSectDesc::Prolog &prolog() const {
    return m_OutputSectDesc->prolog();
  }
  OutputSectDesc::Prolog &prolog() { return m_OutputSectDesc->prolog(); }

  const OutputSectDesc::Epilog &epilog() const {
    return m_OutputSectDesc->epilog();
  }
  OutputSectDesc::Epilog &epilog() { return m_OutputSectDesc->epilog(); }

  size_t order() const { return m_Order; }

  bool hasOrder() const { return (m_Order != UINT_MAX); }

  void setOrder(size_t pOrder) { m_Order = pOrder; }

  bool hasContent() const;

  const ELFSection *getSection() const { return m_pSection; }
  ELFSection *getSection() { return m_pSection; }

  void setSection(ELFSection *pSection) {
    m_pSection = pSection;
    computeHash();
  }
  void computeHash();
  iterator begin() { return m_InputList.begin(); }
  iterator end() { return m_InputList.end(); }

  const_iterator begin() const { return m_InputList.begin(); }
  const_iterator end() const { return m_InputList.end(); }

  const InputList &getRuleContainer() const { return m_InputList; }

  const_reference front() const { return m_InputList.front(); }
  reference front() { return m_InputList.front(); }
  const_reference back() const { return m_InputList.back(); }
  reference back() { return m_InputList.back(); }

  size_t size() const { return m_InputList.size(); }

  bool empty() const { return m_InputList.empty(); }

  bool isDiscard() const { return m_bIsDiscard; }

  void append(RuleContainer *pInput) { m_InputList.push_back(pInput); }

  sym_iterator sym_begin() { return m_SymbolAssignments.begin(); }
  sym_iterator sym_end() { return m_SymbolAssignments.end(); }

  const SymbolAssignments &symAssignments() const {
    return m_SymbolAssignments;
  }
  SymbolAssignments &symAssignments() { return m_SymbolAssignments; }

  const SymbolAssignments &sections_end_assignments() const {
    return m_SectionEndAssignments;
  }
  SymbolAssignments &sectionEndAssignments() { return m_SectionEndAssignments; }

  sym_iterator sectionendsym_begin() { return m_SectionEndAssignments.begin(); }
  sym_iterator sectionendsym_end() { return m_SectionEndAssignments.end(); }

  void moveSectionAssignments(OutputSectionEntry *out) {
    m_SectionEndAssignments = out->sectionEndAssignments();
    out->sectionEndAssignments().clear();
  }

  bool hasAssignments() const { return (m_SymbolAssignments.size() > 0); }

  // A section may be part of multiple segments, this only returns the
  // segment where the section would get loaded.
  void setLoadSegment(ELFSegment *E) { m_pLoadSegment = E; }

  ELFSegment *getLoadSegment() const { return m_pLoadSegment; }

  // Set the first fragment in the output section.
  void setFirstNonEmptyRule(RuleContainer *R) { FirstNonEmptyRule = R; }

  RuleContainer *getFirstNonEmptyRule() const { return FirstNonEmptyRule; }

  Fragment *getFirstFrag() const;

  RuleContainer *getLastRule() const { return m_LastRule; }

  void setLastRule(RuleContainer *R) { m_LastRule = R; }

  RuleContainer *createDefaultRule(Module &M);

  // ------------------Branch island support ------------------------
  BranchIslandsIter islands_begin() { return m_BranchIslands.begin(); }

  BranchIslandsIter islands_end() { return m_BranchIslands.end(); }

  void addBranchIsland(BranchIsland *b) { m_BranchIslands.push_back(b); }

  void addBranchIsland(ResolveInfo *pSym, BranchIsland *b) {
    BranchIslandForSymbol[pSym];
    BranchIslandForSymbol[pSym].push_back(b);
    m_BranchIslands.push_back(b);
  }

  uint32_t getNumBranchIslands() const { return m_BranchIslands.size(); }

  void dump(llvm::raw_ostream &outs) const;

  uint64_t getHash() {
    if (!m_Hash)
      computeHash();
    return m_Hash;
  }

  std::string getSectionTypeStr() const;

  // ----------------------Reuse trampolines optimization---------------
  std::vector<BranchIsland *>
  getBranchIslandsForSymbol(ResolveInfo *pSym) const {
    std::vector<BranchIsland *> Islands;
    auto Iter = BranchIslandForSymbol.find(pSym);
    if (Iter == BranchIslandForSymbol.end())
      return Islands;
    return Iter->second;
  }

  // -------------------- Add Linker script rules ------------------------
  RuleContainer *createRule(eld::Module &M, std::string Annotation,
                            InputFile *I);

  bool insertAfterRule(RuleContainer *I, RuleContainer *R);

  bool insertBeforeRule(RuleContainer *I, RuleContainer *R);

  // -------------------- String merging support -------------------------

  MergeableString *getMergedString(const MergeableString *S) const {
    auto Str = UniqueStrings.find(S->String);
    if (Str == UniqueStrings.end())
      return nullptr;
    MergeableString *MergedString = Str->second;
    if (MergedString == S)
      return nullptr;
    return MergedString;
  }

  void addString(MergeableString *S) {
    AllStrings.push_back(S);
    UniqueStrings.insert({S->String, S});
  }

  uint64_t getTrampolineCount(const std::string &trampolineName);

  uint64_t getTotalTrampolineCount() const;

  // Update epilog in OutputSectionEntry to point epilog entries to
  // entries created in the linker
  eld::Expected<void> updateEpilog(Module &M);

  const llvm::SmallVectorImpl<MergeableString *> &getMergeStrings() const {
    return AllStrings;
  }

private:
  std::string m_Name;
  OutputSectDesc *m_OutputSectDesc = nullptr;
  ELFSection *m_pSection;
  ELFSegment *m_pLoadSegment;
  size_t m_Order;
  bool m_bIsDiscard;
  InputList m_InputList;
  SymbolAssignments m_SymbolAssignments;
  SymbolAssignments m_SectionEndAssignments;
  RuleContainer *FirstNonEmptyRule;
  RuleContainer *m_LastRule;
  std::vector<BranchIsland *> m_BranchIslands;
  std::unordered_map<ResolveInfo *, std::vector<BranchIsland *>>
      BranchIslandForSymbol;
  llvm::StringMap<MergeableString *> UniqueStrings;
  llvm::SmallVector<MergeableString *, 0> AllStrings;
  uint64_t m_Hash = 0;
  llvm::StringMap<uint64_t> m_TrampolineNameToCountMap;
};

} // namespace eld

#endif
