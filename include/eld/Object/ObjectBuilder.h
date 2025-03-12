//===- ObjectBuilder.h-----------------------------------------------------===//
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

#ifndef ELD_OBJECT_OBJECTBUILDER_H
#define ELD_OBJECT_OBJECTBUILDER_H
#include "eld/Input/ObjectFile.h"
#include "eld/PluginAPI/SectionIteratorPlugin.h"
#include "eld/Target/LDFileFormat.h"
#include "llvm/Support/DataTypes.h"
#include <mutex>
#include <string>
#include <unordered_set>

namespace ELD {
class LinkerWrapper;
}
namespace eld {

class ELFSection;
class Fragment;
struct MergeableString;
class MergeStringFragment;
class GNULDBackend;
class Input;
class LinkerConfig;
class Module;
class Relocation;
class RelocData;
class SectionMap;
/** \class ObjectBuilder
 *  \brief ObjectBuilder recieve ObjectAction and build the eld::Module.
 */
class ObjectBuilder {
public:
  ObjectBuilder(LinkerConfig &pConfig, Module &pTheModule);

  ELFSection *CreateSection(const std::string &pInputName,
                            LDFileFormat::Kind pKind, uint32_t pType,
                            uint32_t pFlag, uint32_t pAlign = 0x0);

  ELFSection *MergeSection(GNULDBackend &pGNULDBackend,
                           ELFSection *pInputSection);

  /// MoveSection - move the fragment of pFrom to pTo section data.
  bool MoveSection(ELFSection *pFrom, ELFSection *pTo);

  /// MoveIntoOutputSection - move the fragment of pFrom to pTo section data.
  bool MoveIntoOutputSection(ELFSection *pFrom, ELFSection *pTo);

  LinkerConfig &config() { return m_Config; }

  void assignInputFromOutput(eld::InputFile *obj);

  void assignOutputSections(std::vector<InputFile *> inputs, bool);

  /// Update section mappings for pending section overrides associated with the
  /// LinkerWrapper LW. If LW is nullptr, then update section mappings all the
  /// pending section overrides.
  void reAssignOutputSections(const plugin::LinkerWrapper *LW);

  bool InitializePluginsAndProcess(const std::vector<eld::InputFile *> &inputs,
                                   plugin::Plugin::Type T);

  void DoPluginIterateSections(eld::InputFile *obj, plugin::PluginBase *P);

  bool DoPluginOutputSectionsIterate(plugin::PluginBase *P);

  void printStats();

  Module &module() { return m_Module; }

  void updateSectionFlags(ELFSection *pTo, ELFSection *pFrom);

  void mayChangeSectionTypeOrKind(ELFSection *, ELFSection *) const;

  void storePatternsForInputFile(InputFile *, eld::SectionMap &);

  void mergeStrings(MergeStringFragment *F, OutputSectionEntry *O);

private:
  bool shouldSkipMergeSection(ELFSection *) const;

  bool shouldCreateNewSection(ELFSection *, ELFSection *) const;

  void traceMergeStrings(const MergeableString *From,
                         const MergeableString *To) const;

  /// Assigns output sections to internal common sections.
  bool assignOutputSectionsToCommonSymbols();

  // -------------------- Linker Script Caching Support -----------------

  /// Returns input sections on which linker script rule-matching needs to be
  /// performed.
  std::vector<Section *> getInputSectionsForRuleMatching(ObjectFile *objFile);

  /// (try to) Match section S using cache.
  ///
  /// Returns true if section is matched using cache-file; Otherwise returns
  /// false.

private:
  LinkerConfig &m_Config;
  Module &m_Module;
  std::mutex Mutex;
  bool m_hasLinkerScript;
};

} // namespace eld

#endif
