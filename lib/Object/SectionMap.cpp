//===- SectionMap.cpp------------------------------------------------------===//
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
#include "eld/Config/LinkerConfig.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticInfos.h"
#include "eld/Input/ArchiveMemberInput.h"
#include "eld/Input/Input.h"
#include "eld/Input/ObjectFile.h"
#include "eld/Object/ObjectLinker.h"
#include "eld/PluginAPI/PluginBase.h"
#include "eld/Readers/CommonELFSection.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Readers/EhFrameHdrSection.h"
#include "eld/Readers/EhFrameSection.h"
#include "eld/Script/ExcludeFiles.h"
#include "eld/Script/OutputSectData.h"
#include "eld/Script/StringList.h"
#include "eld/Script/WildcardPattern.h"
#include "eld/Support/Memory.h"
#include "eld/Support/MsgHandling.h"
#include "eld/Target/ARMEXIDXSection.h"
#include "eld/Target/ELFSegment.h"
#include "eld/Target/GNULDBackend.h"
#include "llvm/ADT/Hashing.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/Casting.h"
#include <filesystem>
using namespace eld;
using namespace llvm;

SectionMap::SectionMap(LinkerScript &L, const LinkerConfig &Config,
                       LayoutInfo *LayoutInfo)
    : MLinkerScript(L), ThisConfig(Config),
      IsSectionTracingRequested(Config.options().isSectionTracingRequested()),
      MLayoutInfo(LayoutInfo) {
  insert("", "");
  MPrinter = Config.getPrinter();
}
SectionMap::~SectionMap() {
  MOutputSectionEntryDescList.clear();
  SpecialSections.clear();
  MEntrySections.clear();
}

SectionMap::mapping SectionMap::find(std::string PInputFile,
                                     std::string CurInputSection,
                                     bool IsArchive, std::string Name,
                                     uint64_t InputSectionHash,
                                     uint64_t InputFileHash, uint64_t NameHash,
                                     bool GNUCompatible, bool IsCommonSection) {
  iterator Out, OutBegin = begin(), OutEnd = end();
  for (Out = OutBegin; Out != OutEnd; ++Out) {
    OutputSectionEntry::iterator In, InBegin = (*Out)->begin(),
                                     InEnd = (*Out)->end();
    for (In = InBegin; In != InEnd; ++In) {
      if ((*In)->isSpecial())
        continue;
      if (matched(**In, nullptr, PInputFile, CurInputSection, IsArchive, Name,
                  InputSectionHash, InputFileHash, NameHash, GNUCompatible,
                  IsCommonSection))
        return std::make_pair(*Out, *In);
    }
  }
  auto Sp = SpecialSections.find(CurInputSection);
  if (Sp != SpecialSections.end())
    return std::make_pair(Sp->second.first, Sp->second.second);
  return std::make_pair(nullptr, nullptr);
}

// Find section by name.
ELFSection *SectionMap::find(std::string POutputSection) {
  iterator Out, OutBegin = begin(), OutEnd = end();
  for (Out = OutBegin; Out != OutEnd; ++Out) {
    if ((*Out)->name().compare(POutputSection) == 0)
      return (*Out)->getSection();
  }
  return nullptr;
}

// Find section by type.
ELFSection *SectionMap::find(uint32_t SectionType) {
  iterator Out, OutBegin = begin(), OutEnd = end();
  for (Out = OutBegin; Out != OutEnd; ++Out) {
    if ((*Out)->getSection()->getType() == SectionType)
      return (*Out)->getSection();
  }
  return nullptr;
}

SectionMap::iterator SectionMap::findIter(std::string POutputSection) {
  iterator Out, OutBegin = begin(), OutEnd = end();
  for (Out = OutBegin; Out != OutEnd; ++Out) {
    if ((*Out)->name().compare(POutputSection) == 0)
      return Out;
  }
  return OutEnd;
}

