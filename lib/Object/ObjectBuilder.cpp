//===- ObjectBuilder.cpp---------------------------------------------------===//
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
#include "eld/Object/ObjectBuilder.h"
#include "eld/Config/Config.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticPrinter.h"
#include "eld/Fragment/MergeStringFragment.h"
#include "eld/Input/ArchiveMemberInput.h"
#include "eld/Input/InputFile.h"
#include "eld/Input/ObjectFile.h"
#include "eld/Object/ObjectLinker.h"
#include "eld/Object/RuleContainer.h"
#include "eld/Object/SectionMap.h"
#include "eld/PluginAPI/OutputSectionIteratorPlugin.h"
#include "eld/PluginAPI/SectionIteratorPlugin.h"
#include "eld/PluginAPI/SectionMatcherPlugin.h"
#include "eld/Readers/CommonELFSection.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Readers/Relocation.h"
#include "eld/Support/HashUtils.h"
#include "eld/Support/RegisterTimer.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "eld/Target/GNULDBackend.h"
#include "eld/Target/LDFileFormat.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Parallel.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/ThreadPool.h"
#include <algorithm>
#include <chrono>

using namespace eld;

//===----------------------------------------------------------------------===//
// ObjectBuilder
//===----------------------------------------------------------------------===//
ObjectBuilder::ObjectBuilder(LinkerConfig &pConfig, Module &pTheModule)
    : m_Config(pConfig), m_Module(pTheModule) {}

/// MergeSection - merge the pInput section to the pOutput section
ELFSection *ObjectBuilder::MergeSection(GNULDBackend &pGNULDBackend,
                                        ELFSection *pInputSection) {

  SectionMap::mapping pair;
  pair.first = pInputSection->getOutputSection();
  pair.second = pInputSection->getMatchedLinkerScriptRule();

  if (shouldSkipMergeSection(pInputSection))
    return nullptr;

  std::string output_name;

  if (pair.first == nullptr)
    output_name = pInputSection->name().str();
  else
    output_name = pair.first->name().str();

  ELFSection *target = nullptr;

  if (pair.first)
    target = pair.first->getSection();
  else if (!target)
    target = m_Module.getSection(output_name);

  bool overrideOutput = false;
  if (shouldCreateNewSection(target, pInputSection)) {
    overrideOutput = true;
    const OutputSectionEntry *OutputSection = pInputSection->getOutputSection();
    std::string InputSectionName =
        pInputSection->getDecoratedName(m_Config.options());
    std::string OutputSectionName = OutputSection ? OutputSection->name().str()
                                                  : pInputSection->name().str();
    if (m_Module.getPrinter()->isVerbose())
      m_Config.raise(diag::mapping_input_section_to_output_section)
          << InputSectionName << OutputSectionName;
    target = m_Module.createOutputSection(
        OutputSectionName, pInputSection->getKind(), pInputSection->getType(),
        pInputSection->getFlags(), pInputSection->getAddrAlign());
    target->setEntSize(pInputSection->getEntSize());
    pair.first = target->getOutputSection();
    pair.second = pair.first->getLastRule();
    if (!pair.second)
      pair.second = pair.first->createDefaultRule(m_Module);
    pInputSection->setOutputSection(pair.first);
    pInputSection->setMatchedLinkerScriptRule(pair.second);
    if (pInputSection->getLink())
      target->setLink(pInputSection->getLink());
  } else {
    // If the output section was created previously when merging a section of a
    // similiar name, lets get the output section from in there.
    if (!pair.first) {
      pair.first = target->getOutputSection();
      pair.second = pair.first->getLastRule();
      if (!pair.second)
        pair.second = pair.first->createDefaultRule(m_Module);
      pInputSection->setOutputSection(pair.first);
      pInputSection->setMatchedLinkerScriptRule(pair.second);
    } else
      target = pair.first->getSection();
  }

  // When we dont have linker scripts, the output section needs to be set
  // appropriately for each input section depending on what output section was
  // chosen above.
  if (!pInputSection->getOutputSection() || overrideOutput)
    pInputSection->setOutputSection(pair.first);

  if (!pInputSection->getMatchedLinkerScriptRule())
    pInputSection->setMatchedLinkerScriptRule(pair.second);

  ELFSection *ToSection = nullptr;

  if (pair.second) {
    ToSection = pair.second->getSection();
  } else {
    ToSection = target;
  }

  mayChangeSectionTypeOrKind(ToSection, pInputSection);

  if (pInputSection->isGroupKind()) {
    if (MoveSection(pInputSection, target)) {
      // Add all the input sections that were part of the group
      for (auto *GroupSection : pInputSection->getGroupSections())
        target->addSectionsToGroup(GroupSection);
      target->setSymbol(pInputSection->getSymbol());
      if (!pInputSection->getOutputSection())
        pInputSection->setOutputSection(target->getOutputSection());
      return target;
    }
  } else if (!pGNULDBackend.DoesOverrideMerge(pInputSection)) {
    MoveSection(pInputSection, ToSection);
    return ToSection;
  }
  return nullptr;
}

