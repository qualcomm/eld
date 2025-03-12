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
                       LayoutPrinter *layoutPrinter)
    : m_LinkerScript(L), m_LinkerConfig(Config),
      m_IsSectionTracingRequested(Config.options().isSectionTracingRequested()),
      m_LayoutPrinter(layoutPrinter) {
  insert("", "");
  m_Printer = Config.getPrinter();
}
SectionMap::~SectionMap() {
  m_OutputSectionEntryDescList.clear();
  _specialSections.clear();
  m_entrySections.clear();
}

SectionMap::mapping SectionMap::find(std::string pInputFile,
                                     std::string pInputSection, bool isArchive,
                                     std::string name,
                                     uint64_t inputSectionHash,
                                     uint64_t inputFileHash, uint64_t nameHash,
                                     bool GNUCompatible, bool isCommonSection) {
  iterator out, outBegin = begin(), outEnd = end();
  for (out = outBegin; out != outEnd; ++out) {
    OutputSectionEntry::iterator in, inBegin = (*out)->begin(),
                                     inEnd = (*out)->end();
    for (in = inBegin; in != inEnd; ++in) {
      if ((*in)->isSpecial())
        continue;
      if (matched(**in, nullptr, pInputFile, pInputSection, isArchive, name,
                  inputSectionHash, inputFileHash, nameHash, GNUCompatible,
                  isCommonSection))
        return std::make_pair(*out, *in);
    }
  }
  auto sp = _specialSections.find(pInputSection);
  if (sp != _specialSections.end())
    return std::make_pair(sp->second.first, sp->second.second);
  return std::make_pair(nullptr, nullptr);
}

// Find section by name.
ELFSection *SectionMap::find(std::string pOutputSection) {
  iterator out, outBegin = begin(), outEnd = end();
  for (out = outBegin; out != outEnd; ++out) {
    if ((*out)->name().compare(pOutputSection) == 0)
      return (*out)->getSection();
  }
  return nullptr;
}

// Find section by type.
ELFSection *SectionMap::find(uint32_t SectionType) {
  iterator out, outBegin = begin(), outEnd = end();
  for (out = outBegin; out != outEnd; ++out) {
    if ((*out)->getSection()->getType() == SectionType)
      return (*out)->getSection();
  }
  return nullptr;
}

SectionMap::iterator SectionMap::findIter(std::string pOutputSection) {
  iterator out, outBegin = begin(), outEnd = end();
  for (out = outBegin; out != outEnd; ++out) {
    if ((*out)->name().compare(pOutputSection) == 0)
      return out;
  }
  return outEnd;
}

SectionMap::mapping SectionMap::findIn(SectionMap::iterator outBegin,
                                       std::string pInputFile,
                                       const ELFSection &pInputSection,
                                       bool isArchive, std::string name,
                                       uint64_t inputSectionHash,
                                       uint64_t inputFileHash,
                                       uint64_t nameHash, bool GNUCompatible) {
  bool isCommonSection = llvm::isa<CommonELFSection>(&pInputSection);
  iterator out, outEnd = end();
  for (out = outBegin; out != outEnd; ++out) {
    OutputSectionEntry::iterator in, inBegin = (*out)->begin(),
                                     inEnd = (*out)->end();
    for (in = inBegin; in != inEnd; ++in) {
      if (matched(**in, nullptr, pInputFile, pInputSection.name().str(),
                  isArchive, name, inputSectionHash, inputFileHash, nameHash,
                  GNUCompatible, isCommonSection))
        return std::make_pair(*out, *in);
    }
  }
  return std::make_pair(nullptr, nullptr);
}

