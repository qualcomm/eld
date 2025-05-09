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
class LayoutInfo;
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
  SectionMap(LinkerScript &L, const LinkerConfig &Config,
             LayoutInfo *LayoutInfo);

  ~SectionMap();

  void assignOutputSectionEntrySections(std::vector<eld::Input *> &Inputs,
                                        bool IsPartialLink);

  mapping findOnlyIn(iterator Output, std::string PInputFile,
                     const ELFSection &CurInputSection, bool IsArchive,
                     std::string Name, uint64_t InputSectionHash,
                     uint64_t InputFileHash, uint64_t NameHash,
                     bool GNUCompatible);

  mapping findIn(iterator Output, std::string PInputFile,
                 const ELFSection &CurInputSection, bool IsArchive,
                 std::string Name, uint64_t InputSectionHash,
                 uint64_t InputFileHash, uint64_t NameHash, bool GNUCompatible);

  /// FIXME: this is not used
  mapping find(std::string PInputFile, std::string CurInputSection,
               bool IsArchive, std::string Name, uint64_t InputSectionHash,
               uint64_t InputFileHash, uint64_t InputNameHash,
               bool GNUCompatible, bool IsCommonSection);

  ELFSection *find(std::string EntrySection);

  ELFSection *find(uint32_t SectionType);

  /// FIXME: this is not used or implemented
  ELFSection *findSectionIfNotEmpty(std::string Section);

  iterator findIter(std::string EntrySection);

  std::pair<mapping, bool>
  insert(std::string CurInputSection, std::string EntrySection,
         InputSectDesc::Policy PPolicy = InputSectDesc::NoKeep);
  std::pair<mapping, bool> insert(const InputSectDesc &PInputDesc,
                                  OutputSectDesc &POutputSectionEntryDesc);

  bool empty() const { return MOutputSectionEntryDescList.empty(); }
  size_t size() const { return MOutputSectionEntryDescList.size(); }

  iterator begin() { return MOutputSectionEntryDescList.begin(); }
  const_iterator begin() const { return MOutputSectionEntryDescList.begin(); }
  iterator end() { return MOutputSectionEntryDescList.end(); }
  const_iterator end() const { return MOutputSectionEntryDescList.end(); }

  const_reference front() const { return MOutputSectionEntryDescList.front(); }
  reference front() { return MOutputSectionEntryDescList.front(); }
  const_reference back() const { return MOutputSectionEntryDescList.back(); }
  reference back() { return MOutputSectionEntryDescList.back(); }

  reverse_iterator rbegin() { return MOutputSectionEntryDescList.rbegin(); }
  reverse_iterator rend() { return MOutputSectionEntryDescList.rend(); }

  iterator insert(iterator PPosition, ELFSection *PSection);
  iterator insert(iterator PPosition, OutputSectionEntry *Output);

  ELFSection *createOutputSectionEntry(std::string Section,
                                       LDFileFormat::Kind Kind, uint32_t Type,
                                       uint32_t Flag, uint32_t Align);

  ELFSection *createELFSection(const std::string &Name, LDFileFormat::Kind K,
                               uint32_t Type, uint32_t Flags, uint32_t EntSize);

  EhFrameHdrSection *createEhFrameHdrSection(std::string Section, uint32_t Type,
                                             uint32_t Flag,
                                             uint32_t EntSize = 0,
                                             uint64_t Size = 0);

  ELFSection *createEhFrameSection(std::string Section, uint32_t Type,
                                   uint32_t Flag, uint32_t EntSize = 0,
                                   uint64_t Size = 0);

  std::vector<ELFSection *> &getEntrySections() { return MEntrySections; }

  void addEntrySection(ELFSection *Sec) { MEntrySections.push_back(Sec); }

  bool matched(const RuleContainer &PInput, InputFile *I,
               std::string const &PInputFile,
               std::string const &CurInputSection, bool IsArchive,
               std::string const &Name, uint64_t CurInputSectionHash,
               uint64_t FileNameHash, uint64_t NameHash, bool GNUCompatible,
               bool IsCommonSection, bool StorePatterns = true) const;

  bool matched(const WildcardPattern &PPattern, llvm::StringRef PName,
               uint64_t Hash) const;

  OutputSectionEntryDescList
  getOutputSectionEntrySectionsForPluginType(plugin::Plugin::Type T);

  LinkerScript &getLinkerScript() const { return MLinkerScript; }

  bool matched(const WildcardPattern &PPattern, llvm::StringRef PName) const;

  /// Returns a pointer to an OutputSectionEntry object with name 'name',
  /// if any; Otherwise returns nullptr.
  OutputSectionEntry *findOutputSectionEntry(const std::string &Name);

  bool doesRuleMatchWithSection(const RuleContainer &R, const Section &S,
                                bool DoNotUseRmName) const;

private:
  bool matchedSections(InputFile *I, const WildcardPattern &InputFilePattern,
                       const WildcardPattern &PPattern,
                       std::string const &PName, std::string const &Filename,
                       std::string const &CurInputSection, bool IsArchive,
                       uint64_t CurInputSectionHash, uint64_t FileNameHash,
                       uint64_t NameHash, bool IsCommonSection,
                       const ExcludeFiles &EF, bool StorePatterns = true) const;

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
  getWithSyntacticSugarForCommonPattern(const WildcardPattern &Pattern);

private:
  LinkerScript &MLinkerScript;
  const LinkerConfig &ThisConfig;
  bool IsSectionTracingRequested;
  OutputSectionEntryDescList MOutputSectionEntryDescList;
  std::unordered_map<std::string,
                     std::pair<OutputSectionEntry *, RuleContainer *>>
      SpecialSections;
  std::vector<ELFSection *> MEntrySections;
  const DiagnosticPrinter *MPrinter;
  LayoutInfo *MLayoutInfo = nullptr;
};

} // namespace eld

#endif
