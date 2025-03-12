//===- OutputSectionEntry.cpp----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Object/OutputSectionEntry.h"
#include "eld/Core/Module.h"
#include "eld/Fragment/RegionFragmentEx.h"
#include "eld/Readers/ELFSection.h"
#include <algorithm>

using namespace eld;

namespace {
uint64_t Index = 0;
}

OutputSectionEntry::OutputSectionEntry(SectionMap *parent, std::string pName)
    : m_Name(pName), m_pSection(nullptr), m_pLoadSegment(nullptr),
      m_Order(UINT_MAX), FirstNonEmptyRule(nullptr), m_LastRule(nullptr) {
  m_OutputSectDesc = eld::make<OutputSectDesc>(pName);
  m_pSection = parent->createELFSection(pName, LDFileFormat::Regular,
                                        /*Type=*/0, /*Flags=*/0, /*EntSize=*/0);
  // Set a default index. This index will be overwritten later by postLayout.
  m_pSection->setIndex(Index++);
  m_pSection->setOutputSection(this);
  m_bIsDiscard = pName.compare("/DISCARD/") == 0;
}

OutputSectionEntry::OutputSectionEntry(SectionMap *parent, ELFSection *S)
    : m_Name(S->name()), m_pSection(S), m_pLoadSegment(nullptr),
      m_Order(UINT_MAX), FirstNonEmptyRule(nullptr), m_LastRule(nullptr) {
  m_OutputSectDesc = eld::make<OutputSectDesc>(m_Name);
  S->setOutputSection(this);
  m_bIsDiscard = m_Name.compare("/DISCARD/") == 0;
}

OutputSectionEntry::OutputSectionEntry(SectionMap *parent, std::string pName,
                                       LDFileFormat::Kind pKind, uint32_t pType,
                                       uint32_t pFlag, uint32_t pAlign)
    : m_Name(pName), m_pLoadSegment(nullptr), m_Order(UINT_MAX),
      FirstNonEmptyRule(nullptr), m_LastRule(nullptr) {
  m_OutputSectDesc = eld::make<OutputSectDesc>(pName);
  m_pSection =
      parent->createELFSection(pName, pKind, pType, pFlag, /*EntSize*/ 0);
  m_pSection->setAddrAlign(pAlign);
  m_pSection->setOutputSection(this);
  // Set a default index. This index will be overwritten later by postLayout.
  m_pSection->setIndex(Index++);
  m_bIsDiscard = pName.compare("/DISCARD/") == 0;
}

OutputSectionEntry::OutputSectionEntry(SectionMap *parent,
                                       OutputSectDesc &pOutputDesc)
    : m_Name(pOutputDesc.name()), m_OutputSectDesc(&pOutputDesc),
      m_pSection(nullptr), m_pLoadSegment(nullptr), m_Order(UINT_MAX),
      FirstNonEmptyRule(nullptr), m_LastRule(nullptr) {
  m_pSection = parent->createELFSection(m_Name, LDFileFormat::Regular,
                                        /*Type*/ 0, /*Flags*/ 0, /*EntSize*/ 0);
  // Set a default index. This index will be overwritten later by postLayout.
  m_pSection->setIndex(Index++);
  m_pSection->setOutputSection(this);
  m_bIsDiscard = m_Name.compare("/DISCARD/") == 0;
}

bool OutputSectionEntry::hasContent() const {
  return m_pSection != nullptr &&
         (m_pSection->isWanted() || (m_pSection->size() != 0));
}
void OutputSectionEntry::computeHash() {
  std::string Res;
  llvm::raw_string_ostream OSS(Res);
  dump(OSS);
  m_Hash = llvm::hash_combine(name(), m_pSection->getIndex(), Res);
}

uint64_t
OutputSectionEntry::getTrampolineCount(const std::string &trampolineName) {
  return m_TrampolineNameToCountMap[trampolineName]++;
}

uint64_t OutputSectionEntry::getTotalTrampolineCount() const {
  uint64_t count = 0;
  for (const auto &item : m_TrampolineNameToCountMap)
    count += item.second;
  return count;
}

Fragment *OutputSectionEntry::getFirstFrag() const {
  if (!FirstNonEmptyRule ||
      !FirstNonEmptyRule->getSection()->getFragmentList().size())
    return nullptr;
  return FirstNonEmptyRule->getSection()->getFragmentList().front();
}

