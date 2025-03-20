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
} // namespace ELD
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
  ObjectBuilder(LinkerConfig &PConfig, Module &PTheModule);

  ELFSection *createSection(const std::string &PInputName,
                            LDFileFormat::Kind PKind, uint32_t PType,
                            uint32_t PFlag, uint32_t PAlign = 0x0);

  ELFSection *mergeSection(GNULDBackend &PGnuldBackend,
                           ELFSection *PInputSection);

  /// moveSection - move the fragment of pFrom to pTo section data.
  bool moveSection(ELFSection *PFrom, ELFSection *PTo);

  /// MoveIntoOutputSection - move the fragment of pFrom to pTo section data.
  bool moveIntoOutputSection(ELFSection *PFrom, ELFSection *PTo);

  LinkerConfig &config() { return ThisConfig; }

  void assignInputFromOutput(eld::InputFile *Obj);

  void assignOutputSections(std::vector<InputFile *> Inputs, bool);

  /// Update section mappings for pending section overrides associated with the
  /// LinkerWrapper LW. If LW is nullptr, then update section mappings all the
  /// pending section overrides.
  void reAssignOutputSections(const plugin::LinkerWrapper *LW);

  bool initializePluginsAndProcess(const std::vector<eld::InputFile *> &Inputs,
                                   plugin::Plugin::Type T);

  void doPluginIterateSections(eld::InputFile *Obj, plugin::PluginBase *P);

  bool doPluginOutputSectionsIterate(plugin::PluginBase *P);

  void printStats();

  Module &module() { return ThisModule; }

  void updateSectionFlags(ELFSection *PTo, ELFSection *PFrom);

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
  std::vector<Section *> getInputSectionsForRuleMatching(ObjectFile *ObjFile);

  /// (try to) Match section S using cache.
  ///
  /// Returns true if section is matched using cache-file; Otherwise returns
  /// false.

private:
  LinkerConfig &ThisConfig;
  Module &ThisModule;
  std::mutex Mutex;
  bool HasLinkerScript;
};

} // namespace eld

#endif