void ObjectBuilder::traceMergeStrings(const MergeableString *From,
                                      const MergeableString *To) const {
  ELFSection *OutputSection =
      From->Fragment->getOwningSection()->getOutputELFSection();
  std::string OutputSectionName =
      OutputSection->getDecoratedName(m_Config.options());
  if (!m_Config.options().shouldTraceMergeStrSection(OutputSection) &&
      m_Config.options().getMergeStrTraceType() != GeneralOptions::ALLOC)
    return;
  /// The output section's alloc flag has not been set yet, so we will have to
  /// use the input section's flag here
  if (m_Config.options().getMergeStrTraceType() == GeneralOptions::ALLOC &&
      !From->Fragment->getOwningSection()->isAlloc())
    return;
  std::string FileFrom = From->Fragment->getOwningSection()
                             ->getInputFile()
                             ->getInput()
                             ->decoratedPath(true);
  std::string FileTo = To->Fragment->getOwningSection()
                           ->getInputFile()
                           ->getInput()
                           ->decoratedPath(true);
  std::string SectionFrom =
      From->Fragment->getOwningSection()->getDecoratedName(m_Config.options());
  std::string SectionTo =
      From->Fragment->getOwningSection()->getDecoratedName(m_Config.options());
  m_Config.raise(diag::merging_fragments)
      << FileFrom << SectionFrom << FileTo << SectionTo << From->String
      << OutputSectionName;
  assert(From->String == To->String);
}

void ObjectBuilder::mergeStrings(MergeStringFragment *F,
                                 OutputSectionEntry *O) {
  for (MergeableString *S : F->getStrings()) {
    MergeableString *MergedString =
        MergeStringFragment::mergeStrings(S, O, module());
    if (!MergedString)
      continue;
    if (config().getPrinter()->traceMergeStrings() ||
        config().getPrinter()->isVerbose())
      traceMergeStrings(S, MergedString);
  }
}

/// MoveSection - move the fragments of pTO section data to pTo
bool ObjectBuilder::MoveSection(ELFSection *pFrom, ELFSection *pTo) {
  assert(pFrom != pTo && "Cannot move section data to itself!");

  int alignFrom = pFrom->getAddrAlign();
  int alignTo = pTo->getAddrAlign();

  if (alignFrom > alignTo)
    pTo->setAddrAlign(alignFrom);

  if (!pFrom->getFragmentList().size())
    return false;

  if (pFrom->isWanted())
    pTo->setWanted(true);
  pTo->splice(pTo->getFragmentList().end(), pFrom->getFragmentList());
  return true;
}

bool ObjectBuilder::MoveIntoOutputSection(ELFSection *pFrom, ELFSection *pTo) {
  assert(pFrom != pTo && "Cannot move section data to itself!");

  int alignFrom = pFrom->getAddrAlign();
  int alignTo = pTo->getAddrAlign();

  if (alignFrom > alignTo)
    pTo->setAddrAlign(alignFrom);

  if (!pFrom->getFragmentList().size())
    return false;

  if (pFrom->isWanted())
    pTo->setWanted(true);
  return true;
}