SectionMap::mapping
SectionMap::findOnlyIn(SectionMap::iterator out, std::string pInputFile,
                       const ELFSection &pInputSection, bool isArchive,
                       std::string name, uint64_t inputSectionHash,
                       uint64_t inputFileHash, uint64_t nameHash,
                       bool GNUCompatible) {
  OutputSectionEntry::iterator in, inBegin = (*out)->begin(),
                                   inEnd = (*out)->end();
  bool isCommonSection = llvm::isa<CommonELFSection>(&pInputSection);
  RuleContainer *special = nullptr;
  for (in = inBegin; in != inEnd; ++in) {
    if ((*in)->isSpecial())
      special = (*in);
    if (matched(**in, nullptr, pInputFile, pInputSection.name().str(),
                isArchive, name, inputSectionHash, inputFileHash, nameHash,
                GNUCompatible, isCommonSection))
      return std::make_pair(*out, *in);
  }
  // If nothing matches, match the special rule.
  return std::make_pair(*out, special);
}

std::pair<SectionMap::mapping, bool>
SectionMap::insert(std::string pInputSection, std::string pOutputSection,
                   InputSectDesc::Policy pPolicy) {
  iterator out, outBegin = begin(), outEnd = end();
  for (out = outBegin; out != outEnd; ++out) {
    if ((*out)->name().compare(pOutputSection) == 0)
      break;
  }
  if (out != end()) {
    OutputSectionEntry::iterator in, inBegin = (*out)->begin(),
                                     inEnd = (*out)->end();
    for (in = inBegin; in != inEnd; ++in) {
      if ((*in)->getSection()->name().compare(pInputSection) == 0)
        break;
    }

    if (in != (*out)->end()) {
      if (m_IsSectionTracingRequested &&
          m_LinkerConfig.options().traceSection((*in)->getSection())) {
        m_LinkerConfig.raise(diag::section_mapping_info)
            << (*in)->getSection()->getDecoratedName(m_LinkerConfig.options())
            << (*out)->getSection()->getDecoratedName(m_LinkerConfig.options());
      }

      return std::make_pair(std::make_pair(*out, *in), false);
    } else {
      auto input = make<RuleContainer>(this, pInputSection, pPolicy);
      input->getSection()->setOutputSection(*out);
      if (m_IsSectionTracingRequested &&
          m_LinkerConfig.options().traceSection(input->getSection())) {
        m_LinkerConfig.raise(diag::section_mapping_info)
            << input->getSection()->getDecoratedName(m_LinkerConfig.options())
            << (*out)->getSection()->getDecoratedName(m_LinkerConfig.options());
      }
      input->getSection()->setMatchedLinkerScriptRule(input);
      (*out)->append(input);

      if ((*out)->getLastRule())
        (*out)->getLastRule()->setNextRule(input);
      (*out)->setLastRule(input);

      if ((pPolicy == InputSectDesc::SpecialKeep) ||
          (pPolicy == InputSectDesc::SpecialNoKeep))
        _specialSections[pOutputSection] = std::make_pair(*out, input);
      return std::make_pair(std::make_pair(*out, input), true);
    }
  }

  auto output = make<OutputSectionEntry>(this, pOutputSection);
  m_OutputSectionEntryDescList.push_back(output);
  auto input = make<RuleContainer>(this, pInputSection, pPolicy);
  input->getSection()->setOutputSection(output);
  if (m_IsSectionTracingRequested &&
      m_LinkerConfig.options().traceSection(input->getSection())) {
    m_LinkerConfig.raise(diag::section_mapping_info)
        << input->getSection()->getDecoratedName(m_LinkerConfig.options())
        << output->getSection()->getDecoratedName(m_LinkerConfig.options());
  }
  input->getSection()->setMatchedLinkerScriptRule(input);
  output->append(input);

  if (output->getLastRule())
    output->getLastRule()->setNextRule(input);
  output->setLastRule(input);

  if ((pPolicy == InputSectDesc::SpecialKeep) ||
      (pPolicy == InputSectDesc::SpecialNoKeep)) {
    _specialSections[pOutputSection] = std::make_pair(output, input);
  }

  return std::make_pair(std::make_pair(output, input), true);
}