SectionMap::mapping SectionMap::findIn(SectionMap::iterator OutBegin,
                                       std::string PInputFile,
                                       const ELFSection &CurInputSection,
                                       bool IsArchive, std::string Name,
                                       uint64_t InputSectionHash,
                                       uint64_t InputFileHash,
                                       uint64_t NameHash, bool GNUCompatible) {
  bool IsCommonSection = llvm::isa<CommonELFSection>(&CurInputSection);
  iterator Out, OutEnd = end();
  for (Out = OutBegin; Out != OutEnd; ++Out) {
    OutputSectionEntry::iterator In, InBegin = (*Out)->begin(),
                                     InEnd = (*Out)->end();
    for (In = InBegin; In != InEnd; ++In) {
      if (matched(**In, nullptr, PInputFile, CurInputSection.name().str(),
                  IsArchive, Name, InputSectionHash, InputFileHash, NameHash,
                  GNUCompatible, IsCommonSection))
        return std::make_pair(*Out, *In);
    }
  }
  return std::make_pair(nullptr, nullptr);
}

SectionMap::mapping
SectionMap::findOnlyIn(SectionMap::iterator Out, std::string PInputFile,
                       const ELFSection &CurInputSection, bool IsArchive,
                       std::string Name, uint64_t InputSectionHash,
                       uint64_t InputFileHash, uint64_t NameHash,
                       bool GNUCompatible) {
  OutputSectionEntry::iterator In, InBegin = (*Out)->begin(),
                                   InEnd = (*Out)->end();
  bool IsCommonSection = llvm::isa<CommonELFSection>(&CurInputSection);
  RuleContainer *Special = nullptr;
  for (In = InBegin; In != InEnd; ++In) {
    if ((*In)->isSpecial())
      Special = (*In);
    if (matched(**In, nullptr, PInputFile, CurInputSection.name().str(),
                IsArchive, Name, InputSectionHash, InputFileHash, NameHash,
                GNUCompatible, IsCommonSection))
      return std::make_pair(*Out, *In);
  }
  // If nothing matches, match the special rule.
  return std::make_pair(*Out, Special);
}

std::pair<SectionMap::mapping, bool>
SectionMap::insert(std::string pInputSection, std::string pOutputSection,
                   InputSectDesc::Policy pPolicy) {
  iterator out, OutBegin = begin(), OutEnd = end();
  for (out = OutBegin; out != OutEnd; ++out) {
    if ((*out)->name().compare(pOutputSection) == 0)
      break;
  }
  if (out != end()) {
    OutputSectionEntry::iterator In, InBegin = (*out)->begin(),
                                     InEnd = (*out)->end();
    for (In = InBegin; In != InEnd; ++In) {
      if ((*In)->getSection()->name().compare(pInputSection) == 0)
        break;
    }

    if (In != (*out)->end()) {
      if (IsSectionTracingRequested &&
          ThisConfig.options().traceSection((*In)->getSection())) {
        ThisConfig.raise(Diag::section_mapping_info)
            << (*In)->getSection()->getDecoratedName(ThisConfig.options())
            << (*out)->getSection()->getDecoratedName(ThisConfig.options());
      }

      return std::make_pair(std::make_pair(*out, *In), false);
    }
    auto input = make<RuleContainer>(this, pInputSection, pPolicy);
    input->getSection()->setOutputSection(*out);
    if (IsSectionTracingRequested &&
        ThisConfig.options().traceSection(input->getSection())) {
      ThisConfig.raise(Diag::section_mapping_info)
          << input->getSection()->getDecoratedName(ThisConfig.options())
          << (*out)->getSection()->getDecoratedName(ThisConfig.options());
    }
      input->getSection()->setMatchedLinkerScriptRule(input);
      (*out)->append(input);

      if ((*out)->getLastRule())
        (*out)->getLastRule()->setNextRule(input);
      (*out)->setLastRule(input);

      if ((pPolicy == InputSectDesc::SpecialKeep) ||
          (pPolicy == InputSectDesc::SpecialNoKeep))
        SpecialSections[pOutputSection] = std::make_pair(*out, input);
      return std::make_pair(std::make_pair(*out, input), true);
  }

  auto *Output = make<OutputSectionEntry>(this, pOutputSection);
  MOutputSectionEntryDescList.push_back(Output);
  auto *Input = make<RuleContainer>(this, pInputSection, pPolicy);
  Input->getSection()->setOutputSection(Output);
  if (IsSectionTracingRequested &&
      ThisConfig.options().traceSection(Input->getSection())) {
    ThisConfig.raise(Diag::section_mapping_info)
        << Input->getSection()->getDecoratedName(ThisConfig.options())
        << Output->getSection()->getDecoratedName(ThisConfig.options());
  }
  Input->getSection()->setMatchedLinkerScriptRule(Input);
  Output->append(Input);

  if (Output->getLastRule())
    Output->getLastRule()->setNextRule(Input);
  Output->setLastRule(Input);

  if ((pPolicy == InputSectDesc::SpecialKeep) ||
      (pPolicy == InputSectDesc::SpecialNoKeep)) {
    SpecialSections[pOutputSection] = std::make_pair(Output, Input);
  }

  return std::make_pair(std::make_pair(Output, Input), true);
}