void ObjectBuilder::DoPluginIterateSections(eld::InputFile *obj,
                                            plugin::PluginBase *P) {
  bool isLinkerInternal = obj->isInternal();
  ObjectFile *ObjFile = llvm::dyn_cast<ObjectFile>(obj);
  for (auto &sect : ObjFile->getSections()) {
    if (sect->isBitcode())
      continue;
    ELFSection *section = llvm::dyn_cast<eld::ELFSection>(sect);
    // Get the section kind.
    int32_t sectionKind = section->getKind();
    if (sectionKind == LDFileFormat::Null ||
        sectionKind == LDFileFormat::StackNote ||
        (sectionKind == LDFileFormat::NamePool && !isLinkerInternal) ||
        (sectionKind == LDFileFormat::Relocation && !isLinkerInternal) ||
        sectionKind == LDFileFormat::Group)
      continue;
    if (P->getType() == plugin::Plugin::Type::SectionIterator &&
        section->isIgnore())
      continue;
    if (P->getType() == plugin::Plugin::Type::SectionIterator)
      (llvm::dyn_cast<plugin::SectionIteratorPlugin>(P))
          ->processSection(plugin::Section(section));
    else if (P->getType() == plugin::Plugin::Type::SectionMatcher)
      (llvm::dyn_cast<plugin::SectionMatcherPlugin>(P))
          ->processSection(plugin::Section(section));
  }
}

void ObjectBuilder::assignInputFromOutput(eld::InputFile *obj) {
  std::unordered_map<Section *, bool> retrySections;
  bool isPartialLink = (m_Config.codeGenType() == LinkerConfig::Object);
  bool isGNUCompatible =
      (m_Config.options().getScriptOption() == GeneralOptions::MatchGNU);
  bool linkerScriptHasSectionsCommand =
      m_Module.getScript().linkerScriptHasSectionsCommand();
  SectionMap &sectionMap = m_Module.getScript().sectionMap();
  ObjectFile *ObjFile = llvm::dyn_cast<ObjectFile>(obj);
  std::vector<Section *> sections = getInputSectionsForRuleMatching(ObjFile);
  // For all output sections.
  for (auto out : sectionMap) {
    // For each input rule.
    for (auto in : *out) {
      auto start = std::chrono::system_clock::now();
      // For each input section.
      for (Section *section : sections) {
        ELFSection *ELFSect = llvm::dyn_cast<eld::ELFSection>(section);

        bool isRetrySect = false;

        // If the section has an already assigned output section, skip.
        // If the section needs to be retried, we may need to revisit the
        // section to match the best rule.
        if (section->getOutputSection()) {
          if (!retrySections.size() ||
              (retrySections.find(section) == retrySections.end())) {
            continue;
          }
          isRetrySect = true;
        }

        if (ELFSect) {
          // If the rule needs to match on permissions, skip if the rule doesnot
          // satisfy.
          switch (out->prolog().constraint()) {
          case OutputSectDesc::NO_CONSTRAINT:
            break;
          case OutputSectDesc::ONLY_IF_RO:
            if (ELFSect->isWritable())
              continue;
            break;
          case OutputSectDesc::ONLY_IF_RW:
            if (!ELFSect->isWritable())
              continue;
            break;
          }
        }
        // Skip sections with merge strings and if there is no linker scripts
        // provided.
        if (isPartialLink && (ELFSect && ELFSect->isMergeStr()) &&
            !linkerScriptHasSectionsCommand)
          continue;
        InputFile *input = obj;
        bool isCommonSection = false;
        if (CommonELFSection *commonSection =
                llvm::dyn_cast<CommonELFSection>(section)) {
          input = commonSection->getOrigin();
          isCommonSection = true;
        }
        if (section->getOldInputFile())
          input = section->getOldInputFile();
        std::string const &pInputFile =
            input->getInput()->getResolvedPath().native();
        std::string const &name = input->getInput()->getName();
        bool isArchive =
            input->isArchive() ||
            llvm::dyn_cast<eld::ArchiveMemberInput>(input->getInput());
        if (!input->getInput()->isPatternMapInitialized()) {
          std::lock_guard<std::mutex> Guard(Mutex);
          input->getInput()->resize(
              m_Module.getScript().getNumWildCardPatterns());
        }
        // Hash of all the required things for Match.
        uint64_t inputFileHash = input->getInput()->getResolvedPathHash();
        uint64_t nameHash = input->getInput()->getArchiveMemberNameHash();
        std::string sectName = section->name().str();
        uint64_t inputSectionHash = section->sectionNameHash();
        if (ELFSection *ELFSect = llvm::dyn_cast<ELFSection>(section)) {
          if (auto optRMSectName =
                  ObjFile->getRuleMatchingSectName(ELFSect->getIndex())) {
            sectName = optRMSectName.value();
            inputSectionHash = llvm::hash_combine(sectName);
          }
        }
        if (sectionMap.matched(*in, input, pInputFile, sectName, isArchive,
                               name, inputSectionHash, inputFileHash, nameHash,
                               isGNUCompatible, isCommonSection)) {
          in->incMatchCount();
          section->setOutputSection(out);
          section->setMatchedLinkerScriptRule(in);
          // FIXME: Shouldn't we set ELFSect to LDFileFormat::Discard?
          if (ELFSect && out->isDiscard()) {
            ELFSect->setKind(LDFileFormat::Ignore);
            if (m_Config.options().isSectionTracingRequested() &&
                m_Config.options().traceSection(ELFSect->name().str()))
              m_Config.raise(diag::discarded_section_info)
                  << ELFSect->getDecoratedName(m_Config.options())
                  << ObjFile->getInput()->decoratedPath();
          }
          if (isRetrySect && !in->isSpecial())
            retrySections.erase(section);
          // Retry the match until the closest match is found.
          if (in->isSpecial())
            retrySections.insert(std::make_pair(section, true));
        } // end match
      } // end each input section
      in->addMatchTime(std::chrono::system_clock::now() - start);
    } // end each rule
  } // end each output section
}

