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

RuleContainer::RuleContainer(SectionMap *Parent, std::string PName,
                             InputSectDesc::Policy PPolicy)
    : MPolicy(PPolicy), MDirty(false), MFrag(nullptr), MDesc(nullptr),
      MMatchCount(0), MMatchTime(0), MNextRule(nullptr) {
  WildcardPattern *P1 = make<WildcardPattern>("*");
  Parent->getLinkerScript().registerWildCardPattern(P1);
  MSpec.WildcardFilePattern = P1;
  MSpec.InputArchiveMember = nullptr;
  MSpec.InputIsArchive = false;
  MSpec.ExcludeFilesRule = nullptr;
  StringList *Sections = make<StringList>();
  WildcardPattern *P2 =
      make<WildcardPattern>(Parent->getLinkerScript().saveString(PName));
  Parent->getLinkerScript().registerWildCardPattern(P2);
  Sections->pushBack(P2);

  MSpec.WildcardSectionPattern = Sections;

  MPSection = Parent->createELFSection(PName, LDFileFormat::Regular,
                                       /*Type=*/0, /*Flags=*/0, /*EntSize=*/0);
  MPSection->setMatchedLinkerScriptRule(this);
}

RuleContainer::RuleContainer(SectionMap *Parent,
                             const InputSectDesc &PInputDesc)
    : MPolicy(PInputDesc.policy()), MDirty(false), MFrag(nullptr),
      MDesc(&PInputDesc), MMatchCount(0), MMatchTime(0), MNextRule(nullptr) {
  // FIXME: We can use an overloaded assignment operator of InputSectDesc::Spec
  // instead of explicitly setting each value.
  MSpec.WildcardFilePattern = PInputDesc.spec().WildcardFilePattern;
  MSpec.WildcardSectionPattern = PInputDesc.spec().WildcardSectionPattern;
  MSpec.InputArchiveMember = PInputDesc.spec().InputArchiveMember;
  MSpec.InputIsArchive = PInputDesc.spec().InputIsArchive;
  MSpec.ExcludeFilesRule = PInputDesc.spec().getExcludeFiles();
  MPSection = Parent->createELFSection("", LDFileFormat::Regular, /*Type=*/0,
                                       /*Flags=*/0, /*EntSize=*/0);
  MPSection->setMatchedLinkerScriptRule(this);
}

void RuleContainer::clearFragments() { MPSection->clearFragments(); }

void RuleContainer::init(ELFSection *S) {
  MPSection->setAddrAlign(S->getAddrAlign());
  MPSection->setType(S->getType());
  MPSection->setFlags(S->getFlags());
}

void RuleContainer::dumpMap(llvm::raw_ostream &Ostream) {
  Ostream << " [" << getMatchCount() << ":"
          << static_cast<int>(
                 std::chrono::duration_cast<std::chrono::milliseconds>(
                     getMatchTime())
                     .count())
          << "]";
}

bool RuleContainer::hasContent() const {
  return !MPSection->getFragmentList().empty();
}

RuleContainer *RuleContainer::getNextRuleWithContent() const {
  RuleContainer *Result = this->getNextRule();
  while (Result) {
    if (Result->hasContent())
      return Result;
    Result = Result->getNextRule();
  }
  return Result;
}

Fragment *RuleContainer::getFirstFrag() const {
  return MPSection && !MPSection->getFragmentList().empty()
             ? MPSection->getFragmentList().front()
             : nullptr;
}

Fragment *RuleContainer::getLastFrag() const {
  return MPSection && !MPSection->getFragmentList().empty()
             ? MPSection->getFragmentList().back()
             : nullptr;
}

void RuleContainer::updateMatchedSections(const Module &Module) {
  // Clear the vector of sections collected by the rules.
  for (OutputSectionEntry *OutSect : Module.getScript().sectionMap()) {
    for (RuleContainer *Rule : *OutSect)
      Rule->clearSections();
  }

  for (const InputFile *InputFile : Module.getObjectList()) {
    const ObjectFile *ObjFile = llvm::dyn_cast<ObjectFile>(InputFile);
    for (Section *S : ObjFile->getSections()) {
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
  std::string S;
  llvm::raw_string_ostream Stream(S);
  if (!desc())
    return "";
  desc()->dumpMap(Stream, /*useColor=*/false, /*useNewLine=*/false);
  return Stream.str();
}