std::pair<SectionMap::mapping, bool>
SectionMap::insert(const InputSectDesc &pInputDesc,
                   OutputSectDesc &POutputDesc) {
  iterator out, OutBegin = begin(), OutEnd = end();
  for (out = OutBegin; out != OutEnd; ++out) {
    if ((*out)->name().compare(POutputDesc.name()) == 0 &&
        (*out)->prolog() == POutputDesc.prolog() &&
        (*out)->epilog() == POutputDesc.epilog())
      break;
  }
  if (out != end()) {
    bool IsOutputSectionData = llvm::isa<OutputSectData>(&pInputDesc);

    OutputSectionEntry::iterator In, InBegin = (*out)->begin(),
                                     InEnd = (*out)->end();

    // We always need to create a new rule for explicit output
    // section data to ensure correct placement.
    In = (IsOutputSectionData ? InEnd : InBegin);

    // FIXME: Report a warning (-Wlinker-script) if an output section
    // description contain multiple same input section descriptions.
    for (; In != InEnd; ++In) {
      if ((*In)->policy() == pInputDesc.policy() &&
          (*In)->spec() == pInputDesc.spec())
        break;
    }

    if (In != (*out)->end()) {
      if (IsSectionTracingRequested &&
          ThisConfig.options().traceSection((*In)->getSection())) {
        ThisConfig.raise(Diag::section_mapping_info)
            << (*In)->getSection()->getDecoratedName(ThisConfig.options())
            << (*out)->getSection()->getDecoratedName(ThisConfig.options());
      }
      return std::make_pair(std::make_pair(*out, *In), false);
    }
    auto input = make<RuleContainer>(this, pInputDesc);
    input->getSection()->setOutputSection(*out);
    if (IsSectionTracingRequested &&
        ThisConfig.options().traceSection(input->getSection())) {
      ThisConfig.raise(Diag::section_mapping_info)
          << input->getSection()->getDecoratedName(ThisConfig.options())
          << (*out)->getSection()->getDecoratedName(ThisConfig.options());
    }
      input->getSection()->setMatchedLinkerScriptRule(input);
      if (pInputDesc.isSpecial())
        SpecialSections[(*out)->name().str()] = std::make_pair(*out, input);
      (*out)->append(input);
      if ((*out)->getLastRule())
        (*out)->getLastRule()->setNextRule(input);
      (*out)->setLastRule(input);
      return std::make_pair(std::make_pair(*out, input), true);
  }

  auto *Output = make<OutputSectionEntry>(this, POutputDesc);
  MOutputSectionEntryDescList.push_back(Output);
  auto *Input = make<RuleContainer>(this, pInputDesc);

  Input->getSection()->setOutputSection(Output);
  if (IsSectionTracingRequested &&
      ThisConfig.options().traceSection(Input->getSection())) {
    ThisConfig.raise(Diag::section_mapping_info)
        << Input->getSection()->getDecoratedName(ThisConfig.options())
        << Output->getSection()->getDecoratedName(ThisConfig.options());
  }
  Input->getSection()->setMatchedLinkerScriptRule(Input);
  Output->append(Input);

  if (Output->getLastRule())
    Output->getLastRule()->setNextRule(Input);
  Output->setLastRule(Input);

  if (pInputDesc.isSpecial()) {
    SpecialSections[Output->name().str()] = std::make_pair(Output, Input);
  }

  return std::make_pair(std::make_pair(Output, Input), true);
}