bool ObjectBuilder::InitializePluginsAndProcess(
    const std::vector<eld::InputFile *> &inputs, plugin::Plugin::Type T) {
  LinkerScript::PluginVectorT PluginList =
      m_Module.getScript().getPluginForType(T);
  if (!PluginList.size())
    return true;

  for (auto &P : PluginList) {
    if (!P->Init(m_Module.getOutputTarWriter()))
      return false;
  }

  if (T == plugin::Plugin::SectionMatcher ||
      T == plugin::Plugin::SectionIterator) {
    for (auto &obj : inputs) {
      if (obj->isLinkerScript())
        continue;
      for (auto &P : PluginList) {
        DoPluginIterateSections(obj, P->getLinkerPlugin());
      }
    }
  }

  if (T == plugin::Plugin::OutputSectionIterator) {
    for (auto &P : PluginList)
      DoPluginOutputSectionsIterate(P->getLinkerPlugin());
  }

  for (auto &P : PluginList) {
    if (!P->Run(m_Module.getScript().getPluginRunList())) {
      m_Module.setFailure(true);
      return false;
    }
  }

  if (PluginList.size()) {
    for (auto &P : PluginList) {
      if (!P->Destroy()) {
        m_Module.setFailure(true);
        return false;
      }
    }
  }

  if (!m_Config.getDiagEngine()->diagnose()) {
    if (m_Module.getPrinter()->isVerbose())
      m_Config.raise(diag::function_has_error) << __PRETTY_FUNCTION__;
    return false;
  }
  return true;
}

// FIXME: Pass sectionMap as const. This function is used in multithreaded-code,
// and sectionMap must not be modified in this function as modifying sectionMap
// would be data-race undefined behavior. Passing sectionMap as const
// will serve as a reminder that it must not be modified.
void ObjectBuilder::storePatternsForInputFile(InputFile *obj,
                                              eld::SectionMap &sectionMap) {
  assert(obj != m_Module.getCommonInternalInput() &&
         "Common internal input file contains common symbols from various "
         "different input files. StorePatternsForInputFile should be "
         "called on the actual input files.");
  for (auto out : sectionMap) {
    for (auto &in : *out) {
      if (in->spec().hasArchiveMember() && obj->isArchive()) {
        std::string const &InputName = obj->getInput()->getName();
        uint64_t nameHash = obj->getInput()->getArchiveMemberNameHash();
        bool result =
            sectionMap.matched(in->spec().archiveMember(), InputName, nameHash);
        obj->getInput()->addMemberMatchedPattern(in->spec().getArchiveMember(),
                                                 result);
      }
      if (in->spec().hasFile() && obj->isArchive()) {
        std::string const &InputName = obj->getInput()->getName();
        uint64_t nameHash = obj->getInput()->getArchiveMemberNameHash();
        bool result =
            sectionMap.matched(in->spec().file(), InputName, nameHash);
        obj->getInput()->addMemberMatchedPattern(in->spec().getFile(), result);
      }
      if (in->spec().hasFile()) {
        std::string const &InputFile =
            obj->getInput()->getResolvedPath().native();
        uint64_t inputFileHash = obj->getInput()->getResolvedPathHash();
        bool result =
            sectionMap.matched(in->spec().file(), InputFile, inputFileHash);
        obj->getInput()->addFileMatchedPattern(in->spec().getFile(), result);
      }
    }
  }
}

