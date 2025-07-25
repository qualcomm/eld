//===- RuleContainer.h-----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_OBJECT_RULECONTAINER_H
#define ELD_OBJECT_RULECONTAINER_H

#include "eld/Object/SectionMap.h"
#include "eld/Script/Assignment.h"
#include "eld/Script/InputSectDesc.h"
#include "eld/Script/OutputSectDesc.h"
#include "eld/Target/LDFileFormat.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/DataTypes.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace eld {

class ELFSection;
class LinkerScript;
class EhFrameHdrSection;
class OutputSectionEntry;
class SectionMap;

class RuleContainer {
public:
  typedef std::vector<Assignment *> SymbolAssignments;
  typedef SymbolAssignments::iterator sym_iterator;

  RuleContainer(SectionMap *, std::string PName, InputSectDesc::Policy PPolicy);

  RuleContainer(SectionMap *, const InputSectDesc &PInputDesc);

  InputSectDesc::Policy policy() const { return MPolicy; }

  const InputSectDesc::Spec &spec() const { return MSpec; }

  const ELFSection *getSection() const { return MPSection; }
  ELFSection *getSection() { return MPSection; }

  std::vector<ELFSection *> &getMatchedInputSections() {
    return MMatchedSections;
  }

  const std::vector<ELFSection *> &getMatchedInputSections() const {
    return MMatchedSections;
  }

  void addMatchedSection(ELFSection *S) { MMatchedSections.push_back(S); }

  void clearSections() { MMatchedSections.clear(); }

  void clearFragments();

  void setRuleHash(uint64_t RHash) { RuleHash = RHash; }

  std::optional<uint64_t> getRuleHash() const { return RuleHash; }

  void init(ELFSection *);

  sym_iterator symBegin() { return MSymbolAssignments.begin(); }
  sym_iterator symEnd() { return MSymbolAssignments.end(); }

  const SymbolAssignments &symAssignments() const { return MSymbolAssignments; }
  SymbolAssignments &symAssignments() { return MSymbolAssignments; }

  void setFragment(Fragment *Frag) { MFrag = Frag; }

  Fragment *frag() const { return MFrag; }

  const InputSectDesc *desc() const { return MDesc; }

  bool isEntry() const {
    return ((MPolicy == InputSectDesc::Keep) ||
            (MPolicy == InputSectDesc::SpecialKeep) ||
            (MPolicy == InputSectDesc::KeepFixed));
  }

  bool isSpecial() const {
    return ((MPolicy == InputSectDesc::SpecialKeep) ||
            (MPolicy == InputSectDesc::SpecialNoKeep));
  }

  bool isFixed() const {
    return ((MPolicy == InputSectDesc::Fixed) ||
            (MPolicy == InputSectDesc::KeepFixed));
  }

  int getMatchCount() const { return MMatchCount; }
  void incMatchCount() { ++MMatchCount; }

  std::chrono::system_clock::duration const &getMatchTime() const {
    return MMatchTime;
  }

  void addMatchTime(std::chrono::system_clock::duration const &Inc) {
    std::lock_guard<std::mutex> MStatsGuard(MMutex);
    MMatchTime += Inc;
  }

  // Does this rule container have any fragments?
  bool hasContent() const;

  // Get the first fragment contained in this rule
  Fragment *getFirstFrag() const;

  // Get the last fragment contained in this rule
  Fragment *getLastFrag() const;

  bool hasAssignments() const { return (MSymbolAssignments.size() > 0); }

  bool isDirty() const { return MDirty; }

  void setDirty() { MDirty = true; }

  void setNextRule(RuleContainer *R) { MNextRule = R; }

  RuleContainer *getNextRule() const { return MNextRule; }

  RuleContainer *getNextRuleWithContent() const;

  void dumpMap(llvm::raw_ostream &O);

  /// Updates matched sections property of all the rules.
  static void updateMatchedSections(const Module &Module);

  /// Get rule container as a string for diagnostics.
  std::string getAsString() const;

private:
  std::optional<uint64_t> RuleHash;
  InputSectDesc::Policy MPolicy;
  InputSectDesc::Spec MSpec;
  ELFSection *MPSection;
  std::vector<ELFSection *> MMatchedSections;
  SymbolAssignments MSymbolAssignments;
  bool MDirty;
  Fragment *MFrag;
  const InputSectDesc *MDesc;
  std::atomic<uint32_t> MMatchCount;
  std::chrono::system_clock::duration MMatchTime;
  std::mutex MMutex;
  RuleContainer *MNextRule;
};

} // end namespace eld

#endif
