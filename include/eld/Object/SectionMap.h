//===- SectionMap.h--------------------------------------------------------===//
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
#ifndef ELD_OBJECT_SECTIONMAP_H
#define ELD_OBJECT_SECTIONMAP_H
#include "eld/Object/OutputSectionEntry.h"
#include "eld/Object/RuleContainer.h"
#include "eld/Script/InputSectDesc.h"
#include "eld/Script/OutputSectDesc.h"
#include "eld/Script/WildcardPattern.h"
#include "eld/Target/LDFileFormat.h"
#include "llvm/ADT/StringRef.h"
#include <unordered_map>
#include <vector>

namespace eld {

class ELFSection;
class LayoutPrinter;
class LinkerScript;
class LinkerConfig;
class EhFrameHdrSection;
class OutputSectionEntry;
class Section;

/** \class SectionMap
 *  \brief descirbe how to map input sections into output sections
 */
class SectionMap {
public:
  typedef std::pair<OutputSectionEntry *, RuleContainer *> mapping;
  typedef std::vector<OutputSectionEntry *> OutputSectionEntryDescList;
  typedef OutputSectionEntryDescList::const_iterator const_iterator;
  typedef OutputSectionEntryDescList::iterator iterator;
  typedef OutputSectionEntryDescList::const_reference const_reference;
  typedef OutputSectionEntryDescList::reference reference;

  typedef OutputSectionEntryDescList::reverse_iterator reverse_iterator;

public:
  SectionMap(LinkerScript &L, const LinkerConfig &config,
             LayoutPrinter *layoutPrinter);

  ~SectionMap();

  void assignOutputSectionEntrySections(std::vector<eld::Input *> &inputs,
                                        bool isPartialLink);

  mapping findOnlyIn(iterator output, std::string pInputFile,
                     const ELFSection &pInputSection, bool isArchive,
                     std::string name, uint64_t inputSectionHash,
                     uint64_t inputFileHash, uint64_t nameHash,
                     bool GNUCompatible);

  mapping findIn(iterator output, std::string pInputFile,
                 const ELFSection &pInputSection, bool isArchive,
                 std::string name, uint64_t inputSectionHash,
                 uint64_t inputFileHash, uint64_t nameHash, bool GNUCompatible);

  /// FIXME: this is not used
  mapping find(std::string pInputFile, std::string pInputSection,
               bool isArchive, std::string name, uint64_t inputSectionHash,
               uint64_t inputFileHash, uint64_t inputNameHash,
               bool GNUCompatible, bool isCommonSection);

  ELFSection *find(std::string pOutputSectionEntrySection);

  ELFSection *find(uint32_t SectionType);

  /// FIXME: this is not used or implemented
  ELFSection *findSectionIfNotEmpty(std::string Section);

  iterator findIter(std::string pOutputSectionEntrySection);

  std::pair<mapping, bool>
  insert(std::string pInputSection, std::string pOutputSectionEntrySection,
         InputSectDesc::Policy pPolicy = InputSectDesc::NoKeep);
  std::pair<mapping, bool> insert(const InputSectDesc &pInputDesc,
                                  OutputSectDesc &pOutputSectionEntryDesc);

  bool empty() const { return m_OutputSectionEntryDescList.empty(); }
  size_t size() const { return m_OutputSectionEntryDescList.size(); }

  iterator begin() { return m_OutputSectionEntryDescList.begin(); }
  const_iterator begin() const { return m_OutputSectionEntryDescList.begin(); }
  iterator end() { return m_OutputSectionEntryDescList.end(); }
  const_iterator end() const { return m_OutputSectionEntryDescList.end(); }

  const_reference front() const { return m_OutputSectionEntryDescList.front(); }
  reference front() { return m_OutputSectionEntryDescList.front(); }
  const_reference back() const { return m_OutputSectionEntryDescList.back(); }
  reference back() { return m_OutputSectionEntryDescList.back(); }

