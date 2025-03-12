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

  RuleContainer(SectionMap *, std::string pName, InputSectDesc::Policy pPolicy);

  RuleContainer(SectionMap *, const InputSectDesc &pInputDesc);

  InputSectDesc::Policy policy() const { return m_Policy; }

  const InputSectDesc::Spec &spec() const { return m_Spec; }

  const ELFSection *getSection() const { return m_pSection; }
  ELFSection *getSection() { return m_pSection; }

  std::vector<ELFSection *> &getMatchedInputSections() {
    return m_MatchedSections;
  }

  const std::vector<ELFSection *> &getMatchedInputSections() const {
    return m_MatchedSections;
  }

  void addMatchedSection(ELFSection *S) { m_MatchedSections.push_back(S); }

  void clearSections() { m_MatchedSections.clear(); }

  void clearFragments();

  void init(ELFSection *);

  sym_iterator sym_begin() { return m_SymbolAssignments.begin(); }
  sym_iterator sym_end() { return m_SymbolAssignments.end(); }

  const SymbolAssignments &symAssignments() const {
    return m_SymbolAssignments;
  }
  SymbolAssignments &symAssignments() { return m_SymbolAssignments; }

  void setFragment(Fragment *frag) { m_Frag = frag; }

  Fragment *frag() const { return m_Frag; }

  const InputSectDesc *desc() const { return m_Desc; }

  bool isEntry() const {
    return ((m_Policy == InputSectDesc::Keep) ||
            (m_Policy == InputSectDesc::SpecialKeep) ||
            (m_Policy == InputSectDesc::KeepFixed));
  }

  bool isSpecial() const {
    return ((m_Policy == InputSectDesc::SpecialKeep) ||
            (m_Policy == InputSectDesc::SpecialNoKeep));
  }

  bool isFixed() const {
    return ((m_Policy == InputSectDesc::Fixed) ||
            (m_Policy == InputSectDesc::KeepFixed));
  }

  int getMatchCount() const { return m_matchCount; }
  void incMatchCount() { ++m_matchCount; }

  std::chrono::system_clock::duration const &getMatchTime() const {
    return m_matchTime;
  }

  void addMatchTime(std::chrono::system_clock::duration const &Inc) {
    std::lock_guard<std::mutex> m_StatsGuard(m_Mutex);
    m_matchTime += Inc;
  }

  // Does this rule container have any fragments?
  bool hasContent() const;

  // Get the first fragment contained in this rule
  Fragment *getFirstFrag() const;

  // Get the last fragment contained in this rule
  Fragment *getLastFrag() const;

  bool hasAssignments() const { return (m_SymbolAssignments.size() > 0); }

  bool isDirty() const { return m_Dirty; }

  void setDirty() { m_Dirty = true; }

  void setNextRule(RuleContainer *R) { m_NextRule = R; }

  RuleContainer *getNextRule() const { return m_NextRule; }

  RuleContainer *getNextRuleWithContent() const;

  void dumpMap(llvm::raw_ostream &O);

  /// Updates matched sections property of all the rules.
  static void updateMatchedSections(const Module &module);

  /// Get rule container as a string for diagnostics.
  std::string getAsString() const;

private:
  InputSectDesc::Policy m_Policy;
  InputSectDesc::Spec m_Spec;
  ELFSection *m_pSection;
  std::vector<ELFSection *> m_MatchedSections;
  SymbolAssignments m_SymbolAssignments;
  bool m_Dirty;
  Fragment *m_Frag;
  const InputSectDesc *m_Desc;
  std::atomic<uint32_t> m_matchCount;
  std::chrono::system_clock::duration m_matchTime;
  std::mutex m_Mutex;
  RuleContainer *m_NextRule;
};

} // end namespace eld

#endif