void ObjectBuilder::assignOutputSections(std::vector<eld::InputFile *> inputs,
                                         bool isPostLTOPhase) {
  auto &sectionMap = m_Module.getScript().sectionMap();
  bool hasSectionsCommand =
      m_Module.getScript().linkerScriptHasSectionsCommand();

  m_Module.setState(plugin::LinkerWrapper::BeforeLayout);

  std::sort(inputs.begin(), inputs.end(), [](InputFile *A, InputFile *B) {
    return A->getNumSections() > B->getNumSections();
  });

  // For each input object.
  if (m_Config.options().numThreads() <= 1 ||
      !m_Config.isAssignOutputSectionsMultiThreaded()) {
    if (m_Module.getPrinter()->traceThreads())
      m_Config.raise(diag::threads_disabled) << "AssignOutputSections";
    for (auto &obj : inputs) {
      if (isPostLTOPhase && obj->isBitcode())
        continue;
      ObjectFile *ObjFile = llvm::dyn_cast<ObjectFile>(obj);
      if (!ObjFile)
        ASSERT(0, "input is not an object file :" +
                      obj->getInput()->decoratedPath());
      /// Internal common sections are assigned output sections later.
      if (obj == m_Module.getCommonInternalInput())
        continue;
      if (ObjFile && hasSectionsCommand && ObjFile->hasHighSectionCount())
        m_Config.raise(diag::more_sections) << obj->getInput()->decoratedPath();
      obj->getInput()->resize(m_Module.getScript().getNumWildCardPatterns());
      storePatternsForInputFile(obj, sectionMap);
      assignInputFromOutput(obj);
      obj->getInput()->clear();
    }
  } else {
    if (m_Module.getPrinter()->traceThreads())
      m_Config.raise(diag::threads_enabled)
          << "AssignOutputSections" << m_Config.options().numThreads();
    llvm::ThreadPoolInterface *Pool = m_Module.getThreadPool();
    for (auto &obj : inputs) {
      if (isPostLTOPhase && obj->isBitcode())
        continue;
      /// Internal common sections are assigned output sections later.
      if (obj == m_Module.getCommonInternalInput())
        continue;
      ObjectFile *ObjFile = llvm::dyn_cast<ObjectFile>(obj);
      if (ObjFile && hasSectionsCommand && ObjFile->hasHighSectionCount())
        m_Config.raise(diag::more_sections) << obj->getInput()->decoratedPath();
      Pool->async([&] {
        obj->getInput()->resize(m_Module.getScript().getNumWildCardPatterns());
        storePatternsForInputFile(obj, sectionMap);
        assignInputFromOutput(obj);
        obj->getInput()->clear();
      });
    }
    Pool->wait();
  }

  assignOutputSectionsToCommonSymbols();

  // print stats if any.
  printStats();
}

void ObjectBuilder::printStats() {
  if (!m_Module.getScript().linkerScriptHasSectionsCommand())
    return;
  auto &sectionMap = m_Module.getScript().sectionMap();
  LayoutPrinter *layoutPrinter = m_Module.getLayoutPrinter();
  for (auto out : sectionMap) {
    for (auto in : *out) {
      if (layoutPrinter && !in->getMatchCount())
        layoutPrinter->recordNoLinkerScriptRuleMatch();
      if (!m_Module.getPrinter()->allStats())
        continue;
      if (in->desc()) {
        std::string Str;
        llvm::raw_string_ostream ss(Str);
        in->desc()->dumpMap(ss, false, false);
        m_Config.raise(diag::linker_script_rule_matching_stats)
            << ss.str() << in->getMatchCount()
            << static_cast<int>(
                   std::chrono::duration_cast<std::chrono::milliseconds>(
                       in->getMatchTime())
                       .count());
      }
    }
  }
}

