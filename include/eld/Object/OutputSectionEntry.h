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

  OutputSectionEntry(SectionMap *, std::string PName);
  OutputSectionEntry(SectionMap *, ELFSection *);
  OutputSectionEntry(SectionMap *, OutputSectDesc &POutputDesc);
  OutputSectionEntry(SectionMap *Parent, std::string PName,
                     LDFileFormat::Kind PKind, uint32_t PType, uint32_t PFlag,
                     uint32_t PAlign);

  llvm::StringRef name() const { return Name; }

  const OutputSectDesc::Prolog &prolog() const {
    return MOutputSectDesc->prolog();
  }
  OutputSectDesc::Prolog &prolog() { return MOutputSectDesc->prolog(); }

  const OutputSectDesc::Epilog &epilog() const {
    return MOutputSectDesc->epilog();
  }
  OutputSectDesc::Epilog &epilog() { return MOutputSectDesc->epilog(); }

  size_t order() const { return MOrder; }

  bool hasOrder() const { return (MOrder != UINT_MAX); }

  void setOrder(size_t POrder) { MOrder = POrder; }

  bool hasContent() const;

  const ELFSection *getSection() const { return MPSection; }
  ELFSection *getSection() { return MPSection; }

  void setSection(ELFSection *PSection) {
    MPSection = PSection;
    computeHash();
  }
  void computeHash();
  iterator begin() { return MInputList.begin(); }
  iterator end() { return MInputList.end(); }

  const_iterator begin() const { return MInputList.begin(); }
  const_iterator end() const { return MInputList.end(); }

  const InputList &getRuleContainer() const { return MInputList; }

  const_reference front() const { return MInputList.front(); }
  reference front() { return MInputList.front(); }
  const_reference back() const { return MInputList.back(); }
  reference back() { return MInputList.back(); }

  size_t size() const { return MInputList.size(); }

  bool empty() const { return MInputList.empty(); }

  bool isDiscard() const { return MBIsDiscard; }

  void append(RuleContainer *PInput) { MInputList.push_back(PInput); }

  sym_iterator symBegin() { return MSymbolAssignments.begin(); }
  sym_iterator symEnd() { return MSymbolAssignments.end(); }

  const SymbolAssignments &symAssignments() const { return MSymbolAssignments; }
  SymbolAssignments &symAssignments() { return MSymbolAssignments; }

  const SymbolAssignments &sectionsEndAssignments() const {
    return MSectionEndAssignments;
  }
  SymbolAssignments &sectionEndAssignments() { return MSectionEndAssignments; }

  sym_iterator sectionendsymBegin() { return MSectionEndAssignments.begin(); }
  sym_iterator sectionendsymEnd() { return MSectionEndAssignments.end(); }

  void moveSectionAssignments(OutputSectionEntry *Out) {
    MSectionEndAssignments = Out->sectionEndAssignments();
    Out->sectionEndAssignments().clear();
  }

  bool hasAssignments() const { return (MSymbolAssignments.size() > 0); }

  // A section may be part of multiple segments, this only returns the
  // segment where the section would get loaded.
  void setLoadSegment(ELFSegment *E) { MPLoadSegment = E; }

  ELFSegment *getLoadSegment() const { return MPLoadSegment; }

  // Set the first fragment in the output section.
  void setFirstNonEmptyRule(RuleContainer *R) { FirstNonEmptyRule = R; }

  RuleContainer *getFirstNonEmptyRule() const { return FirstNonEmptyRule; }

  Fragment *getFirstFrag() const;

  RuleContainer *getLastRule() const { return MLastRule; }

  void setLastRule(RuleContainer *R) { MLastRule = R; }

  RuleContainer *createDefaultRule(Module &M);

  // ------------------Branch island support ------------------------
  BranchIslandsIter islandsBegin() { return MBranchIslands.begin(); }

  BranchIslandsIter islandsEnd() { return MBranchIslands.end(); }

  void addBranchIsland(BranchIsland *B) { MBranchIslands.push_back(B); }

  void addBranchIsland(ResolveInfo *PSym, BranchIsland *B) {
    BranchIslandForSymbol[PSym];
    BranchIslandForSymbol[PSym].push_back(B);
    MBranchIslands.push_back(B);
  }

  uint32_t getNumBranchIslands() const { return MBranchIslands.size(); }

  void dump(llvm::raw_ostream &Outs) const;

  uint64_t getHash() {
    if (!MHash)
      computeHash();
    return MHash;
  }

  std::string getSectionTypeStr() const;

  // ----------------------Reuse trampolines optimization---------------
  std::vector<BranchIsland *>
  getBranchIslandsForSymbol(ResolveInfo *PSym) const {
    std::vector<BranchIsland *> Islands;
    auto Iter = BranchIslandForSymbol.find(PSym);
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

  uint64_t getTrampolineCount(const std::string &TrampolineName);

  uint64_t getTotalTrampolineCount() const;

  // Update epilog in OutputSectionEntry to point epilog entries to
  // entries created in the linker
  eld::Expected<void> updateEpilog(Module &M);

  const llvm::SmallVectorImpl<MergeableString *> &getMergeStrings() const {
    return AllStrings;
  }

private:
  std::string Name;
  OutputSectDesc *MOutputSectDesc = nullptr;
  ELFSection *MPSection;
  ELFSegment *MPLoadSegment;
  size_t MOrder;
  bool MBIsDiscard;
  InputList MInputList;
  SymbolAssignments MSymbolAssignments;
  SymbolAssignments MSectionEndAssignments;
  RuleContainer *FirstNonEmptyRule;
  RuleContainer *MLastRule;
  std::vector<BranchIsland *> MBranchIslands;
  std::unordered_map<ResolveInfo *, std::vector<BranchIsland *>>
      BranchIslandForSymbol;
  llvm::StringMap<MergeableString *> UniqueStrings;
  llvm::SmallVector<MergeableString *, 0> AllStrings;
  uint64_t MHash = 0;
  llvm::StringMap<uint64_t> MTrampolineNameToCountMap;
};

} // namespace eld

#endif