SectionMap::iterator SectionMap::insert(iterator PPosition,
                                        ELFSection *PSection) {
  auto *Output = make<OutputSectionEntry>(this, PSection->name().str());
  auto *Input =
      make<RuleContainer>(this, PSection->name().str(), InputSectDesc::NoKeep);
  Input->getSection()->setOutputSection(Output);
  if (IsSectionTracingRequested &&
      ThisConfig.options().traceSection(Input->getSection())) {
    ThisConfig.raise(Diag::section_mapping_info)
        << Input->getSection()->getDecoratedName(ThisConfig.options())
        << Output->getSection()->getDecoratedName(ThisConfig.options());
  }
  Input->getSection()->setMatchedLinkerScriptRule(Input);
  Output->append(Input);

  if (Output->getLastRule())
    Output->getLastRule()->setNextRule(Input);
  Output->setLastRule(Input);

  Output->setSection(PSection);
  return MOutputSectionEntryDescList.insert(PPosition, Output);
}

SectionMap::iterator SectionMap::insert(iterator PPosition,
                                        OutputSectionEntry *PSection) {
  return MOutputSectionEntryDescList.insert(PPosition, PSection);
}

// pInputFile is the full path used in archive match and
// name is the name of object
bool SectionMap::matched(const RuleContainer &PInput, InputFile *I,
                         std::string const &PInputFile,
                         std::string const &CurInputSection, bool IsArchive,
                         std::string const &Name, uint64_t InputSectionHash,
                         uint64_t FileNameHash, uint64_t NameHash,
                         bool GNUCompatible, bool IsCommonSection,
                         bool StorePatterns) const {

  bool MatchedArchiveMember = false;

  if (PInput.spec().isArchive()) {
    if (!IsArchive)
      return false;
    if (PInput.spec().hasArchiveMember()) {
      const WildcardPattern &ArchiveMemPattern = PInput.spec().archiveMember();
      bool Res = false;
      if (StorePatterns && I &&
          I->getInput()->findMemberMatchedPattern(
              PInput.spec().getArchiveMember(), Res)) {
        if (!Res)
          return false;
      } else {
        const ArchiveMemberInput *AMI =
            llvm::dyn_cast<ArchiveMemberInput>(I->getInput());
        if (!Res && AMI && AMI->getArchiveFile()->isThin() &&
            ThisConfig.options()
                .isThinArchiveRuleMatchingCompatibilityEnabled()) {
          std::string Filename =
              std::filesystem::path(Name).filename().string();
          llvm::hash_code FilenameHash = Input::computeFilePathHash(Filename);
          Res = matched(ArchiveMemPattern, Filename, FilenameHash);
        } else
          Res = matched(ArchiveMemPattern, Name, NameHash);
        if (I && StorePatterns)
          I->getInput()->addMemberMatchedPattern(
              PInput.spec().getArchiveMember(), Res);
        if (!Res)
          return false;
      }
    }
  }

  if (IsArchive && PInput.spec().hasFile()) {
    bool Res = false;
    if (StorePatterns && I &&
        I->getInput()->findMemberMatchedPattern(PInput.spec().getFile(), Res))
      MatchedArchiveMember = Res;
    else {
      Res = matched(PInput.spec().file(), Name, NameHash);
      if (I && StorePatterns)
        I->getInput()->addMemberMatchedPattern(PInput.spec().getFile(), Res);
    }
    MatchedArchiveMember = Res;
  }

  if (!MatchedArchiveMember && PInput.spec().hasFile()) {
    bool Res = false;
    if (I &&
        I->getInput()->findFileMatchedPattern(PInput.spec().getFile(), Res)) {
      if (!Res)
        return false;
    } else {
      Res = matched(PInput.spec().file(), PInputFile, FileNameHash);
      if (I && StorePatterns)
        I->getInput()->addFileMatchedPattern(PInput.spec().getFile(), Res);
    }
    if (!Res)
      return false;
  }

  bool DoOnce = false;
  bool MatchedRule = false;

  if (PInput.spec().hasSections()) {
    StringList::const_iterator Sect, SectEnd = PInput.spec().sections().end();
    for (Sect = PInput.spec().sections().begin(); Sect != SectEnd; ++Sect) {
      const WildcardPattern &Pattern = llvm::cast<WildcardPattern>(**Sect);
      ExcludeFiles EF(PInput.spec().getExcludeFiles(), Pattern.excludeFiles());
      if (!EF.empty() && !DoOnce) {
        // Try to do only for Archive files and Archive patterns. If the Spec is
        // an archive and this matches any archive member, just reject and move
        // on to the next rule.
        DoOnce = true;
        if (PInput.spec().isArchive() && IsArchive && MatchedArchiveMember)
          MatchedRule = true;
      }

      // Match the Input file with the pattern and the Archive member with the
      // pattern, if both match only then try to match the section.
      if (!EF.empty() && DoOnce && MatchedRule)
        return false;

      if (GNUCompatible && EF.empty() && PInput.spec().hasFile() &&
          PInput.spec().file().name() != "*" && IsArchive &&
          !PInput.spec().isArchive())
        return false;

      if (matchedSections(I, PInput.spec().file(), Pattern, Name, PInputFile,
                          CurInputSection, IsArchive, InputSectionHash,
                          FileNameHash, NameHash, IsCommonSection, EF,
                          StorePatterns)) {
        return true;
      }
    }
  }

  return false;
}