std::pair<SectionMap::mapping, bool>
SectionMap::insert(const InputSectDesc &pInputDesc,
                   OutputSectDesc &pOutputDesc) {
  iterator out, outBegin = begin(), outEnd = end();
  for (out = outBegin; out != outEnd; ++out) {
    if ((*out)->name().compare(pOutputDesc.name()) == 0 &&
        (*out)->prolog() == pOutputDesc.prolog() &&
        (*out)->epilog() == pOutputDesc.epilog())
      break;
  }
  if (out != end()) {
    bool isOutputSectionData = llvm::isa<OutputSectData>(&pInputDesc);

    OutputSectionEntry::iterator in, inBegin = (*out)->begin(),
                                     inEnd = (*out)->end();

    // We always need to create a new rule for explicit output
    // section data to ensure correct placement.
    in = (isOutputSectionData ? inEnd : inBegin);

    // FIXME: Report a warning (-Wlinker-script) if an output section
    // description contain multiple same input section descriptions.
    for (; in != inEnd; ++in) {
      if ((*in)->policy() == pInputDesc.policy() &&
          (*in)->spec() == pInputDesc.spec())
        break;
    }

    if (in != (*out)->end()) {
      if (m_IsSectionTracingRequested &&
          m_LinkerConfig.options().traceSection((*in)->getSection())) {
        m_LinkerConfig.raise(diag::section_mapping_info)
            << (*in)->getSection()->getDecoratedName(m_LinkerConfig.options())
            << (*out)->getSection()->getDecoratedName(m_LinkerConfig.options());
      }
      return std::make_pair(std::make_pair(*out, *in), false);
    } else {
      auto input = make<RuleContainer>(this, pInputDesc);
      input->getSection()->setOutputSection(*out);
      if (m_IsSectionTracingRequested &&
          m_LinkerConfig.options().traceSection(input->getSection())) {
        m_LinkerConfig.raise(diag::section_mapping_info)
            << input->getSection()->getDecoratedName(m_LinkerConfig.options())
            << (*out)->getSection()->getDecoratedName(m_LinkerConfig.options());
      }
      input->getSection()->setMatchedLinkerScriptRule(input);
      if (pInputDesc.isSpecial())
        _specialSections[(*out)->name().str()] = std::make_pair(*out, input);
      (*out)->append(input);
      if ((*out)->getLastRule())
        (*out)->getLastRule()->setNextRule(input);
      (*out)->setLastRule(input);
      return std::make_pair(std::make_pair(*out, input), true);
    }
  }

  auto output = make<OutputSectionEntry>(this, pOutputDesc);
  m_OutputSectionEntryDescList.push_back(output);
  auto input = make<RuleContainer>(this, pInputDesc);

  input->getSection()->setOutputSection(output);
  if (m_IsSectionTracingRequested &&
      m_LinkerConfig.options().traceSection(input->getSection())) {
    m_LinkerConfig.raise(diag::section_mapping_info)
        << input->getSection()->getDecoratedName(m_LinkerConfig.options())
        << output->getSection()->getDecoratedName(m_LinkerConfig.options());
  }
  input->getSection()->setMatchedLinkerScriptRule(input);
  output->append(input);

  if (output->getLastRule())
    output->getLastRule()->setNextRule(input);
  output->setLastRule(input);

  if (pInputDesc.isSpecial()) {
    _specialSections[output->name().str()] = std::make_pair(output, input);
  }

  return std::make_pair(std::make_pair(output, input), true);
}

SectionMap::iterator SectionMap::insert(iterator pPosition,
                                        ELFSection *pSection) {
  auto output = make<OutputSectionEntry>(this, pSection->name().str());
  auto input =
      make<RuleContainer>(this, pSection->name().str(), InputSectDesc::NoKeep);
  input->getSection()->setOutputSection(output);
  if (m_IsSectionTracingRequested &&
      m_LinkerConfig.options().traceSection(input->getSection())) {
    m_LinkerConfig.raise(diag::section_mapping_info)
        << input->getSection()->getDecoratedName(m_LinkerConfig.options())
        << output->getSection()->getDecoratedName(m_LinkerConfig.options());
  }
  input->getSection()->setMatchedLinkerScriptRule(input);
  output->append(input);

  if (output->getLastRule())
    output->getLastRule()->setNextRule(input);
  output->setLastRule(input);

  output->setSection(pSection);
  return m_OutputSectionEntryDescList.insert(pPosition, output);
}