// Change kind of section if the section to be merged is different from the one
// being merged into.
void ObjectBuilder::mayChangeSectionTypeOrKind(ELFSection *target,
                                               ELFSection *I) const {
  if (target->isNullType()) {
    target->setKind(I->getKind());
    target->setType(I->getType());
    return;
  }
  if (target->isNoBits() && !I->isNoBits()) {
    target->setKind(LDFileFormat::Regular);
    target->setType(llvm::ELF::SHT_PROGBITS);
    return;
  }
  if (target->isMergeKind() && !I->isMergeKind()) {
    target->setKind(I->getKind());
    target->setFlags(I->getFlags());
  }
}

/// updateSectionFlags - update pTo's flags when merging pFrom
void ObjectBuilder::updateSectionFlags(ELFSection *pTo, ELFSection *pFrom) {
  if (!pFrom->getFlags())
    return;

  // union the flags from input
  uint32_t flags = pTo->getFlags();

  flags |= pFrom->getFlags();

  if (pTo->getFlags()) {
    // If the output section never had a MERGE property, dont set the merge.
    if (0 == (pTo->getFlags() & llvm::ELF::SHF_MERGE))
      flags &= ~llvm::ELF::SHF_MERGE;

    // If the output section never had a STRING property, dont set strings.
    if (0 == (pTo->getFlags() & llvm::ELF::SHF_STRINGS))
      flags &= ~llvm::ELF::SHF_STRINGS;

    // If the output section never had a LINKORDER property, dont set link
    // order.
    if (0 == (pTo->getFlags() & llvm::ELF::SHF_LINK_ORDER))
      flags &= ~llvm::ELF::SHF_LINK_ORDER;

    // If any of the input sections cleared the merge property, clear the
    // merge property.
    if (0 == (pFrom->getFlags() & llvm::ELF::SHF_MERGE))
      flags &= ~llvm::ELF::SHF_MERGE;

    // if there is an input section is not SHF_STRINGS, clean this flag
    if (0 == (pFrom->getFlags() & llvm::ELF::SHF_STRINGS))
      flags &= ~llvm::ELF::SHF_STRINGS;

    // if there is an input section is not LINKORDER, clean this flag
    if (0 == (pFrom->getFlags() & llvm::ELF::SHF_LINK_ORDER))
      flags &= ~llvm::ELF::SHF_LINK_ORDER;

    // Merge Entry size.
    // If the entry sizes are not the same, lets reset the entry size.
    if (pFrom->getEntSize() != pTo->getEntSize())
      pTo->setEntSize(0);
  } else {
    pTo->setEntSize(pFrom->getEntSize());
  }

  // Erase the group flag if not building a object.
  if (config().codeGenType() != LinkerConfig::Object) {
    flags &= ~llvm::ELF::SHF_GROUP;
    // Remove the retain flag.
    flags &= ~llvm::ELF::SHF_GNU_RETAIN;
  } else {
    if (pFrom->isLinkOrder() && !pTo->getLink() && !pTo->hasSectionData())
      pTo->setLink(pFrom->getLink());
  }

  pTo->setFlags(flags);

  if (pTo->getKind() != LDFileFormat::Internal &&
      (flags & llvm::ELF::SHF_MASKPROC))
    pTo->setKind(LDFileFormat::Target);
}