WildcardPattern SectionMap::getWithSyntacticSugarForCommonPattern(
    const WildcardPattern &Pattern) {
  WildcardPattern UpdatedPattern =
      llvm::StringSwitch<WildcardPattern>(Pattern.name())
          .Case("COMMON", WildcardPattern("COMMON.*", Pattern.sortPolicy(),
                                          /*pExcludeFileList=*/nullptr))
          .Case(".scommon.1",
                WildcardPattern(".scommon.1.*", Pattern.sortPolicy(),
                                /*pExcludeFileList=*/nullptr))
          .Case(".scommon.2",
                WildcardPattern(".scommon.2.*", Pattern.sortPolicy(),
                                /*pExcludeFileList=*/nullptr))
          .Case(".scommon.4",
                WildcardPattern(".scommon.4.*", Pattern.sortPolicy(),
                                /*pExcludeFileList=*/nullptr))
          .Case(".scommon.8",
                WildcardPattern(".scommon.8.*", Pattern.sortPolicy(),
                                /*pExcludeFileList=*/nullptr))
          .Default(Pattern);

  return UpdatedPattern;
}

// Match the wild section with the input section name.
// pPattern - The wildcard from input descriptor in linker script
// pName - The actual name of the file
// pFilename - full native filesystem path name for archive match
// pInputSection - the section we are trying to merge/find
// isArchive - used to denote if the file we are matching against is archive.
bool SectionMap::matchedSections(
    InputFile *I, const WildcardPattern &InputFilePattern,
    const WildcardPattern &PPattern, std::string const &PName,
    std::string const &PFilename, std::string const &CurInputSection,
    bool IsArchive, uint64_t InputSectionHash, uint64_t FileNameHash,
    uint64_t NameHash, bool IsCommonSection, const ExcludeFiles &EF,
    bool StorePatterns) const {
  WildcardPattern Pattern = PPattern;
  if (IsCommonSection)
    Pattern = getWithSyntacticSugarForCommonPattern(Pattern);
  bool Match = matched(Pattern, CurInputSection, InputSectionHash);
  if (Match) {
    if (!EF.empty()) {
      for (const auto &Elem : EF) {
        const WildcardPattern *ArchivePattern = Elem->archive();
        bool ArchiveMatchWithFileName = false;
        if (ArchivePattern) {
          bool Res = false;
          if (I && I->getInput()->findFileMatchedPattern(ArchivePattern, Res))
            ArchiveMatchWithFileName = Res;
          else {
            ArchiveMatchWithFileName =
                matched(*ArchivePattern, PFilename, FileNameHash);
            if (I && StorePatterns)
              I->getInput()->addFileMatchedPattern(ArchivePattern,
                                                   ArchiveMatchWithFileName);
          }
        }

        bool FileMatchWithMemberName = false;
        bool FileMatchWithFileName = false;
        const WildcardPattern *FilePattern = Elem->file();
        if (FilePattern) {
          bool Res = false;
          if (StorePatterns && I &&
              I->getInput()->findMemberMatchedPattern(FilePattern, Res))
            FileMatchWithMemberName = Res;
          else {
            FileMatchWithMemberName = matched(*FilePattern, PName, NameHash);
            if (I && StorePatterns)
              I->getInput()->addMemberMatchedPattern(FilePattern,
                                                     FileMatchWithMemberName);
          }
          if (I && I->getInput()->findFileMatchedPattern(FilePattern, Res))
            FileMatchWithFileName = Res;
          else {
            FileMatchWithFileName =
                matched(*FilePattern, PFilename, FileNameHash);
            if (I && StorePatterns)
              I->getInput()->addFileMatchedPattern(FilePattern,
                                                   FileMatchWithFileName);
          }
        }
        // It matches patterns such as: '<ArchivePattern>:<MemberPattern>'
        if (IsArchive && (Elem)->isFileInArchive() &&
            ArchiveMatchWithFileName && FileMatchWithMemberName)
          return false;

        // It matches patterns such as: '<ArchivePattern>:'
        if (IsArchive && (Elem)->isArchive() && ArchiveMatchWithFileName &&
            !(Elem)->isFileInArchive())
          return false;

        // It matches patterns such as: '<MemberPattern>'
        if (IsArchive && (Elem)->file() && !Elem->isArchive() &&
            FileMatchWithMemberName)
          return false;

        // Match archive name because script don't understand what is archive.
        if ((Elem)->file() && FileMatchWithFileName)
          return false;
      }
    }
  }
  return Match;
}

