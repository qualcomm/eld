//===- RuleContainer.cpp---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Object/RuleContainer.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Core/Module.h"
#include "eld/Readers/ELFSection.h"
#include "llvm/Support/Casting.h"

using namespace eld;

RuleContainer::RuleContainer(SectionMap *parent, std::string pName,
                             InputSectDesc::Policy pPolicy)
    : m_Policy(pPolicy), m_Dirty(false), m_Frag(nullptr), m_Desc(nullptr),
      m_matchCount(0), m_matchTime(0), m_NextRule(nullptr) {
  WildcardPattern *P1 = make<WildcardPattern>("*");
  parent->getLinkerScript().registerWildCardPattern(P1);
  m_Spec.m_pWildcardFile = P1;
  m_Spec.m_pArchiveMember = nullptr;
  m_Spec.m_pIsArchive = false;
  m_Spec.m_ExcludeFiles = nullptr;
  StringList *sections = make<StringList>();
  WildcardPattern *P2 =
      make<WildcardPattern>(parent->getLinkerScript().saveString(pName));
  parent->getLinkerScript().registerWildCardPattern(P2);
  sections->push_back(P2);

  m_Spec.m_pWildcardSections = sections;

  m_pSection = parent->createELFSection(pName, LDFileFormat::Regular,
                                        /*Type=*/0, /*Flags=*/0, /*EntSize=*/0);
  m_pSection->setMatchedLinkerScriptRule(this);
}

RuleContainer::RuleContainer(SectionMap *parent,
                             const InputSectDesc &pInputDesc)
    : m_Policy(pInputDesc.policy()), m_Dirty(false), m_Frag(nullptr),
      m_Desc(&pInputDesc), m_matchCount(0), m_matchTime(0),
      m_NextRule(nullptr) {
  // FIXME: We can use an overloaded assignment operator of InputSectDesc::Spec
  // instead of explicitly setting each value.
  m_Spec.m_pWildcardFile = pInputDesc.spec().m_pWildcardFile;
  m_Spec.m_pWildcardSections = pInputDesc.spec().m_pWildcardSections;
  m_Spec.m_pArchiveMember = pInputDesc.spec().m_pArchiveMember;
  m_Spec.m_pIsArchive = pInputDesc.spec().m_pIsArchive;
  m_Spec.m_ExcludeFiles = pInputDesc.spec().getExcludeFiles();
  m_pSection = parent->createELFSection("", LDFileFormat::Regular, /*Type=*/0,
                                        /*Flags=*/0, /*EntSize=*/0);
  m_pSection->setMatchedLinkerScriptRule(this);
}

void RuleContainer::clearFragments() { m_pSection->clearFragments(); }

void RuleContainer::init(ELFSection *S) {
  m_pSection->setAddrAlign(S->getAddrAlign());
  m_pSection->setType(S->getType());
  m_pSection->setFlags(S->getFlags());
}

void RuleContainer::dumpMap(llvm::raw_ostream &ostream) {
  ostream << " [" << getMatchCount() << ":"
          << static_cast<int>(
                 std::chrono::duration_cast<std::chrono::milliseconds>(
                     getMatchTime())
                     .count())
          << "]";
}

bool RuleContainer::hasContent() const {
  return !m_pSection->getFragmentList().empty();
}

RuleContainer *RuleContainer::getNextRuleWithContent() const {
  RuleContainer *result = this->getNextRule();
  while (result) {
    if (result->hasContent())
      return result;
    result = result->getNextRule();
  }
  return result;
}

Fragment *RuleContainer::getFirstFrag() const {
  return m_pSection && !m_pSection->getFragmentList().empty()
             ? m_pSection->getFragmentList().front()
             : nullptr;
}

Fragment *RuleContainer::getLastFrag() const {
  return m_pSection && !m_pSection->getFragmentList().empty()
             ? m_pSection->getFragmentList().back()
             : nullptr;
}

void RuleContainer::updateMatchedSections(const Module &module) {
  // Clear the vector of sections collected by the rules.
  for (OutputSectionEntry *outSect : module.getScript().sectionMap()) {
    for (RuleContainer *rule : *outSect)
      rule->clearSections();
  }

  for (const InputFile *inputFile : module.getObjectList()) {
    const ObjectFile *objFile = llvm::dyn_cast<ObjectFile>(inputFile);
    for (Section *S : objFile->getSections()) {
      ELFSection *ELFSect = llvm::dyn_cast<ELFSection>(S);
      if (!ELFSect)
        continue;
      RuleContainer *R = S->getMatchedLinkerScriptRule();
      if (R)
        R->addMatchedSection(ELFSect);
    }
  }
}

std::string RuleContainer::getAsString() const {
  std::string s;
  llvm::raw_string_ostream stream(s);
  if (!desc())
    return "";
  desc()->dumpMap(stream, /*useColor=*/false, /*useNewLine=*/false);
  return stream.str();
}