/// This function figures out if a new section section needs to be created, or
/// can be merged with an existing output section.
bool ObjectBuilder::shouldCreateNewSection(ELFSection *target,
                                           ELFSection *I) const {
  if (!target || m_Config.options().shouldEmitUniqueOutputSections())
    return true;

  if (I->name().find("@") != llvm::StringRef::npos)
    return true;

  bool hasLinkerScriptSectionsCommand =
      m_Module.getScript().linkerScriptHasSectionsCommand();
  if (m_Config.codeGenType() == LinkerConfig::Object) {
    // The linker only creates groups with partial link.
    if (target->isGroupKind())
      return true;
    if (I->isLinkOrder() && !I->isEXIDX()) {
      if (I->getLink() == target->getLink() || target->isUninit())
        return false;
      else {
        if (!hasLinkerScriptSectionsCommand)
          return true;
        if (m_Config.options().allowIncompatibleSectionsMix()) {
          m_Config.raise(diag::note_incompatible_sections)
              << I->name() << I->getInputFile()->getInput()->decoratedPath()
              << target->name();
          return false;
        }
        std::string Str;
        if (target->getLink())
          Str = target->getLink()->getInputFile()->getInput()->decoratedPath();
        else
          Str = "No Available Sections";
        m_Config.raise(diag::incompatible_sections)
            << I->name() << I->getInputFile()->getInput()->decoratedPath()
            << target->name();
        m_Module.setFailure(true);
        return false;
      }
    }

    uint64_t targetHasGroup = target->getFlags() & llvm::ELF::SHF_GROUP;
    uint64_t InputHasGroup = I->getFlags() & llvm::ELF::SHF_GROUP;

    // Sections that are part of a group are not merged with partial link.
    if (InputHasGroup || targetHasGroup)
      return true;

    // With partial link, and no linker scripts any section that contains
    // strings that can be merged is not merged, so that the final link can
    // merge the sections to save space.
    if (!m_Module.getScript().linkerScriptHasSectionsCommand() &&
        I->isMergeStr() && I->isAlloc())
      return true;
  }
  return false;
}

bool ObjectBuilder::shouldSkipMergeSection(ELFSection *I) const {
  // Dont merge sections for input files that are specified with
  // just symbols
  const InputFile *inputFile = I->getInputFile();
  if (inputFile->getInput()->getAttribute().isJustSymbols())
    return true;

  SectionMap::mapping pair;
  bool isPartialLink = (m_Config.codeGenType() == LinkerConfig::Object);
  pair.first = I->getOutputSection();
  pair.second = I->getMatchedLinkerScriptRule();

  if (pair.first != nullptr && pair.first->isDiscard())
    return true;

  // Dont merge .group sections.
  if (I->isGroupKind() && !isPartialLink)
    return true;

  return false;
}

bool ObjectBuilder::DoPluginOutputSectionsIterate(plugin::PluginBase *P) {
  if (P->getType() != plugin::Plugin::Type::OutputSectionIterator)
    return false;
  for (auto &Out : m_Module.getScript().sectionMap())
    (llvm::dyn_cast<plugin::OutputSectionIteratorPlugin>(P))
        ->processOutputSection(plugin::OutputSection(Out));
  return true;
}

void ObjectBuilder::reAssignOutputSections(const plugin::LinkerWrapper *LW) {
  auto &sectionMap = m_Module.getScript().sectionMap();
  bool isGNUCompatible =
      (m_Config.options().getScriptOption() == GeneralOptions::MatchGNU);

  LinkerScript::OverrideSectionMatchT OverrideMatch =
      m_Module.getScript().getSectionOverrides(LW);

  if (!OverrideMatch.size())
    return;

  auto OverrideFn = [&](size_t n) {
    auto Override = OverrideMatch.at(n);
    ELFSection *ELFSect = Override->getELFSection();
    if (!ELFSect)
      return;
    std::string OutputSectionOverride = Override->getOutputSectionName();
    InputFile *input = ELFSect->getInputFile();
    if (CommonELFSection *commonSection =
            llvm::dyn_cast<CommonELFSection>(ELFSect))
      input = commonSection->getOrigin();
    SectionMap::iterator outputSectIter =
        sectionMap.findIter(OutputSectionOverride);
    if (outputSectIter != sectionMap.end()) {
      if (ELFSect->getOldInputFile())
        input = ELFSect->getOldInputFile();
      std::string const &name = input->getInput()->getName();
      bool isArchive =
          input->isArchive() ||
          llvm::dyn_cast<eld::ArchiveMemberInput>(input->getInput());
      // Hash of all the required things for Match.
      uint64_t inputFileHash = input->getInput()->getResolvedPathHash();
      uint64_t nameHash = input->getInput()->getArchiveMemberNameHash();
      uint64_t inputSectionHash = ELFSect->sectionNameHash();
      SectionMap::mapping pair = m_Module.getScript().sectionMap().findOnlyIn(
          outputSectIter, input->getInput()->getResolvedPath().native(),
          *ELFSect, isArchive, name, inputSectionHash, inputFileHash, nameHash,
          isGNUCompatible);
      if (pair.first) {
        ELFSect->setOutputSection(pair.first);
        ELFSect->setMatchedLinkerScriptRule(pair.second);
      }
      Override->setModifiedRule(pair.second);
    } else {
      // FIXME: This guard can be removed!
      std::lock_guard<std::mutex> warnGuard(Mutex);
      m_Config.raise(diag::output_section_override_not_present)
          << ELFSect->name() << input->getInput()->decoratedPath()
          << OutputSectionOverride;
    }
  };

  // For each input object.
  if (m_Config.options().numThreads() <= 1 ||
      !m_Config.isAssignOutputSectionsMultiThreaded()) {
    if (m_Module.getPrinter()->traceThreads())
      m_Config.raise(diag::threads_disabled) << "ReAssign OutputSections";
    for (uint32_t i = 0; i < OverrideMatch.size(); ++i)
      OverrideFn(i);
  } else {
    if (m_Module.getPrinter()->traceThreads())
      m_Config.raise(diag::threads_enabled)
          << "ReAssign OutputSections" << m_Config.options().numThreads();
    llvm::parallelFor((size_t)0, OverrideMatch.size(), OverrideFn);
  }
  // Clear the Overrides.
  m_Module.getScript().clearSectionOverrides(LW);
}