SectionMap::iterator SectionMap::insert(iterator pPosition,
                                        OutputSectionEntry *pSection) {
  return m_OutputSectionEntryDescList.insert(pPosition, pSection);
}

// pInputFile is the full path used in archive match and
// name is the name of object
bool SectionMap::matched(const RuleContainer &pInput, InputFile *I,
                         std::string const &pInputFile,
                         std::string const &pInputSection, bool isArchive,
                         std::string const &name, uint64_t inputSectionHash,
                         uint64_t fileNameHash, uint64_t nameHash,
                         bool GNUCompatible, bool isCommonSection,
                         bool storePatterns) const {

  bool matchedArchiveMember = false;

  if (pInput.spec().isArchive()) {
    if (!isArchive)
      return false;
    if (pInput.spec().hasArchiveMember()) {
      const WildcardPattern &archiveMemPattern = pInput.spec().archiveMember();
      bool res = false;
      if (storePatterns && I &&
          I->getInput()->findMemberMatchedPattern(
              pInput.spec().getArchiveMember(), res)) {
        if (!res)
          return false;
      } else {
        const ArchiveMemberInput *AMI =
            llvm::dyn_cast<ArchiveMemberInput>(I->getInput());
        if (!res && AMI && AMI->getArchiveFile()->isThin() &&
            m_LinkerConfig.options()
                .isThinArchiveRuleMatchingCompatibilityEnabled()) {
          std::string filename =
              std::filesystem::path(name).filename().string();
          llvm::hash_code filenameHash = Input::computeFilePathHash(filename);
          res = matched(archiveMemPattern, filename, filenameHash);
        } else
          res = matched(archiveMemPattern, name, nameHash);
        if (I && storePatterns)
          I->getInput()->addMemberMatchedPattern(
              pInput.spec().getArchiveMember(), res);
        if (!res)
          return false;
      }
    }
  }

  if (isArchive && pInput.spec().hasFile()) {
    bool res = false;
    if (storePatterns && I &&
        I->getInput()->findMemberMatchedPattern(pInput.spec().getFile(), res))
      matchedArchiveMember = res;
    else {
      res = matched(pInput.spec().file(), name, nameHash);
      if (I && storePatterns)
        I->getInput()->addMemberMatchedPattern(pInput.spec().getFile(), res);
    }
    matchedArchiveMember = res;
  }

  if (!matchedArchiveMember && pInput.spec().hasFile()) {
    bool res = false;
    if (I &&
        I->getInput()->findFileMatchedPattern(pInput.spec().getFile(), res)) {
      if (!res)
        return false;
    } else {
      res = matched(pInput.spec().file(), pInputFile, fileNameHash);
      if (I && storePatterns)
        I->getInput()->addFileMatchedPattern(pInput.spec().getFile(), res);
    }
    if (!res)
      return false;
  }

  bool doOnce = false;
  bool matchedRule = false;

  if (pInput.spec().hasSections()) {
    StringList::const_iterator sect, sectEnd = pInput.spec().sections().end();
    for (sect = pInput.spec().sections().begin(); sect != sectEnd; ++sect) {
      const WildcardPattern &pattern = llvm::cast<WildcardPattern>(**sect);
      ExcludeFiles EF(pInput.spec().getExcludeFiles(), pattern.excludeFiles());
      if (!EF.empty() && !doOnce) {
        // Try to do only for Archive files and Archive patterns. If the Spec is
        // an archive and this matches any archive member, just reject and move
        // on to the next rule.
        doOnce = true;
        if (pInput.spec().isArchive() && isArchive && matchedArchiveMember)
          matchedRule = true;
      }

      // Match the Input file with the pattern and the Archive member with the
      // pattern, if both match only then try to match the section.
      if (!EF.empty() && doOnce && matchedRule)
        return false;

      if (GNUCompatible && EF.empty() && pInput.spec().hasFile() &&
          pInput.spec().file().name() != "*" && isArchive &&
          !pInput.spec().isArchive())
        return false;

      if (matched_sections(I, pInput.spec().file(), pattern, name, pInputFile,
                           pInputSection, isArchive, inputSectionHash,
                           fileNameHash, nameHash, isCommonSection, EF,
                           storePatterns)) {
        return true;
      }
    }
  }

  return false;
}

