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
ObjectBuilder::ObjectBuilder(LinkerConfig &PConfig, Module &PTheModule)
    : ThisConfig(PConfig), ThisModule(PTheModule) {}

/// MergeSection - merge the pInput section to the pOutput section
ELFSection *ObjectBuilder::mergeSection(GNULDBackend &PGnuldBackend,
                                        ELFSection *CurInputSection) {

  SectionMap::mapping Pair;
  Pair.first = CurInputSection->getOutputSection();
  Pair.second = CurInputSection->getMatchedLinkerScriptRule();

  if (shouldSkipMergeSection(CurInputSection))
    return nullptr;

  std::string OutputName;

  if (Pair.first == nullptr)
    OutputName = CurInputSection->name().str();
  else
    OutputName = Pair.first->name().str();

  ELFSection *Target = nullptr;

  if (Pair.first)
    Target = Pair.first->getSection();
  else if (!Target)
    Target = ThisModule.getSection(OutputName);

  bool OverrideOutput = false;
  if (shouldCreateNewSection(Target, CurInputSection)) {
    OverrideOutput = true;
    const OutputSectionEntry *OutputSection =
        CurInputSection->getOutputSection();
    std::string InputSectionName =
        CurInputSection->getDecoratedName(ThisConfig.options());
    std::string OutputSectionName = OutputSection
                                        ? OutputSection->name().str()
                                        : CurInputSection->name().str();
    if (ThisModule.getPrinter()->isVerbose())
      ThisConfig.raise(Diag::mapping_input_section_to_output_section)
          << InputSectionName << OutputSectionName;
    Target = ThisModule.createOutputSection(
        OutputSectionName, CurInputSection->getKind(),
        CurInputSection->getType(), CurInputSection->getFlags(),
        CurInputSection->getAddrAlign());
    Target->setEntSize(CurInputSection->getEntSize());
    Pair.first = Target->getOutputSection();
    Pair.second = Pair.first->getLastRule();
    if (!Pair.second)
      Pair.second = Pair.first->createDefaultRule(ThisModule);
    CurInputSection->setOutputSection(Pair.first);
    CurInputSection->setMatchedLinkerScriptRule(Pair.second);
    if (CurInputSection->getLink())
      Target->setLink(CurInputSection->getLink());
  } else {
    // If the output section was created previously when merging a section of a
    // similiar name, lets get the output section from in there.
    if (!Pair.first) {
      Pair.first = Target->getOutputSection();
      Pair.second = Pair.first->getLastRule();
      if (!Pair.second)
        Pair.second = Pair.first->createDefaultRule(ThisModule);
      CurInputSection->setOutputSection(Pair.first);
      CurInputSection->setMatchedLinkerScriptRule(Pair.second);
    } else
      Target = Pair.first->getSection();
  }

  // When we dont have linker scripts, the output section needs to be set
  // appropriately for each input section depending on what output section was
  // chosen above.
  if (!CurInputSection->getOutputSection() || OverrideOutput)
    CurInputSection->setOutputSection(Pair.first);

  if (!CurInputSection->getMatchedLinkerScriptRule())
    CurInputSection->setMatchedLinkerScriptRule(Pair.second);

  ELFSection *ToSection = nullptr;

  if (Pair.second) {
    ToSection = Pair.second->getSection();
  } else {
    ToSection = Target;
  }

  mayChangeSectionTypeOrKind(ToSection, CurInputSection);

  if (CurInputSection->isGroupKind()) {
    if (moveSection(CurInputSection, Target)) {
      // Add all the input sections that were part of the group
      for (auto *GroupSection : CurInputSection->getGroupSections())
        Target->addSectionsToGroup(GroupSection);
      Target->setSymbol(CurInputSection->getSymbol());
      if (!CurInputSection->getOutputSection())
        CurInputSection->setOutputSection(Target->getOutputSection());
      return Target;
    }
  } else if (!PGnuldBackend.DoesOverrideMerge(CurInputSection)) {
    moveSection(CurInputSection, ToSection);
    return ToSection;
  }
  return nullptr;
}