bool ObjectBuilder::assignOutputSectionsToCommonSymbols() {
  SectionMap &sectionMap = m_Module.getScript().sectionMap();
  ObjectFile *commonSymbolsInput =
      llvm::dyn_cast<ObjectFile>(m_Module.getCommonInternalInput());
  llvm::SmallSet<InputFile *, 16> commonOriginInputs;
  for (const auto &S : commonSymbolsInput->getSections()) {
    if (CommonELFSection *commonSection = llvm::dyn_cast<CommonELFSection>(S)) {
      InputFile *I = commonSection->getOrigin();
      if (I)
        commonOriginInputs.insert(I);
    }
  }
  // Store patterns for all the input files that contains common symbols,
  // assign output sections to internal common sections, and then clear
  // the stored patterns for the input files.
  // It is not the best way to assign output section to common sections,
  // because it may require storing patterns for a large number of input
  // files at the same time. But we need to do this because internal input
  // file, 'commonSymbolsInput', contain common symbols from different
  // source inputs.
  for (const auto &I : commonOriginInputs) {
    I->getInput()->resize(m_Module.getScript().getNumWildCardPatterns());
    storePatternsForInputFile(I, sectionMap);
  }
  assignInputFromOutput(commonSymbolsInput);
  for (const auto &I : commonOriginInputs)
    I->getInput()->clear();
  return true;
}

std::vector<Section *>
ObjectBuilder::getInputSectionsForRuleMatching(ObjectFile *objFile) {
  std::vector<Section *> sections;
  bool isLinkerInternal = objFile->isInternal();
  for (Section *S : objFile->getSections()) {
    if (ELFSection *ELFSect = llvm::dyn_cast<ELFSection>(S)) {
      // Get the section kind.
      int32_t sectionKind = ELFSect->getKind();
      if (sectionKind == LDFileFormat::Discard ||
          sectionKind == LDFileFormat::Ignore) {
        if (m_Config.options().isSectionTracingRequested() &&
            m_Config.options().traceSection(ELFSect->name().str()))
          m_Config.raise(diag::discarded_section_info)
              << ELFSect->getDecoratedName(m_Config.options())
              << objFile->getInput()->decoratedPath();
        continue;
      }
      if (sectionKind == LDFileFormat::Null ||
          sectionKind == LDFileFormat::StackNote ||
          (sectionKind == LDFileFormat::NamePool && !isLinkerInternal) ||
          (sectionKind == LDFileFormat::Relocation && !isLinkerInternal) ||
          sectionKind == LDFileFormat::Group)
        continue;
      if (ELFSect->getOutputSection())
        continue;
    }
    sections.push_back(S);
  }

  return sections;
}
