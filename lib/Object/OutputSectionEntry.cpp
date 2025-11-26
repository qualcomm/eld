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
} // namespace

OutputSectionEntry::OutputSectionEntry(SectionMap *Parent, std::string PName)
    : Name(PName), OutputELFSection(nullptr), LoadSegment(nullptr),
      Order(UINT_MAX), FirstNonEmptyRule(nullptr), LastRule(nullptr) {
  OutputSectionDesc = eld::make<OutputSectDesc>(PName);
  OutputELFSection =
      Parent->createELFSection(PName, LDFileFormat::Regular,
                               /*Type=*/0, /*Flags=*/0, /*EntSize=*/0);
  // Set a default index. This index will be overwritten later by postLayout.
  OutputELFSection->setIndex(Index++);
  OutputELFSection->setOutputSection(this);
  IsDiscard = PName.compare("/DISCARD/") == 0;
}

OutputSectionEntry::OutputSectionEntry(SectionMap *Parent, ELFSection *S)
    : Name(S->name()), OutputELFSection(S), LoadSegment(nullptr),
      Order(UINT_MAX), FirstNonEmptyRule(nullptr), LastRule(nullptr) {
  OutputSectionDesc = eld::make<OutputSectDesc>(Name);
  S->setOutputSection(this);
  IsDiscard = Name.compare("/DISCARD/") == 0;
}

OutputSectionEntry::OutputSectionEntry(SectionMap *Parent, std::string PName,
                                       LDFileFormat::Kind PKind, uint32_t PType,
                                       uint32_t PFlag, uint32_t PAlign)
    : Name(PName), LoadSegment(nullptr), Order(UINT_MAX),
      FirstNonEmptyRule(nullptr), LastRule(nullptr) {
  OutputSectionDesc = eld::make<OutputSectDesc>(PName);
  OutputELFSection =
      Parent->createELFSection(PName, PKind, PType, PFlag, /*EntSize*/ 0);
  OutputELFSection->setAddrAlign(PAlign);
  OutputELFSection->setOutputSection(this);
  // Set a default index. This index will be overwritten later by postLayout.
  OutputELFSection->setIndex(Index++);
  IsDiscard = PName.compare("/DISCARD/") == 0;
}

OutputSectionEntry::OutputSectionEntry(SectionMap *Parent,
                                       OutputSectDesc &POutputDesc)
    : Name(POutputDesc.name()), OutputSectionDesc(&POutputDesc),
      OutputELFSection(nullptr), LoadSegment(nullptr), Order(UINT_MAX),
      FirstNonEmptyRule(nullptr), LastRule(nullptr) {
  OutputELFSection =
      Parent->createELFSection(Name, LDFileFormat::Regular,
                               /*Type*/ 0, /*Flags*/ 0, /*EntSize*/ 0);
  // Set a default index. This index will be overwritten later by postLayout.
  OutputELFSection->setIndex(Index++);
  OutputELFSection->setOutputSection(this);
  IsDiscard = Name.compare("/DISCARD/") == 0;
}

bool OutputSectionEntry::hasContent() const {
  return OutputELFSection != nullptr &&
         (OutputELFSection->isWanted() || (OutputELFSection->size() != 0));
}
void OutputSectionEntry::computeHash() {
  std::string Res;
  llvm::raw_string_ostream OSS(Res);
  dump(OSS);
  Hash = llvm::hash_combine(name(), OutputELFSection->getIndex(), Res);
}

uint64_t
OutputSectionEntry::getTrampolineCount(const std::string &TrampolineName) {
  return TrampolineNameToCountMap[TrampolineName]++;
}

uint64_t OutputSectionEntry::getTotalTrampolineCount() const {
  uint64_t Count = 0;
  for (const auto &Item : TrampolineNameToCountMap)
    Count += Item.second;
  return Count;
}