  reverse_iterator rbegin() { return m_OutputSectionEntryDescList.rbegin(); }
  reverse_iterator rend() { return m_OutputSectionEntryDescList.rend(); }

  iterator insert(iterator pPosition, ELFSection *pSection);
  iterator insert(iterator pPosition, OutputSectionEntry *Output);

  ELFSection *createOutputSectionEntry(std::string section,
                                       LDFileFormat::Kind kind, uint32_t type,
                                       uint32_t flag, uint32_t align);

  ELFSection *createELFSection(const std::string &Name, LDFileFormat::Kind K,
                               uint32_t Type, uint32_t Flags, uint32_t EntSize);

  EhFrameHdrSection *createEhFrameHdrSection(std::string section, uint32_t type,
                                             uint32_t flag,
                                             uint32_t entSize = 0,
                                             uint64_t size = 0);

  ELFSection *createEhFrameSection(std::string section, uint32_t type,
                                   uint32_t flag, uint32_t entSize = 0,
                                   uint64_t size = 0);

  std::vector<ELFSection *> &getEntrySections() { return m_entrySections; }

  void addEntrySection(ELFSection *sec) { m_entrySections.push_back(sec); }

  bool matched(const RuleContainer &pInput, InputFile *I,
               std::string const &pInputFile, std::string const &pInputSection,
               bool isArchive, std::string const &name,
               uint64_t pInputSectionHash, uint64_t fileNameHash,
               uint64_t nameHash, bool GNUCompatible, bool isCommonSection,
               bool storePatterns = true) const;

  bool matched(const WildcardPattern &pPattern, llvm::StringRef pName,
               uint64_t hash) const;

  OutputSectionEntryDescList
  GetOutputSectionEntrySectionsForPluginType(plugin::Plugin::Type T);

  LinkerScript &getLinkerScript() const { return m_LinkerScript; }

  bool matched(const WildcardPattern &pPattern, llvm::StringRef pName) const;

  /// Returns a pointer to an OutputSectionEntry object with name 'name',
  /// if any; Otherwise returns nullptr.
  OutputSectionEntry *findOutputSectionEntry(const std::string &name);

  bool doesRuleMatchWithSection(const RuleContainer &R, const Section &S,
                                bool doNotUseRMName) const;

private:
  bool matched_sections(InputFile *I, const WildcardPattern &inputFilePattern,
                        const WildcardPattern &pPattern,
                        std::string const &pName, std::string const &Filename,
                        std::string const &pInputSection, bool isArchive,
                        uint64_t pInputSectionHash, uint64_t fileNameHash,
                        uint64_t nameHash, bool isCommonSection,
                        const ExcludeFiles &EF,
                        bool storePatterns = true) const;

  /// If 'pattern' is COMMON or one of the .scommon.x, then return the pattern
  /// COMMON.* or .scommon.x.* repectively. Otherwise, return 'pattern' as
  /// it is.
  ///
  /// This is used to provide syntactic sugar for input section description
  /// in the linker script. Internal common sections are of the form
  /// COMMON.SymbolName, or .scommon.x.SymbolName. Syntactic sugar for common
  /// patterns allows correct matching of internal common sections to the
  /// corresponding common patterns. It is required to be consistent with the
  /// existing behavior.
  static WildcardPattern
  getWithSyntacticSugarForCommonPattern(const WildcardPattern &pattern);

private:
  LinkerScript &m_LinkerScript;
  const LinkerConfig &m_LinkerConfig;
  bool m_IsSectionTracingRequested;
  OutputSectionEntryDescList m_OutputSectionEntryDescList;
  std::unordered_map<std::string,
                     std::pair<OutputSectionEntry *, RuleContainer *>>
      _specialSections;
  std::vector<ELFSection *> m_entrySections;
  const DiagnosticPrinter *m_Printer;
  LayoutPrinter *m_LayoutPrinter = nullptr;
};

} // namespace eld

#endif