bool SectionMap::matched(const WildcardPattern &PPattern, StringRef PName,
                         uint64_t NameHash) const {
  // If the pattern to search doesnot contain any wildcards, then
  // just use the hash to compare.
  if (PPattern.hasHash())
    return NameHash == PPattern.hashValue();
  return PPattern.matched(PName, NameHash);
}

SectionMap::OutputSectionEntryDescList
SectionMap::getOutputSectionEntrySectionsForPluginType(plugin::Plugin::Type T) {
  OutputSectionEntryDescList OList;

  for (auto &O : *this) {
    eld::Plugin *P = O->prolog().getPlugin();
    if (!P)
      continue;
    if (P->getType() != T)
      continue;
    OList.push_back(O);
  }
  return OList;
}

ELFSection *SectionMap::createOutputSectionEntry(std::string Section,
                                                 LDFileFormat::Kind Kind,
                                                 uint32_t Type, uint32_t Flag,
                                                 uint32_t Align) {
  OutputSectionEntry *Output =
      make<OutputSectionEntry>(this, Section, Kind, Type, Flag, Align);
  return Output->getSection();
}

ELFSection *SectionMap::createELFSection(const std::string &Name,
                                         LDFileFormat::Kind K, uint32_t Type,
                                         uint32_t Flags, uint32_t EntSize) {
  if (Type == llvm::ELF::SHT_ARM_EXIDX && K == LDFileFormat::Target)
    return make<ARMEXIDXSection>(Name, Flags, EntSize, /*Size=*/0, /*PAddr=*/0);
  return make<ELFSection>(K, Name, Flags, EntSize, /*AddrAlign=*/0, Type,
                          /*Info=*/0,
                          /*Link=*/nullptr,
                          /*Size=*/0, /*PAddr=*/0);
}