void ObjectBuilder::traceMergeStrings(const MergeableString *From,
                                      const MergeableString *To) const {
  ELFSection *OutputSection =
      From->Fragment->getOwningSection()->getOutputELFSection();
  std::string OutputSectionName =
      OutputSection->getDecoratedName(ThisConfig.options());
  if (!ThisConfig.options().shouldTraceMergeStrSection(OutputSection) &&
      ThisConfig.options().getMergeStrTraceType() != GeneralOptions::ALLOC)
    return;
  /// The output section's alloc flag has not been set yet, so we will have to
  /// use the input section's flag here
  if (ThisConfig.options().getMergeStrTraceType() == GeneralOptions::ALLOC &&
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
      From->Fragment->getOwningSection()->getDecoratedName(
          ThisConfig.options());
  std::string SectionTo = From->Fragment->getOwningSection()->getDecoratedName(
      ThisConfig.options());
  ThisConfig.raise(Diag::merging_fragments)
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

/// moveSection - move the fragments of pTO section data to pTo
bool ObjectBuilder::moveSection(ELFSection *PFrom, ELFSection *PTo) {
  assert(PFrom != PTo && "Cannot move section data to itself!");

  int AlignFrom = PFrom->getAddrAlign();
  int AlignTo = PTo->getAddrAlign();

  if (AlignFrom > AlignTo)
    PTo->setAddrAlign(AlignFrom);

  if (!PFrom->getFragmentList().size())
    return false;

  if (PFrom->isWanted())
    PTo->setWanted(true);
  PTo->splice(PTo->getFragmentList().end(), PFrom->getFragmentList());
  return true;
}

bool ObjectBuilder::moveIntoOutputSection(ELFSection *PFrom, ELFSection *PTo) {
  assert(PFrom != PTo && "Cannot move section data to itself!");

  int AlignFrom = PFrom->getAddrAlign();
  int AlignTo = PTo->getAddrAlign();

  if (AlignFrom > AlignTo)
    PTo->setAddrAlign(AlignFrom);

  if (!PFrom->getFragmentList().size())
    return false;

  if (PFrom->isWanted())
    PTo->setWanted(true);
  return true;
}

void ObjectBuilder::doPluginIterateSections(eld::InputFile *Obj,
                                            plugin::PluginBase *P) {
  bool IsLinkerInternal = Obj->isInternal();
  ObjectFile *ObjFile = llvm::dyn_cast<ObjectFile>(Obj);
  for (auto &Sect : ObjFile->getSections()) {
    if (Sect->isBitcode())
      continue;
    ELFSection *Section = llvm::dyn_cast<eld::ELFSection>(Sect);
    // Get the section kind.
    int32_t SectionKind = Section->getKind();
    if (SectionKind == LDFileFormat::Null ||
        SectionKind == LDFileFormat::StackNote ||
        (SectionKind == LDFileFormat::NamePool && !IsLinkerInternal) ||
        (SectionKind == LDFileFormat::Relocation && !IsLinkerInternal) ||
        SectionKind == LDFileFormat::Group)
      continue;
    if (P->getType() == plugin::Plugin::Type::SectionIterator &&
        Section->isIgnore())
      continue;
    if (P->getType() == plugin::Plugin::Type::SectionIterator)
      (llvm::dyn_cast<plugin::SectionIteratorPlugin>(P))
          ->processSection(plugin::Section(Section));
    else if (P->getType() == plugin::Plugin::Type::SectionMatcher)
      (llvm::dyn_cast<plugin::SectionMatcherPlugin>(P))
          ->processSection(plugin::Section(Section));
  }
}

void ObjectBuilder::assignInputFromOutput(eld::InputFile *Obj) {
  std::unordered_map<Section *, bool> RetrySections;
  bool IsPartialLink = (ThisConfig.codeGenType() == LinkerConfig::Object);
  bool IsGnuCompatible =
      (ThisConfig.options().getScriptOption() == GeneralOptions::MatchGNU);
  bool LinkerScriptHasSectionsCommand =
      ThisModule.getScript().linkerScriptHasSectionsCommand();
  SectionMap &SectionMap = ThisModule.getScript().sectionMap();
  ObjectFile *ObjFile = llvm::dyn_cast<ObjectFile>(Obj);
  std::vector<Section *> Sections = getInputSectionsForRuleMatching(ObjFile);
  // For all output sections.
  for (auto *Out : SectionMap) {
    // For each input rule.
    for (auto *In : *Out) {
      auto Start = std::chrono::system_clock::now();
      // For each input section.
      for (Section *Section : Sections) {
        ELFSection *ELFSect = llvm::dyn_cast<eld::ELFSection>(Section);

        bool IsRetrySect = false;

        // If the section has an already assigned output section, skip.
        // If the section needs to be retried, we may need to revisit the
        // section to match the best rule.
        if (Section->getOutputSection()) {
          if (!RetrySections.size() ||
              (RetrySections.find(Section) == RetrySections.end())) {
            continue;
          }
          IsRetrySect = true;
        }

        if (ELFSect) {
          // If the rule needs to match on permissions, skip if the rule doesnot
          // satisfy.
          switch (Out->prolog().constraint()) {
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
        if (IsPartialLink && (ELFSect && ELFSect->isMergeStr()) &&
            !LinkerScriptHasSectionsCommand)
          continue;
        InputFile *Input = Obj;
        bool IsCommonSection = false;
        if (CommonELFSection *CommonSection =
                llvm::dyn_cast<CommonELFSection>(Section)) {
          Input = CommonSection->getOrigin();
          IsCommonSection = true;
        }
        if (Section->getOldInputFile())
          Input = Section->getOldInputFile();
        std::string const &PInputFile =
            Input->getInput()->getResolvedPath().native();
        std::string const &Name = Input->getInput()->getName();
        bool IsArchive =
            Input->isArchive() ||
            llvm::dyn_cast<eld::ArchiveMemberInput>(Input->getInput());
        if (!Input->getInput()->isPatternMapInitialized()) {
          std::lock_guard<std::mutex> Guard(Mutex);
          Input->getInput()->resize(
              ThisModule.getScript().getNumWildCardPatterns());
        }
        // Hash of all the required things for Match.
        uint64_t InputFileHash = Input->getInput()->getResolvedPathHash();
        uint64_t NameHash = Input->getInput()->getArchiveMemberNameHash();
        std::string SectName = Section->name().str();
        uint64_t InputSectionHash = Section->sectionNameHash();
        if (ELFSection *ELFSect = llvm::dyn_cast<ELFSection>(Section)) {
          if (auto OptRThisSectionName =
                  ObjFile->getRuleMatchingSectName(ELFSect->getIndex())) {
            SectName = OptRThisSectionName.value();
            InputSectionHash = llvm::hash_combine(SectName);
          }
        }
        if (SectionMap.matched(*In, Input, PInputFile, SectName, IsArchive,
                               Name, InputSectionHash, InputFileHash, NameHash,
                               IsGnuCompatible, IsCommonSection)) {
          In->incMatchCount();
          Section->setOutputSection(Out);
          Section->setMatchedLinkerScriptRule(In);
          // FIXME: Shouldn't we set ELFSect to LDFileFormat::Discard?
          if (ELFSect && Out->isDiscard()) {
            ELFSect->setKind(LDFileFormat::Ignore);
            if (ThisConfig.options().isSectionTracingRequested() &&
                ThisConfig.options().traceSection(ELFSect->name().str()))
              ThisConfig.raise(Diag::discarded_section_info)
                  << ELFSect->getDecoratedName(ThisConfig.options())
                  << ObjFile->getInput()->decoratedPath();
          }
          if (IsRetrySect && !In->isSpecial())
            RetrySections.erase(Section);
          // Retry the match until the closest match is found.
          if (In->isSpecial())
            RetrySections.insert(std::make_pair(Section, true));
        } // end match
      } // end each input section
      In->addMatchTime(std::chrono::system_clock::now() - Start);
    } // end each rule
  } // end each output section
}

bool ObjectBuilder::initializePluginsAndProcess(
    const std::vector<eld::InputFile *> &Inputs, plugin::Plugin::Type T) {
  LinkerScript::PluginVectorT PluginList =
      ThisModule.getScript().getPluginForType(T);
  if (!PluginList.size())
    return true;

  for (auto &P : PluginList) {
    if (!P->init(ThisModule.getOutputTarWriter()))
      return false;
  }

  if (T == plugin::Plugin::SectionMatcher ||
      T == plugin::Plugin::SectionIterator) {
    for (auto &Obj : Inputs) {
      if (Obj->isLinkerScript())
        continue;
      for (auto &P : PluginList) {
        doPluginIterateSections(Obj, P->getLinkerPlugin());
      }
    }
  }

  if (T == plugin::Plugin::OutputSectionIterator) {
    for (auto &P : PluginList)
      doPluginOutputSectionsIterate(P->getLinkerPlugin());
  }

  for (auto &P : PluginList) {
    if (!P->run(ThisModule.getScript().getPluginRunList())) {
      ThisModule.setFailure(true);
      return false;
    }
  }

  if (PluginList.size()) {
    for (auto &P : PluginList) {
      if (!P->destroy()) {
        ThisModule.setFailure(true);
        return false;
      }
    }
  }

  if (!ThisConfig.getDiagEngine()->diagnose()) {
    if (ThisModule.getPrinter()->isVerbose())
      ThisConfig.raise(Diag::function_has_error) << __PRETTY_FUNCTION__;
    return false;
  }
  return true;
}

// FIXME: Pass sectionMap as const. This function is used in multithreaded-code,
// and sectionMap must not be modified in this function as modifying sectionMap
// would be data-race undefined behavior. Passing sectionMap as const
// will serve as a reminder that it must not be modified.
void ObjectBuilder::storePatternsForInputFile(InputFile *Obj,
                                              eld::SectionMap &SectionMap) {
  assert(Obj != ThisModule.getCommonInternalInput() &&
         "Common internal input file contains common symbols from various "
         "different input files. StorePatternsForInputFile should be "
         "called on the actual input files.");
  for (auto *Out : SectionMap) {
    for (auto &In : *Out) {
      if (In->spec().hasArchiveMember() && Obj->isArchive()) {
        std::string const &InputName = Obj->getInput()->getName();
        uint64_t NameHash = Obj->getInput()->getArchiveMemberNameHash();
        bool Result =
            SectionMap.matched(In->spec().archiveMember(), InputName, NameHash);
        Obj->getInput()->addMemberMatchedPattern(In->spec().getArchiveMember(),
                                                 Result);
      }
      if (In->spec().hasFile() && Obj->isArchive()) {
        std::string const &InputName = Obj->getInput()->getName();
        uint64_t NameHash = Obj->getInput()->getArchiveMemberNameHash();
        bool Result =
            SectionMap.matched(In->spec().file(), InputName, NameHash);
        Obj->getInput()->addMemberMatchedPattern(In->spec().getFile(), Result);
      }
      if (In->spec().hasFile()) {
        std::string const &InputFile =
            Obj->getInput()->getResolvedPath().native();
        uint64_t InputFileHash = Obj->getInput()->getResolvedPathHash();
        bool Result =
            SectionMap.matched(In->spec().file(), InputFile, InputFileHash);
        Obj->getInput()->addFileMatchedPattern(In->spec().getFile(), Result);
      }
    }
  }
}

void ObjectBuilder::assignOutputSections(std::vector<eld::InputFile *> Inputs,
                                         bool IsPostLtoPhase) {
  auto &SectionMap = ThisModule.getScript().sectionMap();
  bool HasSectionsCommand =
      ThisModule.getScript().linkerScriptHasSectionsCommand();

  ThisModule.setLinkState(LinkState::BeforeLayout);

  std::sort(Inputs.begin(), Inputs.end(), [](InputFile *A, InputFile *B) {
    return A->getNumSections() > B->getNumSections();
  });

  // For each input object.
  if (ThisConfig.options().numThreads() <= 1 ||
      !ThisConfig.isAssignOutputSectionsMultiThreaded()) {
    if (ThisModule.getPrinter()->traceThreads())
      ThisConfig.raise(Diag::threads_disabled) << "AssignOutputSections";
    for (auto &Obj : Inputs) {
      if (IsPostLtoPhase && Obj->isBitcode())
        continue;
      ObjectFile *ObjFile = llvm::dyn_cast<ObjectFile>(Obj);
      if (!ObjFile)
        ASSERT(0, "input is not an object file :" +
                      Obj->getInput()->decoratedPath());
      /// Internal common sections are assigned output sections later.
      if (Obj == ThisModule.getCommonInternalInput())
        continue;
      if (ObjFile && HasSectionsCommand && ObjFile->hasHighSectionCount())
        ThisConfig.raise(Diag::more_sections)
            << Obj->getInput()->decoratedPath();
      Obj->getInput()->resize(ThisModule.getScript().getNumWildCardPatterns());
      storePatternsForInputFile(Obj, SectionMap);
      assignInputFromOutput(Obj);
      Obj->getInput()->clear();
    }
  } else {
    if (ThisModule.getPrinter()->traceThreads())
      ThisConfig.raise(Diag::threads_enabled)
          << "AssignOutputSections" << ThisConfig.options().numThreads();
    llvm::ThreadPoolInterface *Pool = ThisModule.getThreadPool();
    for (auto &Obj : Inputs) {
      if (IsPostLtoPhase && Obj->isBitcode())
        continue;
      /// Internal common sections are assigned output sections later.
      if (Obj == ThisModule.getCommonInternalInput())
        continue;
      ObjectFile *ObjFile = llvm::dyn_cast<ObjectFile>(Obj);
      if (ObjFile && HasSectionsCommand && ObjFile->hasHighSectionCount())
        ThisConfig.raise(Diag::more_sections)
            << Obj->getInput()->decoratedPath();
      Pool->async([&] {
        Obj->getInput()->resize(
            ThisModule.getScript().getNumWildCardPatterns());
        storePatternsForInputFile(Obj, SectionMap);
        assignInputFromOutput(Obj);
        Obj->getInput()->clear();
      });
    }
    Pool->wait();
  }

  assignOutputSectionsToCommonSymbols();

  // print stats if any.
  printStats();
}

void ObjectBuilder::printStats() {
  if (!ThisModule.getScript().linkerScriptHasSectionsCommand())
    return;
  auto &SectionMap = ThisModule.getScript().sectionMap();
  LayoutInfo *LayoutInfo = ThisModule.getLayoutInfo();
  for (auto *Out : SectionMap) {
    for (auto *In : *Out) {
      if (LayoutInfo && !In->getMatchCount())
        LayoutInfo->recordNoLinkerScriptRuleMatch();
      if (!ThisModule.getPrinter()->allStats())
        continue;
      if (In->desc()) {
        std::string Str;
        llvm::raw_string_ostream Ss(Str);
        In->desc()->dumpMap(Ss, false, false);
        ThisConfig.raise(Diag::linker_script_rule_matching_stats)
            << Ss.str() << In->getMatchCount()
            << static_cast<int>(
                   std::chrono::duration_cast<std::chrono::milliseconds>(
                       In->getMatchTime())
                       .count());
      }
    }
  }
}

// Change kind of section if the section to be merged is different from the one
// being merged into.
void ObjectBuilder::mayChangeSectionTypeOrKind(ELFSection *Target,
                                               ELFSection *I) const {
  if (Target->isNullType()) {
    Target->setKind(I->getKind());
    Target->setType(I->getType());
    return;
  }
  if (Target->isNoBits() && !I->isNoBits()) {
    Target->setKind(LDFileFormat::Regular);
    Target->setType(llvm::ELF::SHT_PROGBITS);
    return;
  }
  if (Target->isMergeKind() && !I->isMergeKind()) {
    Target->setKind(I->getKind());
    Target->setFlags(I->getFlags());
  }
}

/// updateSectionFlags - update pTo's flags when merging pFrom
void ObjectBuilder::updateSectionFlags(ELFSection *PTo, ELFSection *PFrom) {
  if (!PFrom->getFlags())
    return;

  // union the flags from input
  uint32_t Flags = PTo->getFlags();

  Flags |= PFrom->getFlags();

  if (PTo->getFlags()) {
    // If the output section never had a MERGE property, dont set the merge.
    if (0 == (PTo->getFlags() & llvm::ELF::SHF_MERGE))
      Flags &= ~llvm::ELF::SHF_MERGE;

    // If the output section never had a STRING property, dont set strings.
    if (0 == (PTo->getFlags() & llvm::ELF::SHF_STRINGS))
      Flags &= ~llvm::ELF::SHF_STRINGS;

    // If the output section never had a LINKORDER property, dont set link
    // order.
    if (0 == (PTo->getFlags() & llvm::ELF::SHF_LINK_ORDER))
      Flags &= ~llvm::ELF::SHF_LINK_ORDER;

    // If any of the input sections cleared the merge property, clear the
    // merge property.
    if (0 == (PFrom->getFlags() & llvm::ELF::SHF_MERGE))
      Flags &= ~llvm::ELF::SHF_MERGE;

    // if there is an input section is not SHF_STRINGS, clean this flag
    if (0 == (PFrom->getFlags() & llvm::ELF::SHF_STRINGS))
      Flags &= ~llvm::ELF::SHF_STRINGS;

    // if there is an input section is not LINKORDER, clean this flag
    if (0 == (PFrom->getFlags() & llvm::ELF::SHF_LINK_ORDER))
      Flags &= ~llvm::ELF::SHF_LINK_ORDER;

    // Merge Entry size.
    // If the entry sizes are not the same, lets reset the entry size.
    if (PFrom->getEntSize() != PTo->getEntSize())
      PTo->setEntSize(0);
  } else {
    PTo->setEntSize(PFrom->getEntSize());
  }

  // Erase the group flag if not building a object.
  if (config().codeGenType() != LinkerConfig::Object) {
    Flags &= ~llvm::ELF::SHF_GROUP;
    // Remove the retain flag.
    Flags &= ~llvm::ELF::SHF_GNU_RETAIN;
  } else {
    if (PFrom->isLinkOrder() && !PTo->getLink() && !PTo->hasSectionData())
      PTo->setLink(PFrom->getLink());
  }

  PTo->setFlags(Flags);

  if (PTo->getKind() != LDFileFormat::Internal &&
      (Flags & llvm::ELF::SHF_MASKPROC))
    PTo->setKind(LDFileFormat::Target);
}

/// This function figures out if a new section section needs to be created, or
/// can be merged with an existing output section.
bool ObjectBuilder::shouldCreateNewSection(ELFSection *target,
                                           ELFSection *I) const {
  if (!target || ThisConfig.options().shouldEmitUniqueOutputSections())
    return true;

  if (I->name().find("@") != llvm::StringRef::npos)
    return true;

  bool hasLinkerScriptSectionsCommand =
      ThisModule.getScript().linkerScriptHasSectionsCommand();
  if (ThisConfig.codeGenType() == LinkerConfig::Object) {
    // The linker only creates groups with partial link.
    if (target->isGroupKind())
      return true;
    if (I->isLinkOrder() && !I->isEXIDX()) {
      if (I->getLink() == target->getLink() || target->isUninit())
        return false;
      if (!hasLinkerScriptSectionsCommand)
        return true;
      if (ThisConfig.options().allowIncompatibleSectionsMix()) {
        ThisConfig.raise(Diag::note_incompatible_sections)
            << I->name() << I->getInputFile()->getInput()->decoratedPath()
            << target->name();
        return false;
      }
        std::string Str;
        if (target->getLink())
          Str = target->getLink()->getInputFile()->getInput()->decoratedPath();
        else
          Str = "No Available Sections";
        ThisConfig.raise(Diag::incompatible_sections)
            << I->name() << I->getInputFile()->getInput()->decoratedPath()
            << target->name();
        ThisModule.setFailure(true);
        return false;
    }

    uint64_t TargetHasGroup = target->getFlags() & llvm::ELF::SHF_GROUP;
    uint64_t InputHasGroup = I->getFlags() & llvm::ELF::SHF_GROUP;

    // Sections that are part of a group are not merged with partial link.
    if (InputHasGroup || TargetHasGroup)
      return true;

    // With partial link, and no linker scripts any section that contains
    // strings that can be merged is not merged, so that the final link can
    // merge the sections to save space.
    if (!ThisModule.getScript().linkerScriptHasSectionsCommand() &&
        I->isMergeStr() && I->isAlloc())
      return true;
  }
  return false;
}

bool ObjectBuilder::shouldSkipMergeSection(ELFSection *I) const {
  // Dont merge sections for input files that are specified with
  // just symbols
  const InputFile *InputFile = I->getInputFile();
  if (InputFile->getInput()->getAttribute().isJustSymbols())
    return true;

  SectionMap::mapping Pair;
  bool IsPartialLink = (ThisConfig.codeGenType() == LinkerConfig::Object);
  Pair.first = I->getOutputSection();
  Pair.second = I->getMatchedLinkerScriptRule();

  if (Pair.first != nullptr && Pair.first->isDiscard())
    return true;

  // Dont merge .group sections.
  if (I->isGroupKind() && !IsPartialLink)
    return true;

  return false;
}

bool ObjectBuilder::doPluginOutputSectionsIterate(plugin::PluginBase *P) {
  if (P->getType() != plugin::Plugin::Type::OutputSectionIterator)
    return false;
  for (auto &Out : ThisModule.getScript().sectionMap())
    (llvm::dyn_cast<plugin::OutputSectionIteratorPlugin>(P))
        ->processOutputSection(plugin::OutputSection(Out));
  return true;
}

void ObjectBuilder::reAssignOutputSections(const plugin::LinkerWrapper *LW) {
  auto &SectionMap = ThisModule.getScript().sectionMap();
  bool IsGnuCompatible =
      (ThisConfig.options().getScriptOption() == GeneralOptions::MatchGNU);

  LinkerScript::OverrideSectionMatchT OverrideMatch =
      ThisModule.getScript().getSectionOverrides(LW);

  if (!OverrideMatch.size())
    return;

  auto OverrideFn = [&](size_t N) {
    auto *Override = OverrideMatch.at(N);
    ELFSection *ELFSect = Override->getELFSection();
    if (!ELFSect)
      return;
    std::string OutputSectionOverride = Override->getOutputSectionName();
    InputFile *Input = ELFSect->getInputFile();
    if (CommonELFSection *CommonSection =
            llvm::dyn_cast<CommonELFSection>(ELFSect))
      Input = CommonSection->getOrigin();
    SectionMap::iterator OutputSectIter =
        SectionMap.findIter(OutputSectionOverride);
    if (OutputSectIter != SectionMap.end()) {
      if (ELFSect->getOldInputFile())
        Input = ELFSect->getOldInputFile();
      std::string const &Name = Input->getInput()->getName();
      bool IsArchive =
          Input->isArchive() ||
          llvm::dyn_cast<eld::ArchiveMemberInput>(Input->getInput());
      // Hash of all the required things for Match.
      uint64_t InputFileHash = Input->getInput()->getResolvedPathHash();
      uint64_t NameHash = Input->getInput()->getArchiveMemberNameHash();
      uint64_t InputSectionHash = ELFSect->sectionNameHash();
      SectionMap::mapping Pair = ThisModule.getScript().sectionMap().findOnlyIn(
          OutputSectIter, Input->getInput()->getResolvedPath().native(),
          *ELFSect, IsArchive, Name, InputSectionHash, InputFileHash, NameHash,
          IsGnuCompatible);
      if (Pair.first) {
        ELFSect->setOutputSection(Pair.first);
        ELFSect->setMatchedLinkerScriptRule(Pair.second);
      }
      Override->setModifiedRule(Pair.second);
    } else {
      // FIXME: This guard can be removed!
      std::lock_guard<std::mutex> WarnGuard(Mutex);
      ThisConfig.raise(Diag::output_section_override_not_present)
          << ELFSect->name() << Input->getInput()->decoratedPath()
          << OutputSectionOverride;
    }
  };

  // For each input object.
  if (ThisConfig.options().numThreads() <= 1 ||
      !ThisConfig.isAssignOutputSectionsMultiThreaded()) {
    if (ThisModule.getPrinter()->traceThreads())
      ThisConfig.raise(Diag::threads_disabled) << "ReAssign OutputSections";
    for (uint32_t I = 0; I < OverrideMatch.size(); ++I)
      OverrideFn(I);
  } else {
    if (ThisModule.getPrinter()->traceThreads())
      ThisConfig.raise(Diag::threads_enabled)
          << "ReAssign OutputSections" << ThisConfig.options().numThreads();
    llvm::parallelFor((size_t)0, OverrideMatch.size(), OverrideFn);
  }
  // Clear the Overrides.
  ThisModule.getScript().clearSectionOverrides(LW);
}

bool ObjectBuilder::assignOutputSectionsToCommonSymbols() {
  SectionMap &SectionMap = ThisModule.getScript().sectionMap();
  ObjectFile *CommonSymbolsInput =
      llvm::dyn_cast<ObjectFile>(ThisModule.getCommonInternalInput());
  llvm::SmallSet<InputFile *, 16> CommonOriginInputs;
  for (const auto &S : CommonSymbolsInput->getSections()) {
    if (CommonELFSection *CommonSection = llvm::dyn_cast<CommonELFSection>(S)) {
      InputFile *I = CommonSection->getOrigin();
      if (I)
        CommonOriginInputs.insert(I);
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
  for (const auto &I : CommonOriginInputs) {
    I->getInput()->resize(ThisModule.getScript().getNumWildCardPatterns());
    storePatternsForInputFile(I, SectionMap);
  }
  assignInputFromOutput(CommonSymbolsInput);
  for (const auto &I : CommonOriginInputs)
    I->getInput()->clear();
  return true;
}

std::vector<Section *>
ObjectBuilder::getInputSectionsForRuleMatching(ObjectFile *ObjFile) {
  std::vector<Section *> Sections;
  bool IsLinkerInternal = ObjFile->isInternal();
  for (Section *S : ObjFile->getSections()) {
    if (ELFSection *ELFSect = llvm::dyn_cast<ELFSection>(S)) {
      // Get the section kind.
      int32_t SectionKind = ELFSect->getKind();
      if (SectionKind == LDFileFormat::Discard ||
          SectionKind == LDFileFormat::Ignore) {
        if (ThisConfig.options().isSectionTracingRequested() &&
            ThisConfig.options().traceSection(ELFSect->name().str()))
          ThisConfig.raise(Diag::discarded_section_info)
              << ELFSect->getDecoratedName(ThisConfig.options())
              << ObjFile->getInput()->decoratedPath();
        continue;
      }
      if (SectionKind == LDFileFormat::Null ||
          SectionKind == LDFileFormat::StackNote ||
          (SectionKind == LDFileFormat::NamePool && !IsLinkerInternal) ||
          (SectionKind == LDFileFormat::Relocation && !IsLinkerInternal) ||
          SectionKind == LDFileFormat::Group)
        continue;
      if (ELFSect->getOutputSection())
        continue;
    }
    Sections.push_back(S);
  }

  return Sections;
}