RuleContainer *OutputSectionEntry::createDefaultRule(eld::Module &M) {
  // Add a default spec to catch rules that belong to the output section.
  InputSectDesc::Spec defaultSpec;
  defaultSpec.initialize();
  StringList *stringList = eld::make<StringList>();

  WildcardPattern *PatOne = make<WildcardPattern>("*");
  M.getScript().registerWildCardPattern(PatOne);
  defaultSpec.m_pWildcardFile = PatOne;

  WildcardPattern *PatTwo = make<WildcardPattern>(m_Name);
  M.getScript().registerWildCardPattern(PatTwo);
  stringList->push_back(PatTwo);

  defaultSpec.m_pWildcardSections = stringList;
  defaultSpec.m_pIsArchive = 0;

  static OutputSectDesc O(m_Name);
  O.initialize();
  InputSectDesc *Input =
      make<InputSectDesc>(M.getScript().getIncrementedRuleCount(),
                          InputSectDesc::SpecialNoKeep, defaultSpec, O);
  Input->setInputFileInContext(
      M.getInternalInput(Module::InternalInputType::Script));

  auto input = make<RuleContainer>(&M.getScript().sectionMap(), *Input);
  input->getSection()->setOutputSection(this);

  if (this->getLastRule())
    this->getLastRule()->setNextRule(input);
  this->setLastRule(input);

  append(input);

  return input;
}

RuleContainer *OutputSectionEntry::createRule(eld::Module &M,
                                              std::string Annotation,
                                              InputFile *I) {
  InputSectDesc::Spec Spec;
  Spec.initialize();
  StringList *stringList = eld::make<StringList>();

  WildcardPattern *PatOne = make<WildcardPattern>("*");
  M.getScript().registerWildCardPattern(PatOne);
  Spec.m_pWildcardFile = PatOne;

  WildcardPattern *PatTwo = make<WildcardPattern>(
      M.getScript().saveString(m_Name + "." + Annotation));

  M.getScript().registerWildCardPattern(PatTwo);
  stringList->push_back(PatTwo);

  Spec.m_pWildcardSections = stringList;
  Spec.m_pIsArchive = 0;

  OutputSectDesc *O = eld::make<OutputSectDesc>(m_Name);
  O->initialize();
  InputSectDesc *Input =
      make<InputSectDesc>(M.getScript().getIncrementedRuleCount(),
                          InputSectDesc::SpecialNoKeep, Spec, *O);
  Input->setInputFileInContext(I);

  RuleContainer *R = make<RuleContainer>(&M.getScript().sectionMap(), *Input);
  R->getSection()->setOutputSection(this);
  return R;
}

void OutputSectionEntry::dump(llvm::raw_ostream &outs) const {
  m_OutputSectDesc->dump(outs);
}

bool OutputSectionEntry::insertAfterRule(RuleContainer *C, RuleContainer *R) {
  auto Iter = std::find(m_InputList.begin(), m_InputList.end(), C);
  if (Iter == m_InputList.end())
    return false;
  ++Iter;
  Iter = m_InputList.insert(Iter, R);
  R->setNextRule(C->getNextRule());
  C->setNextRule(R);
  return true;
}

bool OutputSectionEntry::insertBeforeRule(RuleContainer *C, RuleContainer *R) {
  auto iter = std::find(m_InputList.begin(), m_InputList.end(), C);
  if (iter == m_InputList.end())
    return false;
  if (iter != m_InputList.begin()) {
    auto prevIter = iter - 1;
    (*prevIter)->setNextRule(R);
  }
  m_InputList.insert(iter, R);
  R->setNextRule(C);
  return true;
}

std::optional<llvm::StringRef> getRegion(const Fragment *F) {
  if (!F)
    return {};
  if (auto *R = llvm::dyn_cast<RegionFragment>(F))
    return R->getRegion();
  if (auto *R = llvm::dyn_cast<RegionFragmentEx>(F))
    return R->getRegion();
  return {};
}

std::string OutputSectionEntry::getSectionTypeStr() const {
  return ELFSection::getELFTypeStr(m_Name, m_pSection->getType()).str();
}
