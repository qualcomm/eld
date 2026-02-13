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
class OverlayDesc;
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
    return OutputSectionDesc->prolog();
  }
  OutputSectDesc::Prolog &prolog() { return OutputSectionDesc->prolog(); }

  const OutputSectDesc::Epilog &epilog() const {
    return OutputSectionDesc->epilog();
  }
  OutputSectDesc::Epilog &epilog() { return OutputSectionDesc->epilog(); }

  size_t order() const { return Order; }

  bool hasOrder() const { return (Order != UINT_MAX); }

  void setOrder(size_t POrder) { Order = POrder; }

  bool hasContent() const;

  const ELFSection *getSection() const { return OutputELFSection; }
  ELFSection *getSection() { return OutputELFSection; }

  void setSection(ELFSection *PSection) {
    OutputELFSection = PSection;
    computeHash();
  }
  void computeHash();
  iterator begin() { return Inputs.begin(); }
  iterator end() { return Inputs.end(); }

  const_iterator begin() const { return Inputs.begin(); }
  const_iterator end() const { return Inputs.end(); }

  const InputList &getRuleContainer() const { return Inputs; }

  const_reference front() const { return Inputs.front(); }
  reference front() { return Inputs.front(); }
  const_reference back() const { return Inputs.back(); }
  reference back() { return Inputs.back(); }

  size_t size() const { return Inputs.size(); }

  bool empty() const { return Inputs.empty(); }

  bool isDiscard() const { return IsDiscard; }

  void append(RuleContainer *PInput) { Inputs.push_back(PInput); }

  const SymbolAssignments &sectionsEndAssignments() const {
    return SectionEndAssignments;
  }
  SymbolAssignments &sectionEndAssignments() { return SectionEndAssignments; }

  sym_iterator sectionendsymBegin() { return SectionEndAssignments.begin(); }
  sym_iterator sectionendsymEnd() { return SectionEndAssignments.end(); }

  void moveSectionAssignments(OutputSectionEntry *Out) {
    SectionEndAssignments = Out->sectionEndAssignments();
    Out->sectionEndAssignments().clear();
  }

  // A section may be part of multiple segments, this only returns the
  // segment where the section would get loaded.
  void setLoadSegment(ELFSegment *E) { LoadSegment = E; }

  ELFSegment *getLoadSegment() const { return LoadSegment; }

  // Set the first fragment in the output section.
  void setFirstNonEmptyRule(RuleContainer *R) { FirstNonEmptyRule = R; }

  RuleContainer *getFirstNonEmptyRule() const { return FirstNonEmptyRule; }

  Fragment *getFirstFrag() const;

  RuleContainer *getLastRule() const { return LastRule; }

  void setLastRule(RuleContainer *R) { LastRule = R; }

  RuleContainer *createDefaultRule(Module &M);

  // ------------------Branch island support ------------------------
  BranchIslandsIter islandsBegin() { return BranchIslands.begin(); }

  BranchIslandsIter islandsEnd() { return BranchIslands.end(); }

  void addBranchIsland(BranchIsland *B) { BranchIslands.push_back(B); }

  void addBranchIsland(ResolveInfo *PSym, BranchIsland *B) {
    BranchIslandForSymbol[PSym];
    BranchIslandForSymbol[PSym].push_back(B);
    BranchIslands.push_back(B);
  }

  uint32_t getNumBranchIslands() const { return BranchIslands.size(); }

  void dump(llvm::raw_ostream &Outs) const;

  uint64_t getHash() {
    if (!Hash)
      computeHash();
    return Hash;
  }

  std::string getSectionTypeStr() const;

  uint64_t pAddr() const { return PAddr; }
  void setPaddr(uint64_t A) { PAddr = A; }

  bool hasOverlayDesc() const { return ThisOverlayDesc != nullptr; }

  OverlayDesc *getOverlayDesc() const { return ThisOverlayDesc; }

  void setOverlayDesc(OverlayDesc *O) { ThisOverlayDesc = O; }

  const OutputSectDesc *getOutputSectDesc() const { return OutputSectionDesc; }

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
  OutputSectDesc *OutputSectionDesc = nullptr;
  ELFSection *OutputELFSection;
  ELFSegment *LoadSegment;
  size_t Order;
  bool IsDiscard;
  InputList Inputs;
  SymbolAssignments SectionEndAssignments;
  RuleContainer *FirstNonEmptyRule;
  RuleContainer *LastRule;
  std::vector<BranchIsland *> BranchIslands;
  std::unordered_map<ResolveInfo *, std::vector<BranchIsland *>>
      BranchIslandForSymbol;
  llvm::StringMap<MergeableString *> UniqueStrings;
  llvm::SmallVector<MergeableString *, 0> AllStrings;
  uint64_t Hash = 0;
  llvm::StringMap<uint64_t> TrampolineNameToCountMap;
  uint64_t PAddr = 0;
  OverlayDesc *ThisOverlayDesc = nullptr;
};

} // namespace eld

#endif