WildcardPattern SectionMap::getWithSyntacticSugarForCommonPattern(
    const WildcardPattern &pattern) {
  WildcardPattern updatedPattern =
      llvm::StringSwitch<WildcardPattern>(pattern.name())
          .Case("COMMON", WildcardPattern("COMMON.*", pattern.sortPolicy(),
                                          /*pExcludeFileList=*/nullptr))
          .Case(".scommon.1",
                WildcardPattern(".scommon.1.*", pattern.sortPolicy(),
                                /*pExcludeFileList=*/nullptr))
          .Case(".scommon.2",
                WildcardPattern(".scommon.2.*", pattern.sortPolicy(),
                                /*pExcludeFileList=*/nullptr))
          .Case(".scommon.4",
                WildcardPattern(".scommon.4.*", pattern.sortPolicy(),
                                /*pExcludeFileList=*/nullptr))
          .Case(".scommon.8",
                WildcardPattern(".scommon.8.*", pattern.sortPolicy(),
                                /*pExcludeFileList=*/nullptr))
          .Default(pattern);

  return updatedPattern;
}

// Match the wild section with the input section name.
// pPattern - The wildcard from input descriptor in linker script
// pName - The actual name of the file
// pFilename - full native filesystem path name for archive match
// pInputSection - the section we are trying to merge/find
// isArchive - used to denote if the file we are matching against is archive.
bool SectionMap::matched_sections(
    InputFile *I, const WildcardPattern &inputFilePattern,
    const WildcardPattern &pPattern, std::string const &pName,
    std::string const &pFilename, std::string const &pInputSection,
    bool isArchive, uint64_t inputSectionHash, uint64_t fileNameHash,
    uint64_t nameHash, bool isCommonSection, const ExcludeFiles &EF,
    bool storePatterns) const {
  WildcardPattern pattern = pPattern;
  if (isCommonSection)
    pattern = getWithSyntacticSugarForCommonPattern(pattern);
  bool match = matched(pattern, pInputSection, inputSectionHash);
  if (match) {
    if (!EF.empty()) {
      for (const auto &elem : EF) {
        const WildcardPattern *ArchivePattern = elem->archive();
        bool ArchiveMatchWithFileName = false;
        if (ArchivePattern) {
          bool res = false;
          if (I && I->getInput()->findFileMatchedPattern(ArchivePattern, res))
            ArchiveMatchWithFileName = res;
          else {
            ArchiveMatchWithFileName =
                matched(*ArchivePattern, pFilename, fileNameHash);
            if (I && storePatterns)
              I->getInput()->addFileMatchedPattern(ArchivePattern,
                                                   ArchiveMatchWithFileName);
          }
        }

        bool FileMatchWithMemberName = false;
        bool FileMatchWithFileName = false;
        const WildcardPattern *FilePattern = elem->file();
        if (FilePattern) {
          bool res = false;
          if (storePatterns && I &&
              I->getInput()->findMemberMatchedPattern(FilePattern, res))
            FileMatchWithMemberName = res;
          else {
            FileMatchWithMemberName = matched(*FilePattern, pName, nameHash);
            if (I && storePatterns)
              I->getInput()->addMemberMatchedPattern(FilePattern,
                                                     FileMatchWithMemberName);
          }
          if (I && I->getInput()->findFileMatchedPattern(FilePattern, res))
            FileMatchWithFileName = res;
          else {
            FileMatchWithFileName =
                matched(*FilePattern, pFilename, fileNameHash);
            if (I && storePatterns)
              I->getInput()->addFileMatchedPattern(FilePattern,
                                                   FileMatchWithFileName);
          }
        }
        // It matches patterns such as: '<ArchivePattern>:<MemberPattern>'
        if (isArchive && (elem)->isFileInArchive() &&
            ArchiveMatchWithFileName && FileMatchWithMemberName)
          return false;

        // It matches patterns such as: '<ArchivePattern>:'
        if (isArchive && (elem)->isArchive() && ArchiveMatchWithFileName &&
            !(elem)->isFileInArchive())
          return false;

        // It matches patterns such as: '<MemberPattern>'
        if (isArchive && (elem)->file() && !elem->isArchive() &&
            FileMatchWithMemberName)
          return false;

        // Match archive name because script don't understand what is archive.
        if ((elem)->file() && FileMatchWithFileName)
          return false;
      }
    }
  }
  return match;
}