Fragment *OutputSectionEntry::getFirstFrag() const {
  if (!FirstNonEmptyRule ||
      !FirstNonEmptyRule->getSection()->getFragmentList().size())
    return nullptr;
  return FirstNonEmptyRule->getSection()->getFragmentList().front();
}

RuleContainer *OutputSectionEntry::createDefaultRule(eld::Module &M) {
  // Add a default spec to catch rules that belong to the output section.
  InputSectDesc::Spec DefaultSpec;
  DefaultSpec.initialize();
  StringList *StringList = eld::make<eld::StringList>();

  WildcardPattern *PatOne = make<WildcardPattern>("*");
  M.getScript().registerWildCardPattern(PatOne);
  DefaultSpec.WildcardFilePattern = PatOne;

  WildcardPattern *PatTwo = make<WildcardPattern>(Name);
  M.getScript().registerWildCardPattern(PatTwo);
  StringList->pushBack(PatTwo);

  DefaultSpec.WildcardSectionPattern = StringList;
  DefaultSpec.InputIsArchive = 0;

  static OutputSectDesc O(Name);
  O.initialize();
  InputSectDesc *Input =
      make<InputSectDesc>(M.getScript().getIncrementedRuleCount(),
                          InputSectDesc::SpecialNoKeep, DefaultSpec, O);
  Input->setInputFileInContext(
      M.getInternalInput(Module::InternalInputType::Script));

  auto *RC = make<RuleContainer>(&M.getScript().sectionMap(), *Input);
  RC->getSection()->setOutputSection(this);

  if (this->getLastRule())
    this->getLastRule()->setNextRule(RC);
  this->setLastRule(RC);

  append(RC);

  return RC;
}

RuleContainer *OutputSectionEntry::createRule(eld::Module &M,
                                              std::string Annotation,
                                              InputFile *I) {
  InputSectDesc::Spec Spec;
  Spec.initialize();
  StringList *StringList = eld::make<eld::StringList>();

  WildcardPattern *PatOne = make<WildcardPattern>("*");
  M.getScript().registerWildCardPattern(PatOne);
  Spec.WildcardFilePattern = PatOne;

  WildcardPattern *PatTwo =
      make<WildcardPattern>(M.getScript().saveString(Name + "." + Annotation));

  M.getScript().registerWildCardPattern(PatTwo);
  StringList->pushBack(PatTwo);

  Spec.WildcardSectionPattern = StringList;
  Spec.InputIsArchive = 0;

  OutputSectDesc *O = eld::make<OutputSectDesc>(Name);
  O->initialize();
  InputSectDesc *Input =
      make<InputSectDesc>(M.getScript().getIncrementedRuleCount(),
                          InputSectDesc::SpecialNoKeep, Spec, *O);
  Input->setInputFileInContext(I);

  RuleContainer *R = make<RuleContainer>(&M.getScript().sectionMap(), *Input);
  R->getSection()->setOutputSection(this);
  return R;
}

void OutputSectionEntry::dump(llvm::raw_ostream &Outs) const {
  OutputSectionDesc->dump(Outs);
}

bool OutputSectionEntry::insertAfterRule(RuleContainer *C, RuleContainer *R) {
  auto Iter = std::find(Inputs.begin(), Inputs.end(), C);
  if (Iter == Inputs.end())
    return false;
  ++Iter;
  Iter = Inputs.insert(Iter, R);
  R->setNextRule(C->getNextRule());
  C->setNextRule(R);
  return true;
}

bool OutputSectionEntry::insertBeforeRule(RuleContainer *C, RuleContainer *R) {
  auto Iter = std::find(Inputs.begin(), Inputs.end(), C);
  if (Iter == Inputs.end())
    return false;
  if (Iter != Inputs.begin()) {
    auto PrevIter = Iter - 1;
    (*PrevIter)->setNextRule(R);
  }
  Inputs.insert(Iter, R);
  R->setNextRule(C);
  return true;
}

std::string OutputSectionEntry::getSectionTypeStr() const {
  return ELFSection::getELFTypeStr(Name, OutputELFSection->getType()).str();
}