ELFSection *SectionMap::createEhFrameSection(std::string Section, uint32_t Type,
                                             uint32_t Flag, uint32_t EntSize,
                                             uint64_t Size) {
  return make<EhFrameSection>(Section, ThisConfig.getDiagEngine(), Type, Flag,
                              EntSize, Size);
}

EhFrameHdrSection *SectionMap::createEhFrameHdrSection(std::string Section,
                                                       uint32_t Type,
                                                       uint32_t Flag,
                                                       uint32_t EntSize,
                                                       uint64_t Size) {
  return make<EhFrameHdrSection>(Section, Type, Flag, EntSize, Size);
}

OutputSectionEntry *
SectionMap::findOutputSectionEntry(const std::string &Name) {
  auto It = std::find_if(
      MOutputSectionEntryDescList.begin(), MOutputSectionEntryDescList.end(),
      [Name](const OutputSectionEntry *O) { return O->name() == Name; });
  if (It == MOutputSectionEntryDescList.end())
    return nullptr;
  return *It;
}

bool SectionMap::doesRuleMatchWithSection(const RuleContainer &R,
                                          const Section &S,
                                          bool DoNotUseRmName) const {
  InputFile *IF = S.getInputFile();
  ASSERT(IF != nullptr, "Section must always have a non-empty InputFile!");
  bool IsCommonSection = false;
  if (const CommonELFSection *CommonSection =
          llvm::dyn_cast<const CommonELFSection>(&S)) {
    IF = CommonSection->getOrigin();
    IsCommonSection = true;
  }
  if (S.getOldInputFile())
    IF = S.getOldInputFile();
  const std::string &InputFileName = IF->getInput()->getResolvedPath().native();
  std::string SectName = S.name().str();
  uint64_t SectHash = 0;
  if (!DoNotUseRmName) {
    ObjectFile *OF = llvm::dyn_cast<ObjectFile>(IF);
    const ELFSection *ESect = llvm::dyn_cast<const ELFSection>(&S);
    if (OF && ESect) {
      if (auto OptRmSectName = OF->getRuleMatchingSectName(ESect->getIndex())) {
        SectName = OptRmSectName.value();
        SectHash = llvm::hash_combine(SectName);
      }
    }
  }
  bool IsArchive = IF->isArchive() ||
                   llvm::dyn_cast<eld::ArchiveMemberInput>(IF->getInput());
  uint64_t InputFileHash = IF->getInput()->getResolvedPathHash();
  uint64_t ArchiveMemNameHash = IF->getInput()->getArchiveMemberNameHash();
  return matched(
      R, IF, InputFileName, SectName, IsArchive, IF->getInput()->getName(),
      SectHash, InputFileHash, ArchiveMemNameHash,
      (ThisConfig.options().getScriptOption() == GeneralOptions::MatchGNU),
      IsCommonSection, /*storePatterns=*/false);
}