bool SectionMap::matched(const WildcardPattern &pPattern, StringRef pName,
                         uint64_t nameHash) const {
  // If the pattern to search doesnot contain any wildcards, then
  // just use the hash to compare.
  if (pPattern.hasHash())
    return nameHash == pPattern.hashValue();
  return pPattern.matched(pName, nameHash);
}

SectionMap::OutputSectionEntryDescList
SectionMap::GetOutputSectionEntrySectionsForPluginType(plugin::Plugin::Type T) {
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

ELFSection *SectionMap::createOutputSectionEntry(std::string section,
                                                 LDFileFormat::Kind kind,
                                                 uint32_t type, uint32_t flag,
                                                 uint32_t align) {
  OutputSectionEntry *output =
      make<OutputSectionEntry>(this, section, kind, type, flag, align);
  return output->getSection();
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

ELFSection *SectionMap::createEhFrameSection(std::string section, uint32_t type,
                                             uint32_t flag, uint32_t entSize,
                                             uint64_t size) {
  return make<EhFrameSection>(section, m_LinkerConfig.getDiagEngine(), type,
                              flag, entSize, size);
}

EhFrameHdrSection *SectionMap::createEhFrameHdrSection(std::string section,
                                                       uint32_t type,
                                                       uint32_t flag,
                                                       uint32_t entSize,
                                                       uint64_t size) {
  return make<EhFrameHdrSection>(section, type, flag, entSize, size);
}

OutputSectionEntry *
SectionMap::findOutputSectionEntry(const std::string &name) {
  auto it = std::find_if(
      m_OutputSectionEntryDescList.begin(), m_OutputSectionEntryDescList.end(),
      [name](const OutputSectionEntry *O) { return O->name() == name; });
  if (it == m_OutputSectionEntryDescList.end())
    return nullptr;
  return *it;
}

bool SectionMap::doesRuleMatchWithSection(const RuleContainer &R,
                                          const Section &S,
                                          bool doNotUseRMName) const {
  InputFile *IF = S.getInputFile();
  ASSERT(IF != nullptr, "Section must always have a non-empty InputFile!");
  bool isCommonSection = false;
  if (const CommonELFSection *commonSection =
          llvm::dyn_cast<const CommonELFSection>(&S)) {
    IF = commonSection->getOrigin();
    isCommonSection = true;
  }
  if (S.getOldInputFile())
    IF = S.getOldInputFile();
  const std::string &inputFileName = IF->getInput()->getResolvedPath().native();
  std::string sectName = S.name().str();
  uint64_t sectHash = 0;
  if (!doNotUseRMName) {
    ObjectFile *OF = llvm::dyn_cast<ObjectFile>(IF);
    const ELFSection *ESect = llvm::dyn_cast<const ELFSection>(&S);
    if (OF && ESect) {
      if (auto optRMSectName = OF->getRuleMatchingSectName(ESect->getIndex())) {
        sectName = optRMSectName.value();
        sectHash = llvm::hash_combine(sectName);
      }
    }
  }
  bool isArchive = IF->isArchive() ||
                   llvm::dyn_cast<eld::ArchiveMemberInput>(IF->getInput());
  uint64_t inputFileHash = IF->getInput()->getResolvedPathHash();
  uint64_t archiveMemNameHash = IF->getInput()->getArchiveMemberNameHash();
  return matched(
      R, IF, inputFileName, sectName, isArchive, IF->getInput()->getName(),
      sectHash, inputFileHash, archiveMemNameHash,
      (m_LinkerConfig.options().getScriptOption() == GeneralOptions::MatchGNU),
      isCommonSection, /*storePatterns=*/false);
}
