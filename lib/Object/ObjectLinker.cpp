//===- ObjectLinker.cpp----------------------------------------------------===//
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
#include "eld/Object/ObjectLinker.h"
#include "eld/BranchIsland/BranchIslandFactory.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Driver/GnuLdDriver.h"
#include "eld/GarbageCollection/GarbageCollection.h"
#include "eld/Input/ArchiveMemberInput.h"
#include "eld/Input/BitcodeFile.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/Input/InputTree.h"
#include "eld/Input/InternalInputFile.h"
#include "eld/Input/LinkerScriptFile.h"
#include "eld/Input/ObjectFile.h"
#include "eld/LayoutMap/LayoutInfo.h"
#include "eld/LayoutMap/TextLayoutPrinter.h"
#include "eld/Object/GroupReader.h"
#include "eld/Object/ObjectBuilder.h"
#include "eld/Object/SectionMap.h"
#include "eld/PluginAPI/LinkerPlugin.h"
#include "eld/PluginAPI/OutputSectionIteratorPlugin.h"
#include "eld/Readers/ArchiveParser.h"
#include "eld/Readers/BinaryFileParser.h"
#include "eld/Readers/BitcodeReader.h"
#include "eld/Readers/CommonELFSection.h"
#include "eld/Readers/ELFDynObjParser.h"
#include "eld/Readers/ELFExecObjParser.h"
#include "eld/Readers/ELFRelocObjParser.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Readers/EhFrameHdrSection.h"
#include "eld/Readers/EhFrameSection.h"
#include "eld/Readers/ObjectReader.h"
#include "eld/Readers/Relocation.h"
#include "eld/Script/Assignment.h"
#include "eld/Script/InputSectDesc.h"
#include "eld/Script/OutputSectData.h"
#include "eld/Script/OutputSectDesc.h"
#include "eld/Script/ScriptFile.h"
#include "eld/Script/ScriptReader.h"
#include "eld/Script/ScriptSymbol.h"
#include "eld/Support/Memory.h"
#include "eld/Support/MsgHandling.h"
#include "eld/Support/RegisterTimer.h"
#include "eld/Support/StringRefUtils.h"
#include "eld/Support/Utils.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "eld/Target/ELFFileFormat.h"
#include "eld/Target/GNULDBackend.h"
#include "eld/Target/Relocator.h"
#include "llvm/ADT/ScopeExit.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/CodeGen/CommandFlags.h"
#include "llvm/CodeGen/MachineOptimizationRemarkEmitter.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/Support/Caching.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/FileOutputBuffer.h"
#include "llvm/Support/MemoryBufferRef.h"
#include "llvm/Support/Parallel.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/ThreadPool.h"
#include "llvm/Support/raw_ostream.h"
#include <chrono>
#include <mutex>
#include <sstream>

using namespace llvm;
using namespace eld;
namespace {
static DiagnosticEngine *SDiagEngineForLto = nullptr;
class PrepareDiagEngineForLTO {
public:
  PrepareDiagEngineForLTO(DiagnosticEngine *DiagEngine) {
    MMutex.lock();
    SDiagEngineForLto = DiagEngine;
  }
  ~PrepareDiagEngineForLTO() {
    SDiagEngineForLto = nullptr;
    MMutex.unlock();
  }

private:
  std::mutex MMutex;
};

codegen::RegisterCodeGenFlags CGF;

} // namespace

//===----------------------------------------------------------------------===//
// ObjectLinker
//===----------------------------------------------------------------------===//
ObjectLinker::ObjectLinker(LinkerConfig &PConfig, Module &M)
    : ThisConfig(PConfig), ThisModule(&M) {
  MSaveTemps = ThisConfig.options().getSaveTemps();
  MTraceLTO = ThisConfig.options().traceLTO();
  MLtoTempPrefix = getLTOTempPrefix();
}

void ObjectLinker::close() {
  // Cleanup all the temporary files created as a result of LTO.
  if (!MSaveTemps) {
    for (auto &P : FilesToRemove) {
      if (!P.empty()) {
        if (MTraceLTO || ThisConfig.getPrinter()->isVerbose())
          ThisConfig.raise(Diag::lto_deleting_temp_file) << P;
        llvm::sys::fs::remove(P);
      }
    }
  }
}

bool ObjectLinker::initialize() {
  MPBitcodeReader = createBitcodeReader();
  // initialize the readers and writers
  RelocObjParser = createRelocObjParser();
  DynObjReader = createDynObjReader();
  ArchiveParser = createArchiveParser();
  ELFExecObjParser = createELFExecObjParser();
  BinaryFileParser = createBinaryFileParser();
  // SymDef Reader.
  SymDefReader = createSymDefReader();
  GroupReader = make<eld::GroupReader>(*ThisModule, this);
  ScriptReader = make<eld::ScriptReader>();
  ObjWriter = createWriter();

  // initialize Relocator
  if (isBackendInitialized())
    getTargetBackend().initRelocator();

  return true;
}

/// initStdSections - initialize standard sections
bool ObjectLinker::initStdSections() {
  ObjectBuilder Builder(ThisConfig, *ThisModule);

  // initialize standard sections
  eld::Expected<void> E = getTargetBackend().initStdSections();
  if (!E) {
    ThisConfig.raiseDiagEntry(std::move(E.error()));
    return false;
  }

  // initialize dynamic sections
  if (LinkerConfig::Object != ThisConfig.codeGenType()) {
    getTargetBackend().initDynamicSections(
        *getTargetBackend().getDynamicSectionHeadersInputFile());

    // Note that patch section are only created in one internal input file
    // (DynamicSectionHeadersInputFile).
    if (ThisConfig.options().isPatchEnable())
      getTargetBackend().initPatchSections(
          *getTargetBackend().getDynamicSectionHeadersInputFile());
  }

  // initialize target-dependent sections
  getTargetBackend().initTargetSections(Builder);

  return true;
}

// Read Linker script Helper.
bool ObjectLinker::readLinkerScript(InputFile *Input) {

  LinkerScriptFile *LSFile = llvm::dyn_cast<eld::LinkerScriptFile>(Input);

  if (LSFile->isParsed()) {
    return true;
  }

  // Record the linker script in the Map file.
  LayoutInfo *layoutInfo = ThisModule->getLayoutInfo();
  if (layoutInfo)
    layoutInfo->recordLinkerScript(Input->getInput()->getFileName());

  ThisModule->getScript().addToHash(Input->getInput()->decoratedPath());

  ScriptFile *S =
      make<ScriptFile>(ScriptFile::LDScript, *ThisModule, *LSFile,
                       ThisModule->getIRBuilder()->getInputBuilder());

  LSFile->setParsed();
  LSFile->setScriptFile(S);

  bool SuccessFullInParse = getScriptReader()->readScript(ThisConfig, *S);
  if (layoutInfo)
    layoutInfo->closeLinkerScript();

  // Error if the linker script has an issue parsing.
  if (!SuccessFullInParse) {
    ThisConfig.raise(Diag::file_has_error)
        << Input->getInput()->getResolvedPath();
    return false;
  }
  // Update the caller with information if the linker script had sections et
  // all.
  if (S->linkerScriptHasSectionsCommand())
    ThisModule->getScript().setHasSectionsCmd();

  // Activate the Linker script.
  eld::Expected<void> E = S->activate(*ThisModule);
  if (!E) {
    ThisConfig.raiseDiagEntry(std::move(E.error()));
    if (!ThisConfig.getDiagEngine()->diagnose())
      return false;
  }

  Input->setUsed(true);

  return true;
}

bool ObjectLinker::readInputs(const std::vector<Node *> &InputVector) {
  typedef std::vector<Node *>::const_iterator Iter;

  for (Iter Begin = InputVector.begin(), End = InputVector.end(); Begin != End;
       ++Begin) {
    // is a group node
    if ((*Begin)->kind() == Node::GroupStart) {
      eld::RegisterTimer T("Read Start Group and End Group",
                           "Read all Input files",
                           ThisConfig.options().printTimingStats());
      getGroupReader()->readGroup(Begin,
                                  ThisModule->getIRBuilder()->getInputBuilder(),
                                  ThisConfig, MPostLtoPhase);
      continue;
    }

    FileNode *Node = llvm::dyn_cast<FileNode>(*Begin);

    if (!Node)
      continue;

    Input *Input = Node->getInput();
    // Resolve the path.
    if (!Input->resolvePath(ThisConfig)) {
      ThisModule->setFailure(true);
      return false;
    }
    if (!readAndProcessInput(Input, MPostLtoPhase))
      return false;
    if (Input->getInputFile()->getKind() == InputFile::GNULinkerScriptKind) {
      // Read inputs that the script contains.
      if (!readInputs(
              llvm::dyn_cast<eld::LinkerScriptFile>(Input->getInputFile())
                  ->getNodes())) {
        return false;
      }
    }
  } // end of for
  return true;
}

bool ObjectLinker::normalize() {
  if (MPostLtoPhase) {
    if (!insertPostLTOELF())
      return false;
  } else {
    parseIncludeOrExcludeLTOfiles();
    ThisModule->getNamePool().setupNullSymbol();
    addUndefSymbols();
    addDuplicateCodeInsteadOfTrampolines();
    addNoReuseOfTrampolines();
  }

  // Read all the inputs
  auto ReadSuccess =
      readInputs(ThisModule->getIRBuilder()->getInputBuilder().getInputs());
  if (!ReadSuccess) {
    return false;
  }

  // Create patch base input.
  if (const auto &PatchBase = ThisConfig.options().getPatchBase()) {
    Input *Input = make<eld::Input>(*PatchBase, ThisConfig.getDiagEngine());
    // Resolve the path.
    if (!Input->resolvePath(ThisConfig)) {
      ThisModule->setFailure(true);
      return false;
    }
    Input->getAttribute().setPatchBase();
    if (!readAndProcessInput(Input, MPostLtoPhase))
      return false;
  }

  if (!isBackendInitialized()) {
    ThisConfig.raise(Diag::error_unknown_target_emulation);
    return false;
  }

  getTargetBackend().addSymbols();

  // Initialize Default section mappings.
  if (!ThisModule->getScript().linkerScriptHasSectionsCommand())
    getTargetBackend().getInfo().InitializeDefaultMappings(*ThisModule);
  return true;
}

bool ObjectLinker::parseVersionScript() {
  if (!ThisConfig.options().hasVersionScript())
    return true;
  LayoutInfo *layoutInfo = ThisModule->getLayoutInfo();
  for (const auto &List : ThisConfig.options().getVersionScripts()) {
    Input *VersionScriptInput =
        eld::make<Input>(List, ThisConfig.getDiagEngine(), Input::Script);
    if (!VersionScriptInput->resolvePath(ThisConfig))
      return false;
    // Create an Input file and set the input file to be of kind DynamicList
    InputFile *VersionScriptInputFile =
        InputFile::create(VersionScriptInput, InputFile::GNULinkerScriptKind,
                          ThisConfig.getDiagEngine());
    addInputFileToTar(VersionScriptInputFile, eld::MappingFile::VersionScript);
    VersionScriptInput->setInputFile(VersionScriptInputFile);
    // Record the dynamic list script in the Map file.
    if (layoutInfo)
      layoutInfo->recordVersionScript(List);
    // Read the dynamic List file
    ScriptFile VersionScriptReader(
        ScriptFile::VersionScript, *ThisModule,
        *(llvm::dyn_cast<eld::LinkerScriptFile>(VersionScriptInputFile)),
        ThisModule->getIRBuilder()->getInputBuilder());
    bool SuccessFullInParse =
        getScriptReader()->readScript(ThisConfig, VersionScriptReader);
    if (!SuccessFullInParse)
      return false;
    ThisModule->addVersionScript(VersionScriptReader.getVersionScript());
    for (auto &VersionScriptNode :
         VersionScriptReader.getVersionScript()->getNodes()) {
      if (!VersionScriptNode->isAnonymous()) {
        ThisConfig.raise(Diag::unsupported_version_node)
            << VersionScriptInput->decoratedPath();
        continue;
      }
      if (VersionScriptNode->hasDependency()) {
        ThisConfig.raise(Diag::unsupported_dependent_node)
            << VersionScriptInput->decoratedPath();
        continue;
      }
      if (VersionScriptNode->hasError()) {
        ThisConfig.raise(Diag::error_parsing_version_script)
            << VersionScriptInput->decoratedPath();
        return false;
      }
      ThisModule->addVersionScriptNode(VersionScriptNode);
    }
  }
  auto &SymbolScopes = getTargetBackend().symbolScopes();
  for (auto &G : ThisModule->getNamePool().getGlobals()) {
    ResolveInfo *R = G.getValue();
    for (auto &VersionScriptNode : ThisModule->getVersionScriptNodes()) {
      if (VersionScriptNode->getGlobalBlock()) {
        for (auto *Sym : VersionScriptNode->getGlobalBlock()->getSymbols()) {
          if (Sym->getSymbolPattern()->matched(*R)) {
            getTargetBackend().addSymbolScope(R, Sym);
          } // end Symbol Match
        } // end Symbols
      } // end Global
      if (SymbolScopes.find(R) != SymbolScopes.end())
        continue;
      if (VersionScriptNode->getLocalBlock()) {
        for (auto *Sym : VersionScriptNode->getLocalBlock()->getSymbols()) {
          if (Sym->getSymbolPattern()->matched(*R)) {
            getTargetBackend().addSymbolScope(R, Sym);
          } // end Symbol Match
        } // end Symbols
      } // end Local
    } // end all Nodes
  } // end Globals
  return true;
}

std::string ObjectLinker::getLTOTempPrefix() const {
  std::string OutFileName = ThisConfig.options().outputFileName();
  auto &SaveTempsDir = ThisConfig.options().getSaveTempsDir();
  if (SaveTempsDir) {
    llvm::SmallString<256> Path(*SaveTempsDir);
    llvm::sys::path::append(Path,
                            Twine(llvm::sys::path::filename(OutFileName)));
    OutFileName = std::string(Path);
  }
  return (Twine(OutFileName) + Twine(".llvm-lto.")).str();
}

llvm::Expected<std::unique_ptr<llvm::raw_fd_ostream>>
ObjectLinker::createLTOTempFile(size_t Task, const std::string &Suffix,
                                SmallString<256> &FileName) const {

  int FD;
  std::error_code EC;
  std::string ErrMsg;
  if (MSaveTemps) {
    FileName =
        (Twine(MLtoTempPrefix) + Twine(Task) + Twine(".") + Twine(Suffix))
            .str();
    EC = llvm::sys::fs::openFileForWrite(FileName, FD);
    ErrMsg = std::string(FileName);
  } else {
    EC = llvm::sys::fs::createTemporaryFile("llvm-lto", Suffix, FD, FileName);
    ErrMsg = "Could not create unique LTO object file";
  }
  if (EC) {
    ErrMsg += ": " + EC.message();
    ThisConfig.raise(Diag::fatal_no_codegen_compile) << ErrMsg;
    return make_error<StringError>(EC);
  }

  return std::make_unique<llvm::raw_fd_ostream>(FD, true);
}

void ObjectLinker::addInputFileToTar(const std::string &Name,
                                     eld::MappingFile::Kind K) {
  auto *OutputTar = ThisModule->getOutputTarWriter();
  if (!OutputTar || !llvm::sys::fs::exists(Name))
    return;
  OutputTar->createAndAddFile(Name, Name, K, /*isLTO*/ false);
}

void ObjectLinker::parseDynList() {
  if (!ThisConfig.options().hasDynamicList())
    return;

  GeneralOptions::const_dyn_list_iterator It =
      ThisConfig.options().dynListBegin();
  GeneralOptions::const_dyn_list_iterator Ie =
      ThisConfig.options().dynListEnd();
  for (; It != Ie; It++) {
    if (!parseListFile(*It, ScriptFile::DynamicList))
      return;
  }
}

void ObjectLinker::dataStrippingOpt() {
  // Garbege collection
  if (ThisModule->getIRBuilder()->shouldRunGarbageCollection())
    runGarbageCollection("GC");
}

void ObjectLinker::runGarbageCollection(const std::string &Phase,
                                        bool CommonSectionsOnly) {
  eld::RegisterTimer T("Perform Garbage collection", "Garbage Collection",
                       ThisConfig.options().printTimingStats());
  GarbageCollection GC(ThisConfig, getTargetBackend(), *ThisModule);
  GC.run(Phase, CommonSectionsOnly);
  MGcHasRun = true;
}

bool ObjectLinker::getInputs(std::vector<InputFile *> &Inputs) {
  eld::Module::obj_iterator Obj, ObjEnd = ThisModule->objEnd();
  for (Obj = ThisModule->objBegin(); Obj != ObjEnd; ++Obj)
    Inputs.push_back(*Obj);
  return true;
}

///
/// readRelocations - read all relocation entries
///
bool ObjectLinker::readRelocations() {
  std::vector<InputFile *> Inputs;
  getInputs(Inputs);
  for (auto *Ai : Inputs) {
    if (Ai->getInput()->getAttribute().isPatchBase()) {
      if (auto *ELFFile = llvm::dyn_cast<ELFFileBase>(Ai)) {
        eld::Expected<bool> Exp =
            getELFExecObjParser()->parsePatchBase(*ELFFile);
        if (!Exp.has_value())
          ThisConfig.raiseDiagEntry(std::move(Exp.error()));
        if (!Exp.has_value() || !Exp.value())
          return false;
      }
    }
    if (!Ai->isObjectFile())
      continue;
    // Dont read relocations from inputs that are specified
    // with just symbols attribute
    if (Ai->getInput()->getAttribute().isJustSymbols())
      continue;
    ObjectFile *Obj = llvm::dyn_cast<ObjectFile>(Ai);
    if (Obj->isInputRelocsRead())
      continue;
    if (ThisModule->getPrinter()->isVerbose())
      ThisConfig.raise(Diag::reading_relocs)
          << Obj->getInput()->decoratedPath();
    Obj->setInputRelocsRead();
    eld::Expected<bool> ExpReadRelocs =
        getRelocObjParser()->readRelocations(*Ai);
    if (!ExpReadRelocs.has_value())
      ThisConfig.raiseDiagEntry(std::move(ExpReadRelocs.error()));
    if (!ExpReadRelocs.has_value() || !ExpReadRelocs.value())
      return false;
  }
  return true;
}

void ObjectLinker::mergeNonAllocStrings(
    std::vector<OutputSectionEntry *> OutputSections,
    ObjectBuilder &Builder) const {
  for (OutputSectionEntry *O : OutputSections) {
    for (auto *RC : O->getRuleContainer()) {
      for (Fragment *F : RC->getSection()->getFragmentList()) {
        if (F->getOwningSection()->isAlloc())
          continue;
        auto *Strings = llvm::dyn_cast<MergeStringFragment>(F);
        if (!Strings)
          continue;
        Builder.mergeStrings(Strings, O);
      }
    }
  }
}

void ObjectLinker::mergeIdenticalStrings() const {
  /// FIXME: Why do we not have ObjectBuilder as a member variable?
  ObjectBuilder Builder(ThisConfig, *ThisModule);
  /// We can multithread across output sections as there is no shared state
  /// between them wrt string merging. When global string merging is enabled,
  /// strings would need to be placed in one Module, so threads should
  /// not be used.
  bool UseThreads = ThisConfig.options().numThreads() > 1;
  bool GlobalMerge = ThisConfig.options().shouldGlobalStringMerge();
  llvm::ThreadPoolInterface *Pool = ThisModule->getThreadPool();
  auto MergeStrings = [&](OutputSectionEntry *O) {
    for (RuleContainer *RC : *O) {
      for (Fragment *F : RC->getSection()->getFragmentList()) {
        if (!F->isMergeStr())
          continue;
        // if global merge is enabled then non-alloc strings have already been
        // merged
        if (GlobalMerge && !F->getOwningSection()->isAlloc())
          continue;
        Builder.mergeStrings(llvm::cast<MergeStringFragment>(F),
                             F->getOutputELFSection()->getOutputSection());
      }
    }
  };

  std::vector<OutputSectionEntry *> OutputSections;

  /// Run MergeStrings for every output section. Output sections are split
  /// between the section table contained in Module and SectionMap depending
  /// on if the section is an orphan section. These two sets of output sections
  /// are non-overlapping.
  for (ELFSection *S : *ThisModule) {
    if (!S->getOutputSection())
      continue;
    /// REL/RELA linker internal input sections are added to the section table
    /// and may be assigned to an existing OutputSectionEntry. Skip these
    /// sections to avoid merging strings for the same output section more than
    /// once.
    if (S->isRelocationSection())
      continue;
    OutputSections.push_back(S->getOutputSection());
  }
  for (OutputSectionEntry *O : ThisModule->getScript().sectionMap()) {
    OutputSections.push_back(O);
  }

  /// if GlobalMerge is enabled, merge non-alloc strings first without threads
  /// then use threads for the rest.
  if (GlobalMerge)
    mergeNonAllocStrings(OutputSections, Builder);

  for (OutputSectionEntry *O : OutputSections) {
    if (UseThreads)
      Pool->async(std::bind(MergeStrings, O));
    else
      MergeStrings(O);
  }
  if (UseThreads)
    Pool->wait();
}

void ObjectLinker::fixMergeStringRelocations() const {
  for (InputFile *I : ThisModule->getObjectList()) {
    if (I->isInternal())
      continue;
    ELFObjectFile *Obj = llvm::dyn_cast<ELFObjectFile>(I);
    if (!Obj)
      continue;
    for (ELFSection *S : Obj->getRelocationSections()) {
      if (ThisModule->getPrinter()->isVerbose() ||
          ThisModule->getPrinter()->traceMergeStrings())
        ThisConfig.raise(Diag::handling_merge_strings_for_section)
            << S->getDecoratedName(ThisConfig.options())
            << S->getInputFile()->getInput()->decoratedPath(true);
      getTargetBackend().getRelocator()->doMergeStrings(S);
    }
  }
}

void ObjectLinker::doMergeStrings() {
  if (ThisConfig.isLinkPartial())
    return;
  mergeIdenticalStrings();
  fixMergeStringRelocations();
}

void ObjectLinker::assignOutputSections(std::vector<eld::InputFile *> &Inputs) {
  ObjectBuilder Builder(ThisConfig, *ThisModule);
  auto Start = std::chrono::steady_clock::now();
  Builder.assignOutputSections(Inputs, MPostLtoPhase);
  // FIXME: Perhaps transfer entry section ownership to GarbageCollection as
  // Entry sections are only relevant with garbage collection.
  // Currently, entry section are computed even if garbage-collection is not
  // enabled.
  collectEntrySections();
  LayoutInfo *LayoutInfo = ThisModule->getLayoutInfo();
  if (LayoutInfo && LayoutInfo->LayoutInfo::showInitialLayout()) {
    TextLayoutPrinter *TextMapPrinter = ThisModule->getTextMapPrinter();
    if (TextMapPrinter) {
      // FIXME: ideally, we should not need 'updateMatchedSections' call here.
      // However, we need it because currently we do not maintain the list of
      // matched input sections for rule containers consistently.
      RuleContainer::updateMatchedSections(*ThisModule);
      TextMapPrinter->printLayout(*ThisModule);
    }
  }
  // FIXME: SectionMatcher plugins should not consume time under
  // 'LinkerScriptRuleMatch' timing category!
  if (!Builder.initializePluginsAndProcess(Inputs,
                                           plugin::Plugin::SectionMatcher))
    return;
  auto End = std::chrono::steady_clock::now();
  if (ThisModule->getPrinter()->allStats())
    ThisConfig.raise(Diag::linker_script_rule_matching_time)
        << (int)std::chrono::duration<double, std::milli>(End - Start).count();
}

// Sections in ELFFileFormat are not internal but are Output sections.
// At the moment, there is  no way being marked with assignOutputSections
// We traverse them explicitly to mark them as ignore before placing them.
void ObjectLinker::markDiscardFileFormatSections() {
  auto &SectionMap = ThisModule->getScript().sectionMap();
  SectionMap::iterator It = SectionMap.findIter("/DISCARD/");
  bool IsGnuCompatible =
      (ThisConfig.options().getScriptOption() == GeneralOptions::MatchGNU);
  for (auto &Sec : getTargetBackend().getOutputFormat()->getSections()) {
    SectionMap::mapping Pair =
        SectionMap.findIn(It, "internal", *Sec, false, "internal",
                          Sec->sectionNameHash(), 0, 0, IsGnuCompatible);
    if (Pair.first && Pair.first->isDiscard()) {
      Sec->setKind(LDFileFormat::Ignore);
      if (ThisConfig.options().isSectionTracingRequested() &&
          ThisConfig.options().traceSection(Pair.first->name().str())) {
        ThisConfig.raise(Diag::discarded_section_info)
            << Pair.first->getSection()->getDecoratedName(ThisConfig.options());
      }
    }
  }
}

bool ObjectLinker::mayBeSortSections(std::vector<Section *> &Sections) {
  // If no linker scripts, we dont store the original input. Lets not sort.
  if (!ThisModule->getScript().linkerScriptHasSectionsCommand())
    return true;
  if (ThisConfig.options().disableLTOLinkOrder())
    return true;
  // If we are doing partial link, lets not sort it.
  bool IsPartialLink = (LinkerConfig::Object == ThisConfig.codeGenType());
  if (IsPartialLink || LtoObjects.empty())
    return true;
  std::stable_sort(Sections.begin(), Sections.end(),
                   [](Section *ASection, Section *BSection) {
                     ELFSection *A = llvm::dyn_cast<ELFSection>(ASection);
                     ELFSection *B = llvm::dyn_cast<ELFSection>(BSection);
                     if (A == nullptr or B == nullptr)
                       return false;
                     // FIXME: Redundant checks. All files have original input.
                     if (!A->originalInput())
                       return false;
                     if (!B->originalInput())
                       return false;
                     if ((A->name().starts_with(".ctors")) ||
                         (B->name().starts_with(".ctors")))
                       return false;
                     if ((A->name().starts_with(".dtors")) ||
                         (B->name().starts_with(".dtors")))
                       return false;
                     int64_t AOrdinal =
                         A->originalInput()->getInput()->getInputOrdinal();
                     int64_t BOrdinal =
                         B->originalInput()->getInput()->getInputOrdinal();
                     if (AOrdinal == BOrdinal)
                       return false;
                     return (AOrdinal < BOrdinal);
                   });
  return true;
}

bool ObjectLinker::mergeInputSections(ObjectBuilder &Builder,
                                      std::vector<Section *> &Sections) {
  bool IsPartialLink = ThisConfig.isLinkPartial();
  for (auto &Section : Sections) {
    if (Section->isBitcode())
      continue;
    ELFSection *Sect = llvm::dyn_cast<ELFSection>(Section);
    bool HasSectionData = Sect->hasSectionData();
    if (Sect->isIgnore() || Sect->isDiscard())
      continue;
    switch (Sect->getKind()) {
    // Some *INPUT sections should not be merged.
    case LDFileFormat::Null:
    case LDFileFormat::NamePool:
    case LDFileFormat::Discard:
      // skip
      continue;
    case LDFileFormat::Relocation:
    case LDFileFormat::LinkOnce: {
      if (Sect->getLink()->isIgnore() || Sect->getLink()->isDiscard())
        Sect->setKind(LDFileFormat::Ignore);
      break;
    }
    case LDFileFormat::Target:
      if (getTargetBackend().DoesOverrideMerge(Sect)) {
        getTargetBackend().mergeSection(Sect);
        break;
      }
      LLVM_FALLTHROUGH;
    case LDFileFormat::EhFrame: {
      if (Sect->getKind() == LDFileFormat::EhFrame) {
        if (!llvm::dyn_cast<eld::EhFrameSection>(Sect)->splitEhFrameSection())
          return false;
        if (!llvm::dyn_cast<eld::EhFrameSection>(Sect)
                 ->createCIEAndFDEFragments())
          return false;
        llvm::dyn_cast<eld::EhFrameSection>(Sect)->finishAddingFragments(
            *ThisModule);
        if (getTargetBackend().getEhFrameHdr() &&
            Sect->getKind() == LDFileFormat::EhFrame) {
          getTargetBackend().getEhFrameHdr()->addCIE(
              llvm::dyn_cast<eld::EhFrameSection>(Sect)->getCIEs());
          // Since we found an EhFrame section, lets go ahead and start creating
          // the fragments necessary to create the .eh_frame_hdr section and
          // the filler eh_frame section.
          getTargetBackend().createEhFrameFillerAndHdrSection();
        }
      }
    }
      LLVM_FALLTHROUGH;
    default: {
      if (!(HasSectionData || (IsPartialLink && Sect->isWanted())))
        continue; // skip

      ELFSection *OutSect = nullptr;
      if (nullptr !=
          (OutSect = Builder.mergeSection(getTargetBackend(), Sect))) {
        Builder.updateSectionFlags(OutSect, Sect);
      }
      break;
    }
    } // end of switch
  } // for each section
  return true;
}

void ObjectLinker::sortByName(RuleContainer *I, bool SortRule) {
  if (SortRule) {
    std::stable_sort(
        I->getMatchedInputSections().begin(),
        I->getMatchedInputSections().end(),
        [](ELFSection *A, ELFSection *B) { return (A->name() < B->name()); });
    return;
  }
  ELFSection *S = I->getSection();
  std::stable_sort(S->getFragmentList().begin(), S->getFragmentList().end(),
                   [](Fragment *A, Fragment *B) {
                     return (A->getOwningSection()->name() <
                             B->getOwningSection()->name());
                   });
}

void ObjectLinker::sortByAlignment(RuleContainer *I, bool SortRule) {
  if (SortRule) {
    std::stable_sort(I->getMatchedInputSections().begin(),
                     I->getMatchedInputSections().end(),
                     [](ELFSection *A, ELFSection *B) {
                       return (A->getAddrAlign() > B->getAddrAlign());
                     });
    return;
  }
  ELFSection *S = I->getSection();
  std::stable_sort(S->getFragmentList().begin(), S->getFragmentList().end(),
                   [](Fragment *A, Fragment *B) {
                     return (A->getOwningSection()->getAddrAlign() >
                             B->getOwningSection()->getAddrAlign());
                   });
}

void ObjectLinker::sortByInitPriority(RuleContainer *I, bool SortRule) {
  if (SortRule) {
    std::stable_sort(I->getMatchedInputSections().begin(),
                     I->getMatchedInputSections().end(),
                     [](ELFSection *A, ELFSection *B) {
                       return (A->getPriority() < B->getPriority());
                     });
    return;
  }
  ELFSection *S = I->getSection();
  std::stable_sort(S->getFragmentList().begin(), S->getFragmentList().end(),
                   [](Fragment *A, Fragment *B) {
                     return (A->getOwningSection()->getPriority() <
                             B->getOwningSection()->getPriority());
                   });
}

bool ObjectLinker::sortSections(RuleContainer *I, bool SortRule) {
  eld::RegisterTimer T("Sort Sections", "Merge Sections",
                       ThisConfig.options().printTimingStats());
  if (!SortRule && (!(I->getSection()->hasSectionData())))
    return false;

  WildcardPattern::SortPolicy P = WildcardPattern::SortPolicy::SORT_NONE;

  if (I->spec().hasFile())
    P = I->spec().file().sortPolicy();

  if (P == WildcardPattern::SortPolicy::SORT_NONE) {
    if (!I->spec().hasSections())
      return false;

    auto &E = I->spec().sections().front();

    if (E->kind() != StrToken::Wildcard)
      return false;

    WildcardPattern &Pattern =
        llvm::cast<WildcardPattern>(*I->spec().sections().front());
    P = Pattern.sortPolicy();
    if (P == WildcardPattern::SortPolicy::SORT_NONE &&
        ThisConfig.options().isSortSectionEnabled()) {
      if (ThisConfig.options().isSortSectionByName())
        P = WildcardPattern::SORT_BY_NAME;
      else if (ThisConfig.options().isSortSectionByAlignment())
        P = WildcardPattern::SORT_BY_ALIGNMENT;
    }
  }

  switch (P) {
  default:
    break;
  case WildcardPattern::SortPolicy::SORT_BY_NAME:
    sortByName(I, SortRule);
    return true;
  case WildcardPattern::SortPolicy::SORT_BY_ALIGNMENT:
    sortByAlignment(I, SortRule);
    return true;
  case WildcardPattern::SortPolicy::SORT_BY_NAME_ALIGNMENT:
    sortByAlignment(I, SortRule);
    sortByName(I, SortRule);
    return true;
  case WildcardPattern::SortPolicy::SORT_BY_ALIGNMENT_NAME:
    sortByName(I, SortRule);
    sortByAlignment(I, SortRule);
    return true;
  case WildcardPattern::SortPolicy::SORT_BY_INIT_PRIORITY:
    sortByInitPriority(I, SortRule);
    return true;
  }
  return false;
}

bool ObjectLinker::createOutputSection(ObjectBuilder &Builder,
                                       OutputSectionEntry *Output,
                                       bool PostLayout) {
  uint64_t OutAlign = 0x0, InAlign = 0x0;
  bool IsPartialLink = (LinkerConfig::Object == ThisConfig.codeGenType());

  ELFSection *OutSect = Output->getSection();
  OutSect->setOutputSection(Output);
  if (OutSect->getKind() != LDFileFormat::NamePool)
    OutSect->setAddrAlign(0);
  OutputSectionEntry::iterator In, InBegin, InEnd;
  InBegin = Output->begin();
  InEnd = Output->end();
  bool HasSubAlign = false;

  // force input alignment from ldscript if any
  if (Output->prolog().hasSubAlign()) {
    Output->prolog().subAlign().eval();
    Output->prolog().subAlign().commit();
    InAlign = Output->prolog().subAlign().result();
    HasSubAlign = true;
  }

  // force output alignment from ldscript if any
  if (Output->prolog().hasAlign()) {
    Output->prolog().align().evaluateAndRaiseError();
    OutAlign = Output->prolog().align().result();
    if (OutAlign && !isPowerOf2_64(OutAlign)) {
      ThisConfig.raise(Diag::error_non_power_of_2_value_to_align_output_section)
          << Output->prolog().align().getContext() << utility::toHex(OutAlign)
          << OutSect->name();
      return false;
    }
    if (OutSect->getAddrAlign() < OutAlign)
      OutSect->setAddrAlign(OutAlign);
  }

  /// FIXME: this vector is unused?
  std::vector<RuleContainer *> InputsWithNoData;
  RuleContainer *FirstNonEmptyRule = nullptr;

  for (In = InBegin; In != InEnd; ++In) {
    ELFSection *InSect = (*In)->getSection();
    bool IsDirty = (*In)->isDirty();
    if (IsDirty) {
      InSect->setType(0);
      InSect->setFlags(0);
    }
    if (InSect->isWanted())
      OutSect->setWanted(true);
    // Recalculate alignment if the input rule is dirty.
    if (IsDirty) {
      size_t Alignment = 1;
      for (auto &F : InSect->getFragmentList()) {
        if (Alignment < F->alignment())
          Alignment = F->alignment();
        F->getOwningSection()->setMatchedLinkerScriptRule(*In);
        F->getOwningSection()->setOutputSection(Output);
        Builder.mayChangeSectionTypeOrKind(InSect, F->getOwningSection());
        Builder.updateSectionFlags(InSect, F->getOwningSection());
      }
      InSect->setAddrAlign(Alignment);
    }
    if (HasSubAlign && (InSect->getAddrAlign() < InAlign)) {
      if (InSect->getFragmentList().size()) {
        InSect->getFragmentList().front()->setAlignment(InAlign);
        InSect->setAddrAlign(InAlign);
      }
    }
    if (InSect->getFragmentList().size() && !FirstNonEmptyRule)
      FirstNonEmptyRule = *In;

    if (Builder.moveIntoOutputSection(InSect, OutSect)) {
      Builder.mayChangeSectionTypeOrKind(OutSect, InSect);
      Builder.updateSectionFlags(OutSect, InSect);
    }
    InSect->setOutputSection(Output);
  }

  finalizeOutputSectionFlags(Output);

  // Set the first fragment in the output section.
  Output->setFirstNonEmptyRule(FirstNonEmptyRule);

  Output->setOrder(getTargetBackend().getSectionOrder(*OutSect));

  // Assign offsets.
  assignOffset(Output);

  if (OutSect->size() || OutSect->isWanted()) {
    std::lock_guard<std::mutex> Guard(Mutex);
    if (IsPartialLink)
      OutSect->setWanted(true);
    ThisModule->addOutputSectionToTable(OutSect);
  }
  return true;
}

bool ObjectLinker::initializeMerge() {
  if (AllInputSections.size())
    return true;
  {
    eld::RegisterTimer T("Prepare Input Files For Merge", "Merge Sections",
                         ThisConfig.options().printTimingStats());
    // Gather all input sections.
    eld::Module::obj_iterator Obj, ObjEnd = ThisModule->objEnd();
    for (Obj = ThisModule->objBegin(); Obj != ObjEnd; ++Obj) {
      ObjectFile *ObjFile = llvm::dyn_cast<ObjectFile>(*Obj);
      if (!ObjFile)
        ASSERT(0, "input is not an object file :" +
                      (*Obj)->getInput()->decoratedPath());
      for (auto &Sect : ObjFile->getSections()) {
        addInputSection(Sect);
        if (!ThisModule->getLayoutInfo())
          continue;
        ThisModule->getLayoutInfo()->recordSectionStat(Sect);
      }
    }
  }
  {
    eld::RegisterTimer T("Sort sections if LTO enabled", "Merge Sections",
                         ThisConfig.options().printTimingStats());
    // Sort sections if we have LTO enabled.
    mayBeSortSections(AllInputSections);
  }
  return true;
}

bool ObjectLinker::updateInputSectionMappingsForPlugin() {
  eld::RegisterTimer T("Update Input Rules For Plugin", "Merge Sections",
                       ThisConfig.options().printTimingStats());
  // Initialize merging of sections.
  initializeMerge();

  // Clear the vector of sections collected by the rules.
  for (auto &Out : ThisModule->getScript().sectionMap()) {
    for (auto &In : *Out)
      In->clearSections();
  }
  for (auto &Section : AllInputSections) {
    if (Section->isBitcode())
      continue;
    ELFSection *Sect = llvm::dyn_cast<ELFSection>(Section);
    switch (Sect->getKind()) {
    // Some *INPUT sections should not be merged.
    case LDFileFormat::Null:
    case LDFileFormat::NamePool:
      continue;
    default:
      if (!Sect->getMatchedLinkerScriptRule())
        continue;
      Sect->getMatchedLinkerScriptRule()->addMatchedSection(Sect);
    }
  }

  // Sort the rule.
  for (auto &Out : ThisModule->getScript().sectionMap()) {
    for (auto &In : *Out)
      sortSections(In, true);
  }
  return true;
}

bool ObjectLinker::finishAssignOutputSections() {
  eld::RegisterTimer T("Update Input Rules From Plugin", "Perform Layout",
                       ThisConfig.options().printTimingStats());
  ObjectBuilder Builder(ThisConfig, *ThisModule);
  // Override output sections again!!
  Builder.reAssignOutputSections(/*LW=*/nullptr);
  // Clear the section match map.
  ThisModule->getScript().clearAllSectionOverrides();
  updateInputSectionMappingsForPlugin();
  return true;
}

bool ObjectLinker::finishAssignOutputSections(const plugin::LinkerWrapper *LW) {
  eld::RegisterTimer T("Update Input Rules From Plugin", "Perform Layout",
                       ThisConfig.options().printTimingStats());
  ObjectBuilder Builder(ThisConfig, *ThisModule);
  // Update section mapping of pending section overrides associated with the
  // LinkerWrapper LW.
  Builder.reAssignOutputSections(LW);
  // Clear section overrides associated with the LinkerWrapper LW.
  ThisModule->getScript().clearSectionOverrides(LW);
  updateInputSectionMappingsForPlugin();
  return true;
}

bool ObjectLinker::runOutputSectionIteratorPlugin() {
  {
    LinkerScript::PluginVectorT PluginList =
        ThisModule->getScript().getPluginForType(
            plugin::Plugin::OutputSectionIterator);
    if (!PluginList.size())
      return true;

    auto cleanup = llvm::make_scope_exit([this, PluginList]() {
      // Fragment movement verification is only done for CreatingSections link
      // state because fragments cannot be moved in any other link state.
      if (ThisModule->isLinkStateCreatingSections()) {
        for (auto *P : PluginList) {
          auto ExpVerifyFragmentMoves = P->verifyFragmentMovements();
          if (!ExpVerifyFragmentMoves) {
            ThisConfig.raiseDiagEntry(
                std::move(ExpVerifyFragmentMoves.error()));
            ThisModule->setFailure(true);
          }
        }
      }
    });

    for (auto &P : PluginList) {
      if (!P->init(ThisModule->getOutputTarWriter()))
        return false;
    }

    for (auto &P : PluginList) {
      plugin::PluginBase *L = P->getLinkerPlugin();
      for (auto &Out : ThisModule->getScript().sectionMap())
        (llvm::dyn_cast<plugin::OutputSectionIteratorPlugin>(L))
            ->processOutputSection(plugin::OutputSection(Out));
    }

    for (auto &P : PluginList) {
      if (!P->run(ThisModule->getScript().getPluginRunList())) {
        ThisModule->setFailure(true);
        return false;
      }
    }

    if (PluginList.size()) {
      for (auto &P : PluginList) {
        if (!P->destroy()) {
          ThisModule->setFailure(true);
          return false;
        }
      }
    }
  }

  return !ThisModule->linkFail();
}

/// mergeSections - put allinput sections into output sections
bool ObjectLinker::mergeSections() {
  ObjectBuilder Builder(ThisConfig, *ThisModule);
  // Output section iterator plugin.
  {
    eld::RegisterTimer T("Init", "Merge Sections",
                         ThisConfig.options().printTimingStats());
    // Initialize merging of input sections.
    if (!initializeMerge())
      return false;

    // Plugin support.
    if (!updateInputSectionMappingsForPlugin())
      return false;
  }

  setCommonSectionsFallbackToBSS();
  setCopyRelocSectionsFallbackToBSS();

  {
    eld::RegisterTimer T("Universal Plugin", "Merge Sections",
                         ThisConfig.options().printTimingStats());
    auto &PM = ThisModule->getPluginManager();
    if (!PM.callActBeforeSectionMergingHook())
      return false;
  }

  // Output section iterator plugin.
  {
    eld::RegisterTimer T("Plugin: Output Section Iterator Before Layout",
                         "Merge Sections",
                         ThisConfig.options().printTimingStats());
    if (!runOutputSectionIteratorPlugin())
      return false;
  }

  // Merge all the input sections.
  {
    eld::RegisterTimer T("Merge Input Sections", "Merge Sections",
                         ThisConfig.options().printTimingStats());
    mergeInputSections(Builder, AllInputSections);
  }

  // Update plugins from the YAML config file specified.
  {
    eld::RegisterTimer T("Update Output Sections With Plugins",
                         "Merge Sections",
                         ThisConfig.options().printTimingStats());
    if (!ThisModule->updateOutputSectionsWithPlugins()) {
      ThisModule->setFailure(true);
      return false;
    }
  }

  // Create output sections.
  SectionMap::iterator Out, OutBegin, OutEnd;
  OutBegin = ThisModule->getScript().sectionMap().begin();
  OutEnd = ThisModule->getScript().sectionMap().end();

  ThisModule->setLinkState(Module::LinkState::CreatingSections);
  if (!ThisConfig.getDiagEngine()->diagnose())
    return false;

  {
    eld::RegisterTimer T("Plugin: Output Section Iterator Creating Sections",
                         "Merge Sections",
                         ThisConfig.options().printTimingStats());
    if (!initializeOutputSectionsAndRunPlugin())
      return false;
  }

  reportPendingPluginRuleInsertions();

  {
    eld::RegisterTimer T("Create Output Section", "Merge Sections",
                         ThisConfig.options().printTimingStats());
    std::vector<OutputSectionEntry *> OutSections;
    for (Out = OutBegin; Out != OutEnd; ++Out) {
      OutSections.push_back(*Out);
    }
    for (auto &S : *ThisModule) {
      OutputSectionEntry *O = S->getOutputSection();
      if (O)
        OutSections.push_back(O);
    }

    if (ThisConfig.options().numThreads() <= 1 ||
        !ThisConfig.isCreateOutputSectionsMultiThreaded()) {
      for (auto &O : OutSections)
        createOutputSection(Builder, O);
    } else {
      llvm::ThreadPoolInterface *Pool = ThisModule->getThreadPool();
      for (auto &O : OutSections)
        Pool->async([&] { createOutputSection(Builder, O); });
      Pool->wait();
    }

    if (!ThisConfig.getDiagEngine()->diagnose()) {
      if (ThisModule->getPrinter()->isVerbose())
        ThisConfig.raise(Diag::function_has_error) << __PRETTY_FUNCTION__;
      return false;
    }
    {
      eld::RegisterTimer T("Assign Group Sections Offset", "Merge Sections",
                           ThisConfig.options().printTimingStats());
      assignOffsetToGroupSections();
    }
  }
  return true;
}

bool ObjectLinker::initializeOutputSectionsAndRunPlugin() {
  for (auto &O : ThisModule->getScript().sectionMap()) {
    for (auto &In : *O)
      sortSections(In, false);
  }
  return runOutputSectionIteratorPlugin();
}

void ObjectLinker::assignOffset(OutputSectionEntry *Out) {
  int64_t O = 0;
  const ELFSection *OutSection = Out->getSection();
  bool HasRules = ThisModule->getScript().linkerScriptHasRules();
  for (auto &C : *Out) {
    C->getSection()->setOffset(O);
    for (auto &F : C->getSection()->getFragmentList()) {
      if (!F->isNull()) {
        F->setOffset(O);
        O = (F->getOffset(ThisConfig.getDiagEngine()) + F->size());
      }

      // Warn if any non-allocatable input section has been assigned to an
      // allocatable output section. Only check for this diagnostic if
      // section mapping rules are defined in the linker script.
      if (!HasRules)
        continue;
      const ELFSection *OwningSection = F->getOwningSection();
      if (OutSection->isAlloc() && !OwningSection->isAlloc())
        ThisConfig.raise(
            Diag::non_allocatable_section_in_allocatable_output_section)
            << OwningSection->name()
            << OwningSection->getInputFile()->getInput()->decoratedPath()
            << OutSection->name();
    }
    C->getSection()->setSize(O - C->getSection()->offset());
  }
  Out->getSection()->setSize(O);
}

void ObjectLinker::assignOffset(ELFSection *C) {
  int64_t O = 0;
  for (auto &F : C->getFragmentList()) {
    F->setOffset(O);
    O = (F->getOffset(ThisConfig.getDiagEngine()) + F->size());
  }
  C->setSize(O);
}

void ObjectLinker::assignOffsetToGroupSections() {
  for (auto &S : *ThisModule) {
    if (S->hasNoFragments())
      continue;
    if (!S->hasSectionData())
      continue;
    if (!S->isGroupKind())
      ThisConfig.raise(Diag::expected_group_section)
          << S->getDecoratedName(ThisConfig.options());
    assignOffset(S);
  }
}

bool ObjectLinker::parseListFile(std::string Filename, uint32_t Type) {
  LayoutInfo *layoutInfo = ThisModule->getLayoutInfo();
  Input *SymbolListInput =
      eld::make<Input>(Filename, ThisConfig.getDiagEngine(), Input::Script);
  if (!SymbolListInput->resolvePath(ThisConfig))
    return false;
  // Create an Input file and set the input file to be of kind ExternList
  InputFile *SymbolListInputFile =
      InputFile::create(SymbolListInput, InputFile::GNULinkerScriptKind,
                        ThisConfig.getDiagEngine());
  eld::MappingFile::Kind K = MappingFile::Other;
  if (Type == ScriptFile::ExternList)
    K = MappingFile::ExternList;
  else if (Type == ScriptFile::DynamicList)
    K = MappingFile::DynamicList;
  SymbolListInputFile->setMappingFileKind(K);
  addInputFileToTar(SymbolListInputFile, K);
  SymbolListInput->setInputFile(SymbolListInputFile);
  // Record the dynamic list script in the Map file.
  if (layoutInfo)
    layoutInfo->recordLinkerScript(SymbolListInput->decoratedPath());
  // Read the dynamic List file
  ScriptFile SymbolListReader(
      (ScriptFile::Kind)Type, *ThisModule,
      *(llvm::dyn_cast<eld::LinkerScriptFile>(SymbolListInputFile)),
      ThisModule->getIRBuilder()->getInputBuilder());
  bool ParseSuccess =
      getScriptReader()->readScript(ThisConfig, SymbolListReader);
  if (!ParseSuccess)
    return false;
  if (Type == ScriptFile::DynamicList && SymbolListReader.getDynamicList()) {
    for (const auto &Sym : *SymbolListReader.getDynamicList()) {
      ThisModule->addScriptSymbolForDynamicListFile(SymbolListInputFile, Sym);
      ThisModule->dynListSyms().push_back(Sym);
    }
  }
  if (Type == ScriptFile::ExternList || Type == ScriptFile::DynamicList) {
    if (!SymbolListReader.activate(*ThisModule))
      return false;
  } else
    for (const auto &Sym : SymbolListReader.getExternList()) {
      if (Type == ScriptFile::DuplicateCodeList)
        ThisModule->addToCopyFarCallSet(Sym->name());
      else if (Type == ScriptFile::NoReuseTrampolineList)
        ThisModule->addToNoReuseOfTrampolines(Sym->name());
    }
  return true;
}

void ObjectLinker::addDuplicateCodeInsteadOfTrampolines() {
  if (ThisConfig.options().hasNoCopyFarCallsFromFile())
    return;
  parseListFile(ThisConfig.options().copyFarCallsFromFile(),
                ScriptFile::DuplicateCodeList);
}

void ObjectLinker::addNoReuseOfTrampolines() {
  if (ThisConfig.options().hasNoReuseOfTrampolinesFile())
    return;
  parseListFile(ThisConfig.options().noReuseOfTrampolinesFile(),
                ScriptFile::NoReuseTrampolineList);
}

/// addUndefSymbols - add any symbols specified by the -u flag
///   @return true if symbols added
bool ObjectLinker::addUndefSymbols() {
  GeneralOptions::const_ext_list_iterator It =
      ThisConfig.options().extListBegin();
  GeneralOptions::const_ext_list_iterator Ie =
      ThisConfig.options().extListEnd();

  for (; It != Ie; It++) {
    if (!parseListFile(*It, ScriptFile::ExternList))
      return false;
  }

  LDSymbol *OutputSym = nullptr;
  Resolver::Result Result;
  // Add the entry symbol.
  if (ThisConfig.options().hasEntry()) {
    if (string::isValidCIdentifier(ThisConfig.options().entry()))
      ThisConfig.options().getUndefSymList().emplace_back(
          eld::make<StrToken>(ThisConfig.options().entry()));
  }

  if (!ThisConfig.options().getUndefSymList().empty()) {
    GeneralOptions::const_undef_sym_iterator UndefSym,
        UndefSymEnd = ThisConfig.options().undefSymEnd();
    for (UndefSym = ThisConfig.options().undefSymBegin();
         UndefSym != UndefSymEnd; ++UndefSym) {
      InputFile *I = ThisModule->getInternalInput(
          eld::Module::InternalInputType::ExternList);
      ThisModule->getNamePool().insertSymbol(
          I, (*UndefSym)->name(), false, eld::ResolveInfo::NoType,
          eld::ResolveInfo::Undefined, eld::ResolveInfo::Global, 0, 0,
          eld::ResolveInfo::Default, nullptr, Result,
          false /* isPostLTOPhase */, false, 0, false /* isPatchable */,
          ThisModule->getPrinter());
      // create a output LDSymbol. All external symbols are entry symbols.
      OutputSym = make<LDSymbol>(Result.Info, false);
      Result.Info->setOutSymbol(OutputSym);
      // Initialize origin.
      Result.Info->setResolvedOrigin(I);
      ThisModule->addNeededSymbol((*UndefSym)->name());
    }
  }

  auto &DynExpSymbols = ThisConfig.options().getExportDynSymList();
  for (const auto &S : DynExpSymbols) {
    InputFile *I = ThisModule->getInternalInput(
        eld::Module::InternalInputType::DynamicExports);
    ThisModule->getNamePool().insertSymbol(
        I, S->name(), false, eld::ResolveInfo::NoType,
        eld::ResolveInfo::Undefined, eld::ResolveInfo::Global, 0, 0,
        eld::ResolveInfo::Default, NULL, Result, false /* isPostLTOPhase */,
        false, 0, false /* isPatchable */, ThisModule->getPrinter());
    // create a output LDSymbol. All external symbols are entry symbols.
    OutputSym = make<LDSymbol>(Result.Info, false);
    Result.Info->setOutSymbol(OutputSym);
    // Initialize origin.
    Result.Info->setResolvedOrigin(I);
    ThisModule->addNeededSymbol(S->name());
  }
  return true;
}

bool ObjectLinker::addSymbolToOutput(const ResolveInfo &PInfo) const {
  // Dont add bitcode symbols to output.
  if (PInfo.isBitCode())
    return false;

  // Dont add section symbols.
  if (PInfo.isSection())
    return false;

  // Dont add internal symbols.
  if (PInfo.visibility() == ResolveInfo::Internal)
    return false;

  // Dont add undefined symbols, that are garbage collected
  if (PInfo.isUndef() && PInfo.outSymbol() && PInfo.outSymbol()->shouldIgnore())
    return false;

  // If its the dot symbol, dont add to output.
  if (PInfo.outSymbol() && PInfo.outSymbol() == ThisModule->getDotSymbol())
    return false;

  // Dont add symbols for which we dont have an outsymbol.
  if (nullptr == PInfo.outSymbol())
    return false;

  if (PInfo.outSymbol()->fragRef() && PInfo.outSymbol()->fragRef()->isDiscard())
    return false;

  // if the symbols defined in the Ignore sections (e.g. discared by GC), then
  // not to put them to output
  if ((PInfo.outSymbol()->hasFragRef() && PInfo.getOwningSection()->isIgnore()))
    return false;

  // if the symbols defined in the Ignore sections (e.g. discared by Plugin),
  // then not to put them to output.
  if ((PInfo.outSymbol()->hasFragRef() &&
       PInfo.getOwningSection()->isDiscard()))
    return false;

  // Set the Used property.
  InputFile *ResolvedOrigin = PInfo.resolvedOrigin();

  if (ResolvedOrigin) {
    if (!PInfo.isFile())
      ResolvedOrigin->setUsed(true);
  }

  static InputFile *I = ThisModule->getInternalInput(
      eld::Module::InternalInputType::DynamicExports);
  if (PInfo.isUndef() && ResolvedOrigin && ResolvedOrigin == I)
    return false;

  if ((ResolvedOrigin && ResolvedOrigin->isInternal()) &&
      (!PInfo.isWeak() && PInfo.isUndef() &&
       (ThisConfig.options().getErrorStyle() == GeneralOptions::llvm))) {
    ThisConfig.raise(Diag::symbol_undefined_by_user)
        << PInfo.name() << ResolvedOrigin->getInput()->decoratedPath();
    return false;
  }

  // If this is a undefined reference to wrapper in LTO generated native
  // object, it is handled with references. It is now an artifact from its
  // bitcode file. So ignore this symbol
  if (PInfo.isUndef() && ResolvedOrigin &&
      ThisModule->hasWrapReference(PInfo.name()) && ResolvedOrigin->isBitcode())
    return false;

  // Let the backend choose to add the symbol to the output.
  if (!getTargetBackend().addSymbolToOutput(const_cast<ResolveInfo *>(&PInfo)))
    return false;

  return true;
}

bool ObjectLinker::mayBeAddSectionSymbol(ELFSection *S) {
  if (S->isRelocationKind())
    return false;
  InputFile *I =
      ThisModule->getInternalInput(Module::InternalInputType::Sections);
  if (!S->isWanted() && !S->size())
    return false;
  ResolveInfo *SymInfo = ThisModule->getNamePool().createSymbol(
      I, S->name().str(), false, ResolveInfo::Section, ResolveInfo::Define,
      ResolveInfo::Local,
      0x0, // size
      ResolveInfo::Default, false /*isPostLTOPhase*/);
  LDSymbol *Sym = make<LDSymbol>(SymInfo, false);
  SymInfo->setOutSymbol(Sym);
  Fragment *F = S->getOutputSection()->getFirstFrag();
  FragmentRef *FragRef = FragmentRef::null();
  if (!F)
    return false;
  FragRef = make<FragmentRef>(*F, 0);
  Sym->setFragmentRef(FragRef);
  // Record the section for lookup.
  ThisModule->recordSectionSymbol(S, SymInfo);
  // Add to the symbol table.
  ThisModule->addSymbol(SymInfo);
  return true;
}

bool ObjectLinker::addSectionSymbols() {
  // create and add section symbols for each output section
  for (auto &S : ThisModule->getScript().sectionMap())
    mayBeAddSectionSymbol(S->getSection());
  for (auto &S : *ThisModule)
    mayBeAddSectionSymbol(S);
  return true;
}

bool ObjectLinker::addSymbolsToOutput() {

  // Traverse all the free ResolveInfo and add the output symobols to output
  for (auto &L : ThisModule->getNamePool().getLocals()) {
    accountSymForTotalSymStats(*L);
    if (addSymbolToOutput(*L))
      ThisModule->addSymbol(L);
    else
      accountSymForDiscardedSymStats(*L);
  }

  // We should not be adding any symbol from dynamic shared libraries into
  // getGlobals.
  // Traverse all the resolveInfo and add the output symbol to output
  for (auto &G : ThisModule->getNamePool().getGlobals()) {
    ResolveInfo *R = G.getValue();
    accountSymForTotalSymStats(*R);
    if (addSymbolToOutput(*R))
      ThisModule->addSymbol(R);
    else
      accountSymForDiscardedSymStats(*R);
  }

  // The option --unresolved-symbols is ignored for partial linking
  if (LinkerConfig::Object == ThisConfig.codeGenType())
    return true;

  // Note the string version is checked here. This is to make sure the default
  // behavior is not affected by this functionality. The default behavior is
  // really used by so many customers and lets not break it.
  if (ThisConfig.options().reportUndefPolicy().empty() ||
      (ThisConfig.options().reportUndefPolicy() == "ignore-all"))
    return true;

  bool HasError = false;
  for (auto &G : ThisModule->getNamePool().getGlobals()) {
    ResolveInfo *R = G.getValue();
    bool NeedSymbolInOutput = addSymbolToOutput(*R);
    // All dynamic symbols in shared libraries are processed here.
    if (!NeedSymbolInOutput) {
      // Skip if the symbol is not dynamic.
      if (!R->isDyn())
        continue;
      // Unfortunately we need to check again.
      if (R->outSymbol() && R->outSymbol() == ThisModule->getDotSymbol())
        continue;
      // If the symbol is undef and not weak, and the symbol needs to be
      // reported, lets report it
      if (R->isUndef() && !R->isWeak()) {
        if (!ThisModule->getLinker()->shouldIgnoreUndefine(R->isDyn())) {
          HasError = true;
          ThisConfig.raise(Diag::undefined_symbol)
              << R->name() << R->resolvedOrigin()->getInput()->decoratedPath();
        }
      }
      continue;
    }
    // If the undefined symbol needs to be reported because the linker
    // was passed an option for unresolved symbol policy, lets do that.
    if (getTargetBackend().canIssueUndef(R)) {
      HasError = true;
      ThisConfig.raise(Diag::undefined_symbol)
          << R->name() << R->resolvedOrigin()->getInput()->decoratedPath();
    }
  }

  if (HasError)
    ThisModule->setFailure(true);

  return !HasError;
}

/// addStandardSymbols - shared object and executable files need some
/// standard symbols
///   @return if there are some input symbols with the same name to the
///   standard symbols, return false
bool ObjectLinker::addStandardSymbols() {
  return getTargetBackend().initStandardSymbols();
}

/// addTargetSymbols - some targets, such as MIPS and ARM, need some
/// target-dependent symbols
///   @return if there are some input symbols with the same name to the
///   target symbols, return false
bool ObjectLinker::addTargetSymbols() {
  getTargetBackend().initTargetSymbols();
  return true;
}

bool ObjectLinker::processInputFiles() {
  std::vector<InputFile *> Inputs;
  getInputs(Inputs);
  return getTargetBackend().processInputFiles(Inputs);
}

/// addScriptSymbols - define symbols from the command line option or linker
/// scripts.
bool ObjectLinker::addScriptSymbols() {

  LinkerScript &Script = ThisModule->getScript();

  uint64_t SymValue = 0x0;
  LDSymbol *Symbol = nullptr;
  bool Ret = true;
  // go through the entire symbol assignments
  for (auto &AssignCmd : Script.assignments()) {
    InputFile *ScriptInput = ThisModule->getInternalInput(eld::Module::Script);
    // FIXME: Ideally, assignCmd should always have a context. We should perhaps
    // add an internal error if the context is missing.
    if (AssignCmd->hasInputFileInContext())
      ScriptInput = AssignCmd->getInputFileInContext();
    llvm::StringRef SymName = AssignCmd->name();
    ResolveInfo::Type Type = ResolveInfo::NoType;
    ResolveInfo::Visibility Vis = ResolveInfo::Default;
    size_t Size = 0;
    ResolveInfo *OldInfo = ThisModule->getNamePool().findInfo(SymName.str());
    Symbol = nullptr;

    if (AssignCmd->isProvideOrProvideHidden()) {
      if (getTargetBackend().isSymInProvideMap(SymName)) {
        if (ThisConfig.showLinkerScriptWarnings())
          ThisConfig.raise(Diag::warn_provide_sym_redecl)
              << AssignCmd->getContext() << SymName;
        continue;
      }
      getTargetBackend().addProvideSymbol(SymName, AssignCmd);
      if (!ThisModule->getAssignmentForSymbol(SymName))
        ThisModule->addAssignment(SymName, AssignCmd);
      continue;
    }
    // if the symbol does not exist, we can set type to NOTYPE
    // else we retain its type, same goes for size - 0 or retain old value
    // and visibility - Default or retain
    if (OldInfo != nullptr) {
      Type = static_cast<ResolveInfo::Type>(OldInfo->type());
      Vis = OldInfo->visibility();
      Size = OldInfo->size();

      if (OldInfo->outSymbol() && OldInfo->outSymbol()->hasFragRefSection()) {
        if (OldInfo->isPatchable())
          ThisConfig.raise(Diag::error_patchable_script)
              << OldInfo->outSymbol()->name();
      }
    }
    PluginManager &PM = ThisModule->getPluginManager();
    SymbolInfo SymInfo(ScriptInput, Size, ResolveInfo::Absolute, Type, Vis,
                       ResolveInfo::Define,
                       /*isBitCode=*/false);
    // We do not create input symbols for non object file symbols!
    DiagnosticPrinter *DP = ThisConfig.getPrinter();
    auto OldErrorCount = DP->getNumErrors() + DP->getNumFatalErrors();
    PM.callVisitSymbolHook(/*sym=*/nullptr, SymName, SymInfo);
    auto NewErrorCount = DP->getNumErrors() + DP->getNumFatalErrors();
    if (NewErrorCount > OldErrorCount)
      Ret = false;
    // Add symbol and refine the visibility if needed
    switch (AssignCmd->type()) {
    case Assignment::HIDDEN:
      Vis = ResolveInfo::Hidden;
      LLVM_FALLTHROUGH;
    // Fall through
    case Assignment::DEFAULT: {
      LLVM_FALLTHROUGH;
    case Assignment::FILL:
      // Add an absolute symbol
      if (!AssignCmd->isDot())
        Symbol =
            ThisModule->getIRBuilder()
                ->addSymbol<eld::IRBuilder::Force, eld::IRBuilder::Unresolve>(
                    ScriptInput, SymName.str(), Type, ResolveInfo::Define,
                    ResolveInfo::Absolute, Size, SymValue, FragmentRef::null(),
                    Vis, true /*PostLTOPhase*/);
      LLVM_FALLTHROUGH;
    case Assignment::ASSERT:
      AssignCmd->setUsed(true);
      break;
    }
    case Assignment::PROVIDE_HIDDEN:
    case Assignment::PROVIDE:
      continue;
    }
    if (Symbol) {
      Symbol->setShouldIgnore(false);
      Symbol->resolveInfo()->setResolvedOrigin(ScriptInput);
      Symbol->setScriptDefined();
      Symbol->resolveInfo()->setInBitCode(false);
    }
    // Record that there was an assignment for this symbol.
    // If there is a relocation to this symbol, the symbols contained in the
    // assignment also need to be considered as part of the list of symbols
    // that will be live.
    if (Symbol)
      ThisModule->addAssignment(Symbol->resolveInfo()->name(), AssignCmd);
  }

  return Ret;
}

bool ObjectLinker::addDynListSymbols() {
  // Dynamic list is only for non-shared libraries that use dynamic symbol
  // tables
  if (ThisConfig.codeGenType() == LinkerConfig::DynObj &&
      !ThisConfig.options().isPIE())
    return true;
  bool NoError = true;
  std::vector<ScriptSymbol *> &DynListSyms = ThisModule->dynListSyms();
  std::unordered_set<ScriptSymbol *> UsedPatterns;
  for (auto &G : ThisModule->getNamePool().getGlobals()) {
    ResolveInfo *R = G.getValue();
    LDSymbol *Sym = ThisModule->getNamePool().findSymbol(R->name());
    bool ShouldExport = !R->isUndef() &&
                        R->visibility() == ResolveInfo::Default && Sym &&
                        !Sym->shouldIgnore();
    if (MDynlistExports.count(R->name())) {
      if (ShouldExport)
        R->setExportToDyn();
      continue;
    }
    for (auto &Pattern : DynListSyms) {
      if (Pattern->matched(*R)) {
        if (ShouldExport)
          R->setExportToDyn();
        UsedPatterns.insert(Pattern);
        break;
      }
    }
  }
  if (ThisConfig.options().getErrorStyle() == GeneralOptions::llvm) {
    for (const auto &E : DynListSyms)
      if (!UsedPatterns.count(E)) {
        NoError = false;
        ThisConfig.raise(Diag::dynlist_symbol_undefined_by_user) << E->name();
      }
  }
  // No further processing is done for dynamic list symbols
  DynListSyms.clear();
  MDynlistExports.clear();
  return NoError;
}

size_t ObjectLinker::getRelocSectSize(uint32_t Type, uint32_t Count) {
  return Count * (Type == llvm::ELF::SHT_RELA
                      ? getTargetBackend().getRelaEntrySize()
                      : getTargetBackend().getRelEntrySize());
}

void ObjectLinker::createRelocationSections() {

  if (!ThisConfig.options().emitRelocs())
    return;

  // SectionName, contentPermissions, filepath
  struct SectionKey {
    SectionKey() : Name(""), Type(-1) {}

    SectionKey(llvm::StringRef Name, int32_t Type) : Name(Name), Type(Type) {}

    // Data members
    StringRef Name;
    int32_t Type;
  };

  struct SectionKeyInfo {
    static SectionKey getEmptyKey() { return SectionKey(); }
    static SectionKey getTombstoneKey() { return SectionKey(); }
    static unsigned getHashValue(const SectionKey &K) {
      return llvm::hash_combine(K.Name, K.Type);
    }
    static bool isEqual(const SectionKey &Lhs, const SectionKey &Rhs) {
      return ((Lhs.Name == Rhs.Name) && (Lhs.Type == Rhs.Type));
    }
  };

  // apply all relocations of all inputs
  llvm::DenseMap<SectionKey, uint32_t, SectionKeyInfo> OutputRelocCount;
  llvm::DenseMap<SectionKey, uint32_t, SectionKeyInfo> OutputRelocAlignment;
  llvm::DenseMap<SectionKey, ELFSection *, SectionKeyInfo>
      OutputRelocTargetSect;

  eld::Module::obj_iterator Input, InEnd = ThisModule->objEnd();
  for (Input = ThisModule->objBegin(); Input != InEnd; ++Input) {
    ELFObjectFile *ObjFile = llvm::dyn_cast<ELFObjectFile>(*Input);
    if (!ObjFile)
      continue;
    for (auto &Rs : ObjFile->getRelocationSections()) {
      if (Rs->isIgnore())
        continue;
      if (Rs->isDiscard())
        continue;

      for (auto &Relocation : Rs->getLink()->getRelocations()) {
        if (getTargetBackend().maySkipRelocProcessing(Relocation))
          continue;

        auto CountRelocEntries = [&](class Relocation *Relocation) -> void {
          ELFSection *OutputSect =
              Relocation->targetRef()->frag()->getOwningSection();

          if (OutputSect->getOutputSection())
            OutputSect = OutputSect->getOutputSection()->getSection();

          ELFSection *TargetSection = Relocation->targetSection();

          if (TargetSection &&
              (!TargetSection->isDiscard() && !TargetSection->isIgnore())) {
            ELFSection *TargetOutputSection =
                TargetSection->getOutputELFSection();
            TargetOutputSection->setWantedInOutput();
          }

          // Count the num of entries that refers to that section.
          SectionKey SectKey(OutputSect->name(),
                             getTargetBackend().getRelocator()->relocType());
          OutputRelocCount[SectKey] += 1;
          OutputRelocAlignment[SectKey] =
              std::max(Rs->getAddrAlign(), OutputRelocAlignment[SectKey]);
          OutputRelocTargetSect[SectKey] = OutputSect;
        };
        CountRelocEntries(Relocation);
      } // for all relocations
    } // for all relocation section
  } // for all inputs
  for (const auto &Kv : OutputRelocCount) {
    ELFSection *OutputSect = ThisModule->createOutputSection(
        getTargetBackend().getOutputRelocSectName(Kv.first.Name.str(),
                                                  Kv.first.Type),
        LDFileFormat::Relocation, Kv.first.Type /* Reloc Kind */, 0x0,
        OutputRelocAlignment[Kv.first]);
    OutputSect->setEntSize(Kv.first.Type == llvm::ELF::SHT_RELA
                               ? getTargetBackend().getRelaEntrySize()
                               : getTargetBackend().getRelEntrySize());
    OutputRelocTargetSect[Kv.first]->setWantedInOutput(true);
    OutputSect->setLink(OutputRelocTargetSect[Kv.first]);
    OutputSect->setSize(getRelocSectSize(Kv.first.Type, Kv.second));
  }
}

namespace {

void addCopyReloc(GNULDBackend &LDBackend, ResolveInfo &Sym,
                  Relocation::Type Type) {
  ASSERT(Sym.outSymbol()->hasFragRef(), "Unresolved copy relocation");
  // Copy relocations are created in the global section.
  ELFSection *S = LDBackend.getRelaDyn();
  Relocation &R = *S->createOneReloc();
  R.setType(Type);
  R.setTargetRef(Sym.outSymbol()->fragRef());
  R.setSymInfo(&Sym);
}

} // namespace

void ObjectLinker::createCopyRelocation(ResolveInfo &Sym,
                                        Relocation::Type Type) {
  // If there are multiple relocation for one symbol, the copy relocation is
  // created only for the first one. The symbol will be resolved to a local
  // symbol and all subsequent relocation will be for this local symbol.
  if (!Sym.isDyn())
    return;

  ResolveInfo *AliasSym = Sym.alias();

  // If the symbol thats externally used is the alias symbol and not the
  // original symbol, the linker needs to export the original symbol and
  // make the alias symbl point to the original symbol.
  if (AliasSym && !AliasSym->outSymbol()) {
    LDSymbol &GlobalSymbol = getTargetBackend().defineSymbolforCopyReloc(
        *ThisModule->getIRBuilder(), AliasSym, &Sym);
    GlobalSymbol.resolveInfo()->setResolvedOrigin(Sym.resolvedOrigin());
    addCopyReloc(getTargetBackend(), *GlobalSymbol.resolveInfo(), Type);
  }
  LDSymbol &CopySym = getTargetBackend().defineSymbolforCopyReloc(
      *ThisModule->getIRBuilder(), &Sym, &Sym);
  if (!AliasSym)
    addCopyReloc(getTargetBackend(), *CopySym.resolveInfo(), Type);
}

void ObjectLinker::scanRelocationsHelper(InputFile *Input, bool IsPartialLink,
                                         LinkerScript::PluginVectorT PVect,
                                         Relocator::CopyRelocs &CopyRelocs) {
  ELFObjectFile *ObjFile = llvm::dyn_cast<ELFObjectFile>(Input);
  if (!ObjFile)
    return;
  uint32_t NumPlugins = PVect.size();
  for (auto &Rs : ObjFile->getRelocationSections()) {
    if (Rs->isIgnore())
      continue;
    if (Rs->isDiscard())
      continue;
    for (auto &Relocation : Rs->getLink()->getRelocations()) {
      // Skip unneeded relocations
      if (getTargetBackend().maySkipRelocProcessing(Relocation))
        continue;
      if (NumPlugins) {
        for (auto &P : PVect)
          P->callReloc(Relocation->type(), Relocation);
      }
      ELFSection *RelocSection = Rs;
      // scan relocation
      if (!IsPartialLink)
        getTargetBackend().getRelocator()->scanRelocation(
            *Relocation, *ThisModule->getIRBuilder(), *RelocSection, *Input,
            CopyRelocs);
      else
        getTargetBackend().getRelocator()->partialScanRelocation(*Relocation,
                                                                 *RelocSection);
    } // for all relocations
  } // for all relocation section
}

LinkerScript::PluginVectorT ObjectLinker::getLinkerPluginWithLinkerConfigs() {
  const LinkerScript::PluginVectorT PluginVect =
      ThisModule->getScript().getPlugins();
  LinkerScript::PluginVectorT PluginVectHavingLinkerConfigs;
  for (auto &P : PluginVect) {
    if (P->getLinkerPluginConfig())
      PluginVectHavingLinkerConfigs.push_back(P);
  }
  return PluginVectHavingLinkerConfigs;
}

bool ObjectLinker::scanRelocations(bool IsPartialLink) {
  LinkerScript::PluginVectorT PluginVect = getLinkerPluginWithLinkerConfigs();

  getTargetBackend().provideSymbols();

  std::vector<std::unique_ptr<Relocator::CopyRelocs>> AllCopyRelocs;
  if (ThisConfig.options().numThreads() <= 1 ||
      !ThisConfig.isScanRelocationsMultiThreaded()) {
    if (ThisModule->getPrinter()->traceThreads())
      ThisConfig.raise(Diag::threads_disabled) << "ScanRelocations";
    for (auto &Input : ThisModule->getObjectList()) {
      auto CopyRelocs = std::make_unique<Relocator::CopyRelocs>();
      scanRelocationsHelper(Input, IsPartialLink, PluginVect, *CopyRelocs);
      AllCopyRelocs.push_back(std::move(CopyRelocs));
    }
  } else {
    if (ThisModule->getPrinter()->traceThreads())
      ThisConfig.raise(Diag::threads_enabled)
          << "ScanRelocations" << ThisConfig.options().numThreads();
    llvm::ThreadPoolInterface *Pool = ThisModule->getThreadPool();
    for (auto &Input : ThisModule->getObjectList()) {
      auto CopyRelocs = std::make_unique<Relocator::CopyRelocs>();
      auto &CopyRelocsRef = *CopyRelocs; // must dereference in the main thread
      Pool->async([&] {
        scanRelocationsHelper(Input, IsPartialLink, PluginVect, CopyRelocsRef);
      });
      AllCopyRelocs.push_back(std::move(CopyRelocs));
    }
    Pool->wait();
  }
  // assume there is only one copy relocation type per target
  Relocation::Type CopyRelocType = getTargetBackend().getCopyRelType();
  for (const auto &RelocVec : AllCopyRelocs)
    for (auto &Reloc : *RelocVec)
      createCopyRelocation(*Reloc, CopyRelocType);

  // Merge per-file relocations
  if (!IsPartialLink) {
    ELFObjectFile *RelocInput =
        getTargetBackend().getDynamicSectionHeadersInputFile();
    auto MergeRelocs = [](llvm::SmallVectorImpl<Relocation *> &To,
                          const llvm::SmallVectorImpl<Relocation *> &From) {
      To.insert(To.end(), From.begin(), From.end());
    };
    for (auto &Input : ThisModule->getObjectList())
      if (ELFObjectFile *Obj = llvm::dyn_cast<ELFObjectFile>(Input))
        if (Obj != RelocInput) {
          if (const auto &S = Obj->getRelaDyn())
            MergeRelocs(RelocInput->getRelaDyn()->getRelocations(),
                        S->getRelocations());
          if (const auto &S = Obj->getRelaPLT())
            MergeRelocs(RelocInput->getRelaPLT()->getRelocations(),
                        S->getRelocations());
        }
  }

  // If there is a undefined symbol, fail the link. No point fixing the
  // relocations. This is overridden by --noinhibit-exec.
  if (!ThisConfig.getDiagEngine()->diagnose()) {
    if (ThisModule->getPrinter()->isVerbose())
      ThisConfig.raise(Diag::function_has_error) << __PRETTY_FUNCTION__;
    return false;
  }
  return true;
}

bool ObjectLinker::finalizeScanRelocations() {
  getTargetBackend().finalizeScanRelocations();
  if (!ThisConfig.getDiagEngine()->diagnose()) {
    if (ThisModule->getPrinter()->isVerbose())
      ThisConfig.raise(Diag::function_has_error) << __PRETTY_FUNCTION__;
    return false;
  }
  return true;
}

/// initStubs - initialize stub-related stuff.
bool ObjectLinker::initStubs() {
  // initialize BranchIslandFactory
  getTargetBackend().initBRIslandFactory();

  // initialize StubFactory
  getTargetBackend().initStubFactory();

  // initialize target stubs
  getTargetBackend().initTargetStubs();
  return true;
}

bool ObjectLinker::allocateCommonSymbols() {
  for (auto &G : ThisModule->getNamePool().getGlobals()) {
    ResolveInfo *R = G.getValue();
    if (R->isCommon() && !R->isBitCode())
      ThisModule->addCommonSymbol(R);
  }

  // Do not allocate common symbols for partial link unless define common
  // option is provided.
  if (LinkerConfig::Object == ThisConfig.codeGenType() &&
      !ThisConfig.options().isDefineCommon())
    return true;
  if (!ThisModule->sortCommonSymbols())
    return false;
  return getTargetBackend().allocateCommonSymbols();
}

bool ObjectLinker::addDynamicSymbols() {
  getTargetBackend().sizeDynNamePools();
  return true;
}

/// prelayout - help backend to do some modification before layout
bool ObjectLinker::prelayout() {
  getTargetBackend().preLayout();

  if (!ThisConfig.getDiagEngine()->diagnose())
    return false;

  getTargetBackend().sizeDynamic();

  getTargetBackend().initSymTab();

  createRelocationSections();

  return true;
}

/// layout - linearly layout all output sections and reserve some space
/// for GOT/PLT
///   Because we do not support instruction relaxing in this early version,
///   if there is a branch can not jump to its target, we return false
///   directly
bool ObjectLinker::layout() { return getTargetBackend().layout(); }

/// prelayout - help backend to do some modification after layout
bool ObjectLinker::postlayout() { return getTargetBackend().postLayout(); }

bool ObjectLinker::printlayout() { return getTargetBackend().printLayout(); }

bool ObjectLinker::finalizeBeforeWrite() {
  getTargetBackend().finalizeBeforeWrite();
  return true;
}

/// finalizeSymbolValue - finalize the resolved symbol value.
///   Before relocate(), after layout(), ObjectLinker should correct value of
///   all
///   symbol.
bool ObjectLinker::finalizeSymbolValues() {
  for (auto *I : ThisModule->getSymbols())
    finalizeSymbolValue(I);
  return true;
}

/// Finalize one symbol.
void ObjectLinker::finalizeSymbolValue(ResolveInfo *I) const {
  if (I->isAbsolute() || I->type() == ResolveInfo::File)
    return;

  if (I->type() == ResolveInfo::ThreadLocal) {
    I->outSymbol()->setValue(
        getTargetBackend().finalizeTLSSymbol(I->outSymbol()));
    return;
  }

  if (I->outSymbol()->hasFragRef() &&
      I->outSymbol()->fragRef()->frag()->getOwningSection()->isDiscard())
    return;

  if (I->outSymbol()->hasFragRef()) {
    // set the virtual address of the symbol. If the output file is
    // relocatable object file, the section's virtual address becomes zero.
    // And the symbol's value become section relative offset.
    uint64_t Value = I->outSymbol()->fragRef()->getOutputOffset(*ThisModule);
    assert(nullptr != I->outSymbol()->fragRef()->frag());
    // For sections that are zero in size, there is no output section. Just
    // rely on the owning section name for now.
    ELFSection *Section = I->getOwningSection();
    if (Section->getOutputSection())
      Section = Section->getOutputSection()->getSection();
    if (Section->isAlloc())
      I->outSymbol()->setValue(Value + Section->addr());
    else
      I->outSymbol()->setValue(Value);
  }
}

/// relocate - applying relocation entries and create relocation
/// section in the output files
/// Create relocation section, asking GNULDBackend to
/// read the relocation information into RelocationEntry
/// and push_back into the relocation section
bool ObjectLinker::relocation(bool EmitRelocs) {
  // when producing relocatables, no need to apply relocation
  if (LinkerConfig::Object == ThisConfig.codeGenType())
    return true;

  // Mapping section to count and max_size.
  llvm::DenseMap<ELFSection *, unsigned> RelocCount, MaxSectSize;

  getTargetBackend().getRelocator()->computeTLSOffsets();

  auto EmitOneReloc = [&](Relocation *Relocation) -> bool {
    if (!EmitRelocs)
      return true;
    ResolveInfo *Info = Relocation->symInfo();
    ELFSection *OutputSect =
        ThisModule->getSection(getTargetBackend().getOutputRelocSectName(
            Relocation->targetRef()->getOutputELFSection()->name().str(),
            getTargetBackend().getRelocator()->relocType()));

    if (!OutputSect)
      return true;

    // Make sure we have an entry for each section because it's possible
    // that the whole section has no any valid relocs.
    if (RelocCount.find(OutputSect) == RelocCount.end()) {
      RelocCount[OutputSect] = 0;
      MaxSectSize[OutputSect] = OutputSect->size();
    }

    Relocation::Type ExtType =
        getTargetBackend().getRemappedInternalRelocationType(
            Relocation->type());

    if (ExtType != Relocation->type() &&
        ThisModule->getPrinter()->isVerbose()) {
      ThisConfig.raise(Diag::verbose_remapped_internal_reloc)
          << getTargetBackend().getRelocator()->getName(Relocation->type())
          << getTargetBackend().getRelocator()->getName(ExtType)
          << Relocation->getSourcePath(ThisConfig.options());
    }

    eld::Relocation *R = eld::Relocation::Create(
        ExtType, getTargetBackend().getRelocator()->getSize(Relocation->type()),
        Relocation->targetRef(), Relocation->addend());

    ResolveInfo *SymInfo = Info;

    if (Info->type() == ResolveInfo::Section) {
      SymInfo = ThisModule->getSectionSymbol(Relocation->outputSection());
      // Get symbol offset from the relocation itself.
      if (!ThisConfig.options().emitGNUCompatRelocs()) {
        R->setAddend(Relocation->symOffset(*ThisModule));
      }
    }

    // set relocation target symbol to the output section symbol's
    // resolveInfo
    R->setSymInfo(SymInfo);
    OutputSect->addRelocation(R);
    RelocCount[OutputSect] += 1;
    return true;
  };

  auto ProcessObjectFile = [&](ObjectFile *ObjFile) -> bool {
    for (auto &Sect : ObjFile->getSections()) {
      if (Sect->isBitcode())
        continue;
      ELFSection *Section = llvm::dyn_cast<eld::ELFSection>(Sect);
      if (!Section->hasRelocData())
        continue;
      if (Section->getKind() != LDFileFormat::Internal)
        continue;
      // Skip internal relocation sections.
      if (Section->isRelocationSection())
        continue;
      for (auto &Relocation : Section->getRelocations()) {
        Relocation->apply(*getTargetBackend().getRelocator());
      }
    }

    if (!ThisConfig.getDiagEngine()->diagnose()) {
      return false;
    }

    ELFFileBase *EObjFile = llvm::dyn_cast<ELFFileBase>(ObjFile);

    if (!EObjFile)
      return false;

    for (auto &Rs : EObjFile->getRelocationSections()) {
      if (Rs->isIgnore())
        continue;
      if (Rs->isDiscard())
        continue;
      auto ProcessReloc = [&](Relocation *Relocation) -> bool {
        // bypass the reloc if the symbol is in the discarded input section
        ResolveInfo *Info = Relocation->symInfo();

        // Dont process relocations that are relaxed.
        if (getTargetBackend().isRelocationRelaxed(Relocation))
          return false;

        if (!Info->outSymbol()->hasFragRef() &&
            ResolveInfo::Section == Info->type() &&
            ResolveInfo::Undefined == Info->desc())
          return false;

        if ((Info->outSymbol()->hasFragRef() && Info->outSymbol()
                                                    ->fragRef()
                                                    ->frag()
                                                    ->getOwningSection()
                                                    ->isDiscard()))
          return false;

        ELFSection *ApplySect =
            Relocation->targetRef()->frag()->getOwningSection();
        // bypass the reloc if the section where it sits will be discarded.
        if (ApplySect->isIgnore())
          return false;
        if (ApplySect->isDiscard())
          return false;
        ELFSection *TargetSect = Relocation->targetSection();
        if (Info->outSymbol()->shouldIgnore() ||
            (Info->outSymbol()->fragRef() &&
             Info->outSymbol()->fragRef()->isDiscard()) ||
            (TargetSect && TargetSect->isIgnore())) {
          if (ThisModule->getPrinter()->isVerbose())
            ThisConfig.raise(Diag::applying_endof_image_address)
                << Info->name()
                << Relocation->getTargetPath(ThisConfig.options())
                << Relocation->getSourcePath(ThisConfig.options());
          Relocation->target() =
              getTargetBackend().getValueForDiscardedRelocations(Relocation);
          return false;
        }
        return true;
      };
      for (auto &Relocation : Rs->getLink()->getRelocations()) {
        if (ProcessReloc(Relocation)) {
          if (EmitRelocs)
            EmitOneReloc(Relocation);
          Relocation->apply(*getTargetBackend().getRelocator());
        }
      } // for all relocations
    } // for all relocation section

    if (!ThisConfig.getDiagEngine()->diagnose()) {
      return false;
    }

    if (ThisConfig.options().emitRelocs()) {
      for (auto &Kv : RelocCount)
        Kv.first->setSize(getRelocSectSize(Kv.first->getType(), Kv.second));
    }
    return true;
  };

  // apply all relocations of all inputs
  if (ThisConfig.options().numThreads() <= 1 ||
      !ThisConfig.isApplyRelocationsMultiThreaded()) {
    if (ThisModule->getPrinter()->traceThreads())
      ThisConfig.raise(Diag::threads_disabled) << "ApplyRelocations";
    for (auto &Input : ThisModule->getObjectList()) {
      ObjectFile *ObjFile = llvm::dyn_cast<ObjectFile>(Input);
      if (!ObjFile)
        continue;
      ProcessObjectFile(ObjFile);
    }
  } else {
    if (ThisModule->getPrinter()->traceThreads())
      ThisConfig.raise(Diag::threads_enabled)
          << "ApplyRelocations" << ThisConfig.options().numThreads();
    llvm::ThreadPoolInterface *Pool = ThisModule->getThreadPool();
    for (auto &Input : ThisModule->getObjectList()) {
      Pool->async([&] {
        ObjectFile *ObjFile = llvm::dyn_cast<ObjectFile>(Input);
        if (!ObjFile)
          return;
        ProcessObjectFile(ObjFile);
      });
    }
    Pool->wait();
  }

  // apply relocations created by relaxation
  SectionMap::iterator Out, OutBegin, OutEnd;
  typedef std::vector<BranchIsland *>::iterator branch_island_iter;
  OutBegin = ThisModule->getScript().sectionMap().begin();
  OutEnd = ThisModule->getScript().sectionMap().end();
  for (Out = OutBegin; Out != OutEnd; ++Out) {
    branch_island_iter Bi = (*Out)->islandsBegin();
    branch_island_iter Be = (*Out)->islandsEnd();
    for (; Bi != Be; ++Bi) {
      BranchIsland::reloc_iterator Iter, IterEnd = (*Bi)->relocEnd();
      for (Iter = (*Bi)->relocBegin(); Iter != IterEnd; ++Iter) {
        (*Iter)->apply(*getTargetBackend().getRelocator());
      }
    }
  }

  // apply linker created relocations
  for (auto *R : getTargetBackend().getInternalRelocs()) {
    R->apply(*getTargetBackend().getRelocator());
  }

  // apply relocations
  if (!ThisConfig.getDiagEngine()->diagnose()) {
    return false;
  }
  return true;
}

/// emitOutput - emit the output file.
bool ObjectLinker::emitOutput(llvm::FileOutputBuffer &POutput) {
  return std::error_code() == getWriter()->writeObject(POutput);
}

/// postProcessing - do modification after all processes
eld::Expected<void>
ObjectLinker::postProcessing(llvm::FileOutputBuffer &POutput) {
  {
    eld::RegisterTimer T("Sync Relocations", "Emit Output File",
                         ThisConfig.options().printTimingStats());
    syncRelocations(POutput.getBufferStart());
  }

  {
    eld::RegisterTimer T("Post Process Output File", "Emit Output File",
                         ThisConfig.options().printTimingStats());
    eld::Expected<void> ExpPostProcess =
        getTargetBackend().postProcessing(POutput);
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(ExpPostProcess);
  }
  return {};
}

void ObjectLinker::syncRelocations(uint8_t *Buffer) {

  // We MUST write relocation by relaxation before those
  // from the inputs because something like R_AARCH64_COPY_INSN
  // must be written before the input relocation overwritten the same
  // location again.
  auto SyncBranchIslandsForOutputSection = [&](OutputSectionEntry *O) -> void {
    typedef std::vector<BranchIsland *>::iterator branch_island_iter;
    branch_island_iter Bi = O->islandsBegin();
    branch_island_iter Be = O->islandsEnd();
    for (; Bi != Be; ++Bi) {
      BranchIsland::reloc_iterator Iter, IterEnd = (*Bi)->relocEnd();
      for (Iter = (*Bi)->relocBegin(); Iter != IterEnd; ++Iter)
        writeRelocationResult(**Iter, Buffer);
    }
  };
  // sync relocations created by relaxation
  if (ThisConfig.options().numThreads() <= 1 ||
      !ThisConfig.isSyncRelocationsMultiThreaded()) {
    if (ThisModule->getPrinter()->traceThreads())
      ThisConfig.raise(Diag::threads_disabled) << "SyncRelocations";
    for (auto &Out : ThisModule->getScript().sectionMap())
      SyncBranchIslandsForOutputSection(Out);
    // sync linker created internal relocations
    for (auto *R : getTargetBackend().getInternalRelocs()) {
      writeRelocationResult(*R, Buffer);
    }
    for (auto &Input : ThisModule->getObjectList()) {
      syncRelocationResult(Buffer, Input);
    }
  } else {
    llvm::ThreadPoolInterface *Pool = ThisModule->getThreadPool();
    if (ThisModule->getPrinter()->traceThreads())
      ThisConfig.raise(Diag::threads_enabled)
          << "SyncRelocations" << ThisConfig.options().numThreads();
    // QTOOL-112094: When R_AARCH_COPY_INSN is used, it may point to the same
    // target as other input relocations. Since R_AARCH_COPY_INSN is created for
    // a branch island, it may be written from a different thread than the input
    // relocation. This creates a race condition, which may lead to the result
    // of the input relocation to be overwritten by R_AARCH_COPY_INSN.
    // Therefore, a barrier is needed between writing branch island
    // relocations and input relocations.
    for (auto &Out : ThisModule->getScript().sectionMap()) {
      Pool->async([&] { SyncBranchIslandsForOutputSection(Out); });
    }
    Pool->wait();
    // sync linker created internal relocations
    for (auto &R : getTargetBackend().getInternalRelocs()) {
      Pool->async([this, &R, &Buffer] { writeRelocationResult(*R, Buffer); });
    }
    Pool->wait();
    for (auto &Input : ThisModule->getObjectList()) {
      Pool->async(
          [this, &Buffer, Input] { syncRelocationResult(Buffer, Input); });
    }
    Pool->wait();
  }
}

void ObjectLinker::syncRelocationResult(uint8_t *Data, InputFile *Input) {
  ObjectFile *ObjFile = llvm::dyn_cast<ObjectFile>(Input);
  if (!ObjFile)
    return;
  for (auto &Sect : ObjFile->getSections()) {
    if (Sect->isBitcode())
      continue;
    ELFSection *Section = llvm::dyn_cast<eld::ELFSection>(Sect);
    if (!Section->hasRelocData())
      continue;
    if (Section->getKind() != LDFileFormat::Internal)
      continue;
    // Skip internal relocation sections.
    if (Section->isRelocationSection())
      continue;
    for (auto &Relocation : Section->getRelocations()) {
      writeRelocationResult(*Relocation, Data);
    }
  }
  ELFFileBase *EObjFile = llvm::dyn_cast<ELFFileBase>(ObjFile);
  if (!EObjFile)
    return;
  for (auto &Rs : EObjFile->getRelocationSections()) {
    if (Rs->isIgnore())
      continue;
    if (Rs->isDiscard())
      continue;

    for (auto &Relocation : Rs->getLink()->getRelocations()) {

      auto SyncOneReloc = [&](class Relocation *Relocation) -> bool {
        // bypass the reloc if the symbol is in the discarded input section
        ResolveInfo *Info = Relocation->symInfo();

        if (!Info->outSymbol()->hasFragRef() &&
            ResolveInfo::Section == Info->type() &&
            ResolveInfo::Undefined == Info->desc())
          return false;

        if (Relocation->targetRef()->frag()->getOwningSection()->isIgnore())
          return false;

        if (Relocation->targetRef()->frag()->getOwningSection()->isDiscard())
          return false;

        // bypass the relocation with NONE type. This is to avoid overwrite
        // the target result by NONE type relocation if there is a place which
        // has two relocations to apply to, and one of it is NONE type. The
        // result we want is the value of the other relocation result. For
        // example, in .exidx, there are usually an R_ARM_NONE and
        // R_ARM_PREL31 apply to the same place
        if (0x0 == Relocation->type())
          return false;

        return true;
      };
      uint64_t ModifiedRelocData = 0;
      if (ThisModule->getRelocationDataForSync(Relocation, ModifiedRelocData))
        writeRelocationData(*Relocation, ModifiedRelocData, Data);
      else if (SyncOneReloc(Relocation))
        writeRelocationResult(*Relocation, Data);
    } // for all relocations
  } // for all relocation section
}

void ObjectLinker::writeRelocationResult(Relocation &PReloc, uint8_t *POutput) {
  // Certain relocations such as R_RISCV_RELAX and R_RISCV_ALIGN do not
  // really apply. They are way to communicate linker to do certain
  // optimizations. Applying them may overrite the immediate bits based on
  // sequence in which they appear w.r.t. "real" relocation
  if (getTargetBackend().shouldIgnoreRelocSync(&PReloc))
    return;

  writeRelocationData(PReloc, PReloc.target(), POutput);
}

void ObjectLinker::writeRelocationData(Relocation &PReloc, uint64_t Data,
                                       uint8_t *POutput) {

  if (!PReloc.targetRef()->getOutputELFSection()->hasOffset())
    return;

  FragmentRef::Offset Off = PReloc.targetRef()->getOutputOffset(*ThisModule);
  if (Off == (FragmentRef::Offset)-1)
    return;

  // get output file offset
  size_t OutOffset = PReloc.targetRef()->getOutputELFSection()->offset() + Off;

  uint8_t *TargetAddr = POutput + OutOffset;

  Off = PReloc.targetRef()->getOutputOffset(*ThisModule);

  std::memcpy(TargetAddr, &Data,
              PReloc.size(*getTargetBackend().getRelocator()) / 8);
}

static void ltoDiagnosticHandler(const llvm::DiagnosticInfo &DI) {
  ASSERT(SDiagEngineForLto, "sDiagEngineForLTO is not set!!");
  std::string ErrStorage;
  {
    llvm::raw_string_ostream OS(ErrStorage);
    llvm::DiagnosticPrinterRawOStream DP(OS);
    DI.print(DP);
  }
  switch (DI.getSeverity()) {
  case DS_Error: {
    SDiagEngineForLto->raise(Diag::lto_codegen_error) << ErrStorage;
    break;
  }
  case DS_Warning:
    SDiagEngineForLto->raise(Diag::lto_codegen_warning) << ErrStorage;
    break;
  case DS_Note:
    SDiagEngineForLto->raise(Diag::lto_codegen_note) << ErrStorage;
    break;
  case DS_Remark:
    switch (DI.getKind()) {
    case llvm::DK_OptimizationRemark: {
      auto OR = cast<OptimizationRemark>(DI);
      ErrStorage += std::string(" [-Rpass=") + std::string(OR.getPassName()) +
                    std::string("]");
      break;
    }
    case llvm::DK_OptimizationRemarkMissed: {
      auto ORM = cast<OptimizationRemarkMissed>(DI);
      ErrStorage += std::string(" [-Rpass-missed=") +
                    std::string(ORM.getPassName()) + std::string("]");
      break;
    }
    case llvm::DK_OptimizationRemarkAnalysis: {
      auto ORA = cast<OptimizationRemarkAnalysis>(DI);
      ErrStorage += std::string(" [-Rpass-analysis=") +
                    std::string(ORA.getPassName()) + std::string("]");
      break;
    }
    case llvm::DK_OptimizationRemarkAnalysisFPCommute: {
      auto ORAFP = cast<OptimizationRemarkAnalysisFPCommute>(DI);
      ErrStorage += std::string(" [-Rpass-analysis=") +
                    std::string(ORAFP.getPassName()) + std::string("]");
      break;
    }
    case llvm::DK_OptimizationRemarkAnalysisAliasing: {
      auto ORAA = cast<OptimizationRemarkAnalysisAliasing>(DI);
      ErrStorage += std::string(" [-Rpass-analysis=") +
                    std::string(ORAA.getPassName()) + std::string("]");
      break;
    }
    case llvm::DK_MachineOptimizationRemark: {
      auto MOR = cast<MachineOptimizationRemark>(DI);
      ErrStorage += std::string(" [-Rpass=") + std::string(MOR.getPassName()) +
                    std::string("]");
      break;
    }
    case llvm::DK_MachineOptimizationRemarkMissed: {
      auto MORM = cast<MachineOptimizationRemarkMissed>(DI);
      ErrStorage += std::string(" [-Rpass-missed=") +
                    std::string(MORM.getPassName()) + std::string("]");
      break;
    }
    case llvm::DK_MachineOptimizationRemarkAnalysis: {
      auto MORA = cast<MachineOptimizationRemarkAnalysis>(DI);
      ErrStorage += std::string(" [-Rpass-analysis=") +
                    std::string(MORA.getPassName()) + std::string("]");
      break;
    }
    }
    SDiagEngineForLto->raise(Diag::lto_codegen_remark) << ErrStorage;
    break;
  }
}

bool ObjectLinker::runAssembler(
    std::vector<std::string> &Files, std::string RelocModel,
    const std::vector<std::string> &AsmFilesFromLto) {
  std::vector<std::string> FileList;

  if (ThisConfig.options().hasLTOAsmFile()) {
    for (auto &F : ThisConfig.options().ltoAsmFile()) {
      ThisConfig.raise(Diag::using_lto_asm_file) << F;
      FileList.push_back(F);
    }
  } else {
    for (auto &F : AsmFilesFromLto) {
      if (!F.empty())
        FileList.push_back(F);
    }
  }

  if (ThisConfig.options().hasLTOOutputFile()) {
    for (auto &F : ThisConfig.options().ltoOutputFile()) {
      ThisConfig.raise(Diag::using_lto_output_file) << F;
      Files.push_back(F);
    }
    return true;
  }

  uint32_t Count = 0;
  for (auto F : FileList) {
    SmallString<256> OutputFileName;
    auto OS = createLTOTempFile(Count, "o", OutputFileName);
    if (!OS)
      return false;
    Files.push_back(std::string(OutputFileName));

    if (!getTargetBackend().ltoCallExternalAssembler(
            F, RelocModel, std::string(OutputFileName)))
      return false;

    ++Count;
  }
  return true;
}

std::unique_ptr<llvm::lto::LTO> ObjectLinker::ltoInit(llvm::lto::Config Conf) {
  // Parse codegen options and pre-initialize the config
  eld::RegisterTimer T("Initialize LTO", "LTO",
                       ThisConfig.options().printTimingStats());
  if (MTraceLTO) {
    if (ThisConfig.options().codegenOpts()) {
      std::stringstream Ss;
      for (auto Ai : ThisConfig.options().codeGenOpts())
        Ss << Ai << " ";
      ThisConfig.raise(Diag::codegen_options)
          << ThisConfig.targets().triple().str() << Ss.str();
    } else {
      ThisConfig.raise(Diag::codegen_options)
          << ThisConfig.targets().triple().str() << "none";
    }
  }

  Conf.DiagHandler = ltoDiagnosticHandler;
  if (ThisConfig.options().hasLTOOptRemarksFile()) {
    std::string OptYamlFileName =
        ThisConfig.options().outputFileName() + std::string("-LTO.opt.yaml");
    Conf.RemarksFilename = OptYamlFileName;
    if (ThisConfig.options().hasLTOOptRemarksDisplayHotness())
      Conf.RemarksWithHotness = true;
  }
  std::string Cpu = getTargetBackend().getInfo().getOutputMCPU().str();
  if (!Cpu.empty()) {
    Conf.CPU = Cpu;
    if (MTraceLTO)
      ThisConfig.raise(Diag::set_codegen_mcpu) << Cpu;
  }

  Conf.DefaultTriple = ThisConfig.targets().triple().str();

  if (getTargetBackend().ltoNeedAssembler())
    Conf.CGFileType = llvm::CodeGenFileType::AssemblyFile;

  auto Model = ltoCodegenModel();

  Conf.RelocModel = Model.first;

  // If save-temps is enabled, get LTO to write out the module
  // TODO: This will write out *lots* of files at different stages. We may
  // want to curtail this to just two as before (for RegularLTO) and perhaps
  // more for ThinLTO.
  if (MTraceLTO || MSaveTemps) {
    if (llvm::Error E = Conf.addSaveTemps(MLtoTempPrefix)) {
      ThisConfig.raise(Diag::lto_cannot_save_temps)
          << llvm::toString(std::move(E));
      return {};
    }
    // FIXME: Output actual file names (this will go with the above TODO).
    ThisConfig.raise(Diag::note_temp_lto_bitcode) << MLtoTempPrefix + "*";
  }

  // Set the number of backend threads to use in ThinLTO
  unsigned NumThreads = 1;
  if (ThisConfig.options().threadsEnabled()) {
    NumThreads = ThisConfig.options().numThreads();
    if (!NumThreads)
      NumThreads = std::thread::hardware_concurrency();
    if (!NumThreads)
      NumThreads = 4; // if hardware_concurrency returns 0
    ThisConfig.raise(Diag::note_lto_threads) << NumThreads;
  }

  // Initialize the LTO backend
  llvm::lto::ThinBackend Backend = llvm::lto::createInProcessThinBackend(
      llvm::heavyweight_hardware_concurrency(NumThreads));
  return std::make_unique<llvm::lto::LTO>(std::move(Conf), std::move(Backend));
}

bool ObjectLinker::finalizeLtoSymbolResolution(
    llvm::lto::LTO &LTO, const std::vector<BitcodeFile *> &BitCodeInputs) {

  eld::RegisterTimer T("Finalize Symbol Resolution", "LTO",
                       ThisConfig.options().printTimingStats());
  bool IsPreserveAllSet = ThisConfig.options().preserveAllLTO();
  bool IsPreserveGlobals = ThisConfig.options().exportDynamic();

  bool TraceWrap = ThisModule->getPrinter()->traceWrapSymbols();

  std::set<std::string> SymbolsToPreserve;
  std::set<ResolveInfo *> PreserveSyms;

  for (auto &L : ThisModule->getNamePool().getLocals()) {
    if (L->shouldPreserve() || (IsPreserveAllSet && L->isBitCode()))
      PreserveSyms.insert(L);
  }

  // Traverse all the resolveInfo and add the output symbol to output
  for (auto &G : ThisModule->getNamePool().getGlobals()) {
    ResolveInfo *Info = G.getValue();
    // preserve all defined global non-hidden symbol in bitcode when building
    // shared library.
    if (ThisConfig.options().hasShared()) {
      // Symbols with reduced scope in version script must be skipped
      auto SymbolScopes = getTargetBackend().symbolScopes();
      const auto &Found = SymbolScopes.find(Info);
      bool DoNotPreserve = Found != SymbolScopes.end() &&
                           Found->second->isLocal() && !Info->isUndef();

      if (Info->isBitCode() &&
          (Info->desc() == ResolveInfo::Define ||
           Info->desc() == ResolveInfo::Common) &&
          Info->binding() != ResolveInfo::Local &&
          (Info->visibility() == ResolveInfo::Default ||
           Info->visibility() == ResolveInfo::Protected) &&
          !DoNotPreserve) {
        PreserveSyms.insert(Info);
      }
    }
    // preserve all defined global symbol in bitcode when building
    // relocatable.
    if (ThisConfig.codeGenType() == LinkerConfig::Object) {
      if (Info->isBitCode() &&
          (Info->desc() == ResolveInfo::Define ||
           Info->desc() == ResolveInfo::Common) &&
          Info->binding() != ResolveInfo::Local)
        Info->shouldPreserve(true);
    }
    if (Info->shouldPreserve() ||
        ((IsPreserveAllSet || IsPreserveGlobals) && Info->isBitCode())) {
      if (Info->outSymbol() &&
          !(MGcHasRun && Info->outSymbol()->shouldIgnore()))
        PreserveSyms.insert(Info);
      else if (MTraceLTO)
        ThisConfig.raise(Diag::note_not_preserving_symbol) << Info->name();
    }
    if (Info->isBitCode() && Info->isCommon() &&
        ThisModule->getScript().linkerScriptHasSectionsCommand()) {
      // never internalize common symbols.
      ThisModule->recordCommon(Info->name(), Info->resolvedOrigin());
      if (MTraceLTO)
        ThisConfig.raise(Diag::note_preserving_common)
            << Info->name() << "COMMON"
            << Info->resolvedOrigin()->getInput()->decoratedPath();
      PreserveSyms.insert(Info);
    }
    if (!(MGcHasRun && Info->outSymbol()->shouldIgnore())) {
      std::vector<ScriptSymbol *> &DynListSyms = ThisModule->dynListSyms();
      for (auto &Pattern : DynListSyms) {
        if (Pattern->matched(*Info)) {
          PreserveSyms.insert(Info);
          if (MTraceLTO)
            ThisConfig.raise(Diag::preserve_dyn_list_sym) << Info->name();
        }
      }
    }
    // Wrapped functions using --wrap need to be preserved as well
    if (Info->isBitCode() && !ThisConfig.options().renameMap().empty()) {
      llvm::StringMap<std::string>::iterator RenameSym =
          ThisConfig.options().renameMap().find(Info->name());
      if (ThisConfig.options().renameMap().end() != RenameSym) {
        if (MTraceLTO || TraceWrap)
          ThisConfig.raise(Diag::preserve_wrap) << Info->name();
        PreserveSyms.insert(Info);
        ResolveInfo *Wrapper =
            ThisModule->getNamePool().findInfo(RenameSym->second);
        if (Wrapper && Wrapper->isBitCode()) {
          PreserveSyms.insert(Wrapper);
          if (MTraceLTO || TraceWrap)
            ThisConfig.raise(Diag::preserve_wrap) << Wrapper->name();
        }
      }
    }
  }

  // Symbols that are preserved from Bitcode files are set to be used.
  for (auto &S : PreserveSyms) {
    InputFile *ResolvedOrigin = S->resolvedOrigin();
    if (ResolvedOrigin)
      ResolvedOrigin->setUsed(true);
    SymbolsToPreserve.insert(S->name());
  }
  if (ThisConfig.options().preserveSymbolsLTO()) {
    for (auto Sym : ThisConfig.options().getPreserveList())
      SymbolsToPreserve.insert(Sym);
  }
  /// Prevailing = False
  /// The linker has not chosen the definition, and compiler can be free to
  /// discard this symbol.

  /// FinalDefinitionInLinkageUnit = True
  /// The definition of this symbol is unpreemptable at runtime and is known
  /// to be in this linkage unit.

  /// VisibleToRegularObj = True
  /// The definition of this symbol is visible outside of the LTO unit.

  /// LinkerRedefined = True
  /// Linker redefined version of the symbol which appeared in -wrap or
  /// -defsym linker option.

  bool HasSectionsCmd =
      ThisModule->getScript().linkerScriptHasSectionsCommand();

  // Add the input files
  for (BitcodeFile *Inp : BitCodeInputs) {

    // Compute the LTO resolutions
    std::vector<llvm::lto::SymbolResolution> LTOResolutions;
    for (const auto &Sym : Inp->getInputFile().symbols()) {

      // Only needed: name, isCommon, isUndefined

      llvm::lto::SymbolResolution LTORes;

      ResolveInfo *Info =
          ThisModule->getNamePool().findInfo(Sym.getName().str());

      if (!Info) {
        llvm_unreachable("Global LTO symbol not in namepool");
      } else if (!Sym.isUndefined()) {

        // If this definition is chosen, set the prevailing property.
        if (Info->resolvedOrigin() == Inp)
          LTORes.Prevailing = true;
        // If a symbol needs to be preserved, because its being referenced
        // from regular object files, set the VisibletoRegularObj property
        // appropriately.
        if (SymbolsToPreserve.count(Sym.getName().str())) {
          if (MTraceLTO)
            ThisConfig.raise(Diag::note_preserve_symbol) << Sym.getName();
          LTORes.VisibleToRegularObj = true;
        }

        // Resolution of bitcode wrappers happen in post LTO phase in ELF.
        // If wrappers are defined in Bitcode and only referred in ELF
        // we have an unpreserved wrapper definition that needs to be
        // preserved. This definition is renamed when native object is
        // resolved.
        llvm::StringRef WrapSymbolName = Sym.getName();
        if (WrapSymbolName.starts_with("__wrap_") ||
            WrapSymbolName.starts_with("__real_"))
          WrapSymbolName = WrapSymbolName.drop_front(7);
        llvm::StringMap<std::string>::iterator RenameSym =
            ThisConfig.options().renameMap().find(WrapSymbolName);
        if (ThisConfig.options().renameMap().end() != RenameSym) {
          LTORes.Prevailing = true;
          LTORes.VisibleToRegularObj = true;
          if (MTraceLTO || TraceWrap)
            ThisConfig.raise(Diag::preserve_wrapper_def) << Sym.getName();
          // If the symbol is part of --wrap mechanism, mark it as linker
          // renamed
          // and save its binding. Post LTO phase will restore the binding to
          // this
          // original value.
          if (Info->isDefine()) {
            LTORes.LinkerRedefined = true;
            ThisModule->saveWrapSymBinding(Info->name(), Info->binding());
          }
        }

        // Preserve all commons if there is a linker script. This is
        // not ideal, but if we do not preserve a common global, it will be
        // internalized. In LTO with linker scripts, we will then have no
        // section information (apart from the pseudo-section COMMON) for that
        // variable. We could notify the user if a common moves to a regular
        // section due to internalization, but this might require changes to
        // their linker script.
        if (HasSectionsCmd && Sym.isCommon())
          LTORes.VisibleToRegularObj = true;

        // This copies the behavior of the gold plugin.
        if (!Info->isDyn() && !Info->isUndef() &&
            (ThisConfig.codeGenType() == LinkerConfig::Exec ||
             ThisConfig.options().isPIE() ||
             Info->visibility() != ResolveInfo::Default))
          LTORes.FinalDefinitionInLinkageUnit = true;
      }

      if (MTraceLTO)
        ThisConfig.raise(Diag::lto_resolution)
            << Sym.getName() << LTORes.Prevailing << LTORes.VisibleToRegularObj
            << LTORes.FinalDefinitionInLinkageUnit
            << (Info ? Info->resolvedOrigin()->getInput()->decoratedPath()
                     : "N/A")
            << LTORes.LinkerRedefined;

      LTOResolutions.push_back(LTORes);
    }

    if (MTraceLTO)
      ThisConfig.raise(Diag::adding_module) << Inp->getInput()->decoratedPath();

    if (llvm::Error E = LTO.add(Inp->takeLTOInputFile(), LTOResolutions)) {
      ThisConfig.raise(Diag::fatal_lto_merge_error)
          << llvm::toString(std::move(E));
      ThisModule->setFailure(true);
      return false;
    }
  }

  return true;
}

void ObjectLinker::addTempFilesToTar(size_t MaxTasks) {
  if (!MSaveTemps)
    return;
  std::vector<std::string> Suffix = {".0.preopt.bc",      ".1.promote.bc",
                                     ".2.internalize.bc", ".3.import.bc",
                                     ".4.opt.bc",         ".5.precodegen.bc"};
  auto Prefix = ThisConfig.options().outputFileName();
  Prefix += ".llvm-lto.";
  // not a bitcode file but should probably be placed in Bitcode category
  addInputFileToTar(Prefix + "resolution.txt", eld::MappingFile::Kind::Bitcode);
  for (size_t Task = 0; Task <= MaxTasks; ++Task)
    for (auto &S : Suffix)
      addInputFileToTar(Prefix + llvm::utostr(Task) + S,
                        eld::MappingFile::Kind::Bitcode);
}

void ObjectLinker::addLTOOutputToTar() {
  if (!ThisModule->getOutputTarWriter())
    return;
  for (auto &Ipt : LTOELFFiles) {
    eld::InputFile *IptFile = Ipt->getInputFile();
    IptFile->setMappedPath(Ipt->getName());
    IptFile->setMappingFileKind(eld::MappingFile::Kind::ObjectFile);
    ThisModule->getOutputTarWriter()->addInputFile(IptFile,
                                                   /*isLTO*/ true);
  }
}

bool ObjectLinker::doLto(llvm::lto::LTO &LTO) {
  eld::RegisterTimer T("Invoke LTO", "LTO",
                       ThisConfig.options().printTimingStats());
  // Run LTO
  std::vector<std::string> Files;
  if (!ThisConfig.options().hasLTOOutputFile())
    Files.resize(LTO.getMaxTasks());
  FilesToRemove.resize(LTO.getMaxTasks());

  auto AddStream = [&](size_t Task, const Twine &ModuleName)
      -> llvm::Expected<std::unique_ptr<llvm::CachedFileStream>> {
    SmallString<256> ObjFileName;
    auto OS = createLTOTempFile(
        Task, getTargetBackend().ltoNeedAssembler() ? "s" : "o", ObjFileName);

    if (!OS) {
      ThisModule->setFailure(true);
      return OS.takeError();
    }

    assert(Files[Task].empty() && "LTO task already produced an output!");
    Files[Task] = std::string(ObjFileName);
    if (!MSaveTemps)
      FilesToRemove[Task] = std::string(ObjFileName);
    return std::make_unique<llvm::CachedFileStream>(std::move(*OS));
  };

  // Callback to add a file from the cache
  auto AddBuffer = [&](size_t Task, const Twine &ModuleName,
                       std::unique_ptr<MemoryBuffer> MB) {
    auto S = AddStream(Task, ModuleName);
    if (!S)
      report_fatal_error(Twine("unable to add memory buffer: ") +
                         toString(S.takeError()));
    *(*S)->OS << MB->getBuffer();
    if (Error Err = (*S)->commit())
      report_fatal_error(std::move(Err));
  };

  llvm::FileCache ThinLTOCache;
  std::string CacheDirectory;
  if (ThisConfig.options().isLTOCacheEnabled()) {
    if (ThisConfig.options().getLTOCacheDirectory().empty())
      CacheDirectory =
          (llvm::Twine(ThisConfig.options().outputFileName() + ".ltocache"))
              .str();
    else
      CacheDirectory = ThisConfig.options().getLTOCacheDirectory().str();

    llvm::Expected<llvm::FileCache> LC =
        llvm::localCache("ThinLTO", "Thin", CacheDirectory, AddBuffer);

    if (!LC) {
      ThisConfig.raise(Diag::fatal_lto_cache_error)
          << CacheDirectory << llvm::toString(LC.takeError());
      ThisModule->setFailure(true);
      return false;
    }
    if (MTraceLTO)
      ThisConfig.raise(Diag::note_lto_cache) << CacheDirectory;

    ThinLTOCache = *LC;
  }

  if (getTargetBackend().ltoNeedAssembler()) {
    // If the output files haven't already been provided, run compilation now
    std::vector<std::string> LTOAsmOutput;
    if (!ThisConfig.options().hasLTOOutputFile() &&
        !ThisConfig.options().hasLTOAsmFile()) {
      if (llvm::Error E = LTO.run(AddStream, ThinLTOCache)) {
        ThisConfig.raise(Diag::fatal_no_codegen_compile)
            << llvm::toString(std::move(E));
        ThisModule->setFailure(true);
        return false;
      }
      // AddStream adds the LTO output files to 'files', but in this case
      // they're just the input files to the assembler so we need to move them
      // to their own array. ltoRunAssembler will add the assembled objects to
      // files.
      LTOAsmOutput = std::move(Files);
    }
    if (!runAssembler(Files, ltoCodegenModel().second, LTOAsmOutput)) {
      ThisConfig.raise(Diag::lto_codegen_error) << "Assembler error occurred";
      ThisModule->setFailure(true);
      return false;
    }

    if (!MSaveTemps && !ThisConfig.options().hasLTOOutputFile())
      FilesToRemove.insert(FilesToRemove.end(), Files.begin(), Files.end());
  } else {
    // If the output files haven't already been provided, run compilation now
    if (!ThisConfig.options().hasLTOOutputFile()) {
      if (llvm::Error E = LTO.run(AddStream, ThinLTOCache)) {
        ThisConfig.raise(Diag::fatal_no_codegen_compile)
            << llvm::toString(std::move(E));
        ThisModule->setFailure(true);
        return false;
      }
    } else {
      for (auto &F : ThisConfig.options().ltoOutputFile()) {
        ThisConfig.raise(Diag::using_lto_output_file) << F;
        Files.push_back(F);
      }
    }
  }
  if (!ThisConfig.getDiagEngine()->diagnose()) {
    if (ThisModule->getPrinter()->isVerbose())
      ThisConfig.raise(Diag::function_has_error) << __PRETTY_FUNCTION__;
    return false;
  }

  // Add the generated object files into the InputBuilder structure
  for (auto &F : Files) {
    if (F.empty())
      continue;
    LtoObjects.push_back(F);
    if (MSaveTemps)
      ThisConfig.raise(Diag::note_temp_lto_object) << F;
  }
  addTempFilesToTar(LTO.getMaxTasks());
  return true;
}

namespace {

void processCodegenOptions(
    llvm::iterator_range<std::vector<std::string>::const_iterator> CmdOpts,
    llvm::lto::Config &Conf, std::vector<std::string> &UserArgs) {

  for (const std::string &Arg : CmdOpts) {
    // Options coming from the linker may not be tokenized, so we need to
    // split them into individual options.
    for (std::pair<StringRef, StringRef> O = getToken(Arg); !O.first.empty();
         O = getToken(O.second)) {

      if (O.first.starts_with("-O")) {
        auto ParsedCGOptLevel =
            llvm::StringSwitch<std::optional<CodeGenOptLevel>>(
                O.first.substr(2))
                .Case("s", CodeGenOptLevel::Default)
                .Case("z", CodeGenOptLevel::Default)
                .Case("g", CodeGenOptLevel::Less)
                .Case("0", CodeGenOptLevel::None)
                .Case("1", CodeGenOptLevel::Less)
                .Case("2", CodeGenOptLevel::Default)
                .Case("3", CodeGenOptLevel::Aggressive)
                .Default(std::nullopt);
        if (ParsedCGOptLevel) {
          Conf.OptLevel = static_cast<unsigned>(*ParsedCGOptLevel);
          Conf.CGOptLevel = *ParsedCGOptLevel;
        } else
          llvm::errs()
              << "LTOCodeGenOptions: Ignoring invalid opt level " // TODO
              << O.first << "\n";
      } else if (O.first.starts_with("debug-pass-manager") ||
                 O.first.starts_with("-debug-pass-manager")) {
        // TODO: Clean up compiler code that uses "debug-pass-manager".
        // The compiler also passes all kinds of options in codegen=,
        // even those that are not codegen options. That's why we
        // have to special case these here.
        Conf.DebugPassManager = true;
      } else if (O.first.consume_front("-mcpu=")) {
        Conf.CPU = O.first.str();
      } else if (O.first.consume_front("-mattr=")) {
        Conf.MAttrs.push_back(O.first.str());
      } else
        UserArgs.push_back(O.first.str());
    }
  }
}

} // namespace

void ObjectLinker::setLTOPlugin(plugin::LinkerPlugin &LTOPlugin) {
  this->LTOPlugin = &LTOPlugin;
}

bool ObjectLinker::createLTOObject(void) {
  std::vector<BitcodeFile *> BitCodeInputs;
  std::vector<InputFile *> AllInputs;

  eld::Module::const_obj_iterator Input, InEnd = ThisModule->objEnd();
  for (Input = ThisModule->objBegin(); Input != InEnd; ++Input) {
    if ((*Input)->isBitcode()) {
      BitCodeInputs.push_back(llvm::dyn_cast<eld::BitcodeFile>(*Input));
    }
    // Dont assign output sections to input files that are specified
    // with just symbols
    if ((*Input)->getInput()->getAttribute().isJustSymbols())
      continue;
    AllInputs.push_back(*Input);
  }

  if (!BitCodeInputs.size())
    return true;
  {
    eld::RegisterTimer T("Assign Output sections to Bitcode sections and ELF",
                         "LTO", ThisConfig.options().printTimingStats());
    // Centralize assigning output sections.
    assignOutputSections(AllInputs);
  }

  llvm::lto::Config Conf;
  std::vector<std::string> Options;
  processCodegenOptions(ThisConfig.options().codeGenOpts(), Conf, Options);

  if (!ThisModule->getLinker()->getLinkerDriver()->processLTOOptions(Conf,
                                                                     Options))
    return false;

  getTargetBackend().AddLTOOptions(Options);

  if (LTOPlugin)
    LTOPlugin->ModifyLTOOptions(Conf, Options);

  // Config.MllvmArgs are ignored, call cl::ParseCommandLineOptions instead.
  std::vector<const char *> CodegenArgv(1, "LTOCodeGenOptions");
  for (const std::string &Arg : Options)
    CodegenArgv.push_back(Arg.c_str());
  cl::ParseCommandLineOptions(CodegenArgv.size(), CodegenArgv.data());

  Conf.Options = llvm::codegen::InitTargetOptionsFromCodeGenFlags(
      ThisConfig.targets().triple());

  if (LTOPlugin) {

    // TODO: Why do we read all relocations here? Get rid of?
    if (!readRelocations())
      return false;

    LTOPlugin->ActBeforeLTO(Conf);
  }

  PrepareDiagEngineForLTO PrepareDiagnosticsForLto(ThisConfig.getDiagEngine());

  std::unique_ptr<llvm::lto::LTO> LTO = ltoInit(std::move(Conf));
  if (!LTO)
    return false;

  if (!finalizeLtoSymbolResolution(*LTO, BitCodeInputs))
    return false;

  if (!doLto(*LTO)) {
    ThisModule->setFailure(true);
    return false;
  }
  return true;
}

void ObjectLinker::beginPostLTO() {
  TextLayoutPrinter *Printer = ThisModule->getTextMapPrinter();
  if (Printer) {
    Printer->addLayoutMessage("Pre-LTO Map records\n");
    Printer->printArchiveRecords(*ThisModule);
    Printer->printInputActions();
    Printer->clearInputRecords();
    Printer->addLayoutMessage("Post-LTO Map records");
  }
  if (ThisConfig.options().cref())
    getTargetBackend().printCref(MPostLtoPhase);
  MPostLtoPhase = true;
}

bool ObjectLinker::insertPostLTOELF() {
  if (LtoObjects.empty())
    return true;

  eld::Module::obj_iterator Obj, BitcodeObj, ObjEnd = ThisModule->objEnd();

  InputFile *BitcodeObject = nullptr;
  for (Obj = ThisModule->objBegin(); Obj != ObjEnd; ++Obj) {
    if ((*Obj)->isBitcode()) {
      if (!BitcodeObject) {
        BitcodeObject = (*Obj);
        BitcodeObj = Obj;
      }
    }
  }

  assert(BitcodeObj != ObjEnd);

  for (auto &Ltoobj : LtoObjects) {
    sys::fs::Path Path = Ltoobj;
    Input *In =
        ThisModule->getIRBuilder()->getInputBuilder().createInput(Ltoobj);
    if (!In->resolvePath(ThisConfig)) {
      ThisModule->setFailure(true);
      return false;
    }
    InputFile *I = InputFile::create(In, ThisConfig.getDiagEngine());
    if (I->getKind() == InputFile::ELFObjFileKind) {
      // FIXME: We should convert all llvm::dyn_cast which should always be
      // successful to llvm::cast.
      ELFObjectFile *ELFObj = llvm::dyn_cast<ELFObjectFile>(I);
      ELFObj->setLTOObject();
      bool ELFOverridenWithBC = false;
      eld::Expected<bool> ExpParseFile =
          getRelocObjParser()->parseFile(*I, ELFOverridenWithBC);
      ASSERT(!ELFOverridenWithBC, "Invalid ELF override to BC operation!");
      // Currently, we have to consider two cases:
      // expParseFile returns an error: In this case, we report the error and
      // return false.
      // expParseFile returns false: Reading failed but we do not
      // have error info available. In this case, we just return false.
      // There should ideally be no case when reading fails but there is no
      // error information available. All such cases must be removed.
      if (!ExpParseFile)
        ThisConfig.raiseDiagEntry(std::move(ExpParseFile.error()));
      if (!ExpParseFile.has_value() || !ExpParseFile.value()) {
        return false;
      }
      In->setInputFile(I);
    } else {
      // Issue an error if the object is not recognized.
      ThisConfig.raise(Diag::err_unrecognized_input_file)
          << In->decoratedPath() << ThisConfig.targets().triple().str();
      ThisModule->setFailure(true);
      return false;
    }
    if (MTraceLTO)
      ThisConfig.raise(Diag::note_insert_object) << In->getResolvedPath();

    if (!ThisConfig.getDiagEngine()->diagnose()) {
      if (ThisModule->getPrinter()->isVerbose())
        ThisConfig.raise(Diag::function_has_error) << __PRETTY_FUNCTION__;
      return false;
    }

    LTOELFFiles.push_back(std::move(In));
  }

  addLTOOutputToTar();

  std::vector<InputFile *> Ltoobjectfiles;
  for (auto &Elffile : LTOELFFiles)
    Ltoobjectfiles.push_back(Elffile->getInputFile());

  ThisModule->insertLTOObjects(BitcodeObj, Ltoobjectfiles);

  if (!ThisModule->getScript().linkerScriptHasSectionsCommand())
    return true;

  llvm::StringMap<InputFile *> MSectionWithOldInputMap;
  // Record the section and the map that contains the old input for a section.
  for (auto &Elffile : Ltoobjectfiles) {
    ELFObjectFile *ObjFile = llvm::dyn_cast<ELFObjectFile>(Elffile);
    for (auto &Sect : ObjFile->getSections()) {
      ELFSection *Section = llvm::dyn_cast<eld::ELFSection>(Sect);
      if (Section->hasOldInputFile())
        MSectionWithOldInputMap[Section->name()] = Section->getOldInputFile();
    }
  }
  // If a section appears with the same name, then associate the same input
  // for the section that contains the old symbol too.
  for (auto &Elffile : Ltoobjectfiles) {
    ELFObjectFile *ObjFile = llvm::dyn_cast<ELFObjectFile>(Elffile);
    for (auto &Sect : ObjFile->getSections()) {
      ELFSection *Section = llvm::dyn_cast<eld::ELFSection>(Sect);
      // If the section already has an old input associated with it, move on.
      if (Section->getOldInputFile())
        continue;
      // Very important to note, that sections that are not tracked and that
      // sections that have the same name will be assigned the same input.
      auto OldInput = MSectionWithOldInputMap.find(Section->name());
      if (OldInput == MSectionWithOldInputMap.end())
        continue;
      Section->setOldInputFile(OldInput->second);
    }
  }
  return true;
}

// Get the codegen model for LTO.
std::pair<std::optional<llvm::Reloc::Model>, std::string>
ObjectLinker::ltoCodegenModel() const {
  // Only
  // Non static builds, Shared library builds or PIE builds will set the LTO
  // Codegen model to be Dynamic.
  if ((!ThisConfig.isCodeStatic() &&
       ThisConfig.codeGenType() != LinkerConfig::Exec) ||
      (LinkerConfig::DynObj == ThisConfig.codeGenType()) ||
      ThisConfig.options().isPIE()) {
    if (MTraceLTO)
      ThisConfig.raise(Diag::lto_code_model) << "Dynamic";
    return std::make_pair(llvm::Reloc::PIC_, "pic");
  }
  // -frwpi, -fropi
  if (ThisConfig.options().hasRWPI() && ThisConfig.options().hasROPI()) {
    if (MTraceLTO)
      ThisConfig.raise(Diag::lto_code_model) << "ROPI, RWPI";
    return std::make_pair(llvm::Reloc::ROPI_RWPI, "ropi-rwpi");
  }
  // -frwpi
  if (ThisConfig.options().hasRWPI()) {
    if (MTraceLTO)
      ThisConfig.raise(Diag::lto_code_model) << "RWPI";
    return std::make_pair(llvm::Reloc::RWPI, "rwpi");
  }
  // --fropi
  if (ThisConfig.options().hasROPI()) {
    if (MTraceLTO)
      ThisConfig.raise(Diag::lto_code_model) << "ROPI";
    return std::make_pair(llvm::Reloc::ROPI, "ropi");
  }
  // If the relocation model is static (the default),
  // we'll let LLVM decide which RelocModel to use based on the "PIC Level"
  // module flag which is set if -fPIC was specified during bitcode
  // compilation. This allows partial linking of -fPIC code with LTO.
  if (MTraceLTO)
    ThisConfig.raise(Diag::lto_code_model) << "Auto Detect (Default: Static)";
  return std::make_pair(std::nullopt, "static");
}

bool ObjectLinker::runSectionIteratorPlugin() {
  ObjectBuilder Builder(ThisConfig, *ThisModule);
  std::vector<InputFile *> Inputs;
  std::vector<ELFSection *> EntrySections;
  getInputs(Inputs);
  Builder.initializePluginsAndProcess(Inputs,
                                      plugin::Plugin::Type::SectionIterator);

  Builder.reAssignOutputSections(/*LW=*/nullptr);
  return true;
}

void ObjectLinker::addInputFileToTar(InputFile *Ipt, MappingFile::Kind K) {
  auto *OutputTar = ThisModule->getOutputTarWriter();
  if (!OutputTar || !Ipt)
    return;
  if (Ipt->isInternal())
    return;
  if (Ipt->getInput()->isArchiveMember())
    return;
  Input *I = Ipt->getInput();
  bool UseDecorated =
      !I->isNamespec() &&
      Ipt->getKind() == InputFile::InputFileKind::ELFDynObjFileKind;
  Ipt->setMappedPath(UseDecorated ? I->decoratedPath() : I->getName());
  Ipt->setMappingFileKind(K);
  OutputTar->addInputFile(Ipt, /*isLTO*/ false);
}

bool ObjectLinker::readAndProcessInput(Input *Input, bool IsPostLto) {
  if (Input->isInternal())
    return true;
  if (!Input->getSize())
    ThisConfig.raise(Diag::input_file_has_zero_size) << Input->decoratedPath();
  LayoutInfo *layoutInfo = ThisModule->getLayoutInfo();
  std::string Path = Input->getResolvedPath().native();

  InputFile *CurInput = Input->getInputFile();
  if (!CurInput) {
    InputFile *I = InputFile::create(Input, ThisConfig.getDiagEngine());
    if (!I) {
      ThisConfig.raise(Diag::err_unrecognized_input_file)
          << Input->getResolvedPath() << ThisConfig.targets().triple().str();
      ThisModule->setFailure(true);
      return false;
    }
    CurInput = I;
    Input->setInputFile(I);
  }
  if (CurInput->shouldSkipFile()) {
    if (layoutInfo) {
      layoutInfo->recordInputActions(LayoutInfo::Skipped, Input);
    }
    return true;
  }
  if (Input->getAttribute().isPatchBase() &&
      CurInput->getKind() != InputFile::ELFExecutableFileKind) {
    ThisConfig.raise(Diag::err_patch_base_not_executable)
        << Input->getResolvedPath();
    ThisModule->setFailure(true);
    return false;
  }

  if (CurInput->isBinaryFile()) {
    eld::RegisterTimer T("Read ELF Executable Files", "Read all Input files",
                         ThisConfig.options().printTimingStats());
    if (layoutInfo)
      layoutInfo->recordInputKind(InputFile::InputFileKind::BinaryFileKind);
    eld::Expected<void> ExpParseFile =
        getBinaryFileParser()->parseFile(*CurInput);
    if (!ExpParseFile) {
      ThisConfig.raiseDiagEntry(std::move(ExpParseFile.error()));
      return false;
    }
    ThisModule->getObjectList().push_back(CurInput);
    addInputFileToTar(CurInput, eld::MappingFile::Kind::ObjectFile);
  } else if (CurInput->getKind() == InputFile::ELFExecutableFileKind) {
    eld::RegisterTimer T("Read ELF Executable Files", "Read all Input files",
                         ThisConfig.options().printTimingStats());
    if (layoutInfo)
      layoutInfo->recordInputKind(CurInput->getKind());
    bool ELFOverriddenWithBC = false;
    if (!isBackendInitialized()) {
      // Infer machine for selecting backend
      eld::Expected<uint16_t> Machine =
          getELFExecObjParser()->getMachine(*CurInput);
      if (!Machine) {
        ThisConfig.raiseDiagEntry(std::move(Machine.error()));
        return false;
      }
      llvm::cast<ObjectFile>(CurInput)->setMachine(*Machine);
      if (!initializeTarget(CurInput))
        return false;
    }
    eld::Expected<bool> ExpParseFile =
        getELFExecObjParser()->parseFile(*CurInput, ELFOverriddenWithBC);
    if (!ExpParseFile)
      ThisConfig.raiseDiagEntry(std::move(ExpParseFile.error()));
    if (!ExpParseFile.has_value() || !ExpParseFile.value()) {
      ThisModule->setFailure(true);
      return false;
    }
    if (!IsPostLto && overrideELFObjectWithBitCode(CurInput)) {
      return readAndProcessInput(Input, IsPostLto);
    }
    ThisModule->getObjectList().push_back(CurInput);
    addInputFileToTar(CurInput, eld::MappingFile::Kind::ObjectFile);
  } else if (CurInput->getKind() == InputFile::ELFObjFileKind) {
    eld::RegisterTimer T("Read ELF Object Files", "Read all Input files",
                         ThisConfig.options().printTimingStats());
    if (layoutInfo)
      layoutInfo->recordInputKind(CurInput->getKind());
    bool ELFOverridenWithBC = false;

    if (!isBackendInitialized()) {
      // Infer machine for selecting backend
      eld::Expected<uint16_t> Machine =
          getRelocObjParser()->getMachine(*CurInput);
      if (!Machine) {
        ThisConfig.raiseDiagEntry(std::move(Machine.error()));
        return false;
      }
      llvm::cast<ObjectFile>(CurInput)->setMachine(*Machine);
      if (!initializeTarget(CurInput))
        return false;
    }

    eld::Expected<bool> ExpParseFile =
        getRelocObjParser()->parseFile(*CurInput, ELFOverridenWithBC);
    // Currently, we have to consider two cases:
    // expParseFile returns an error: In this case, we report the error and
    // return false.
    // expParseFile returns false: Reading failed but we do not
    // have error info available. In this case, we just return false.
    // There should ideally be no case when reading fails but there is no
    // error information available. All such cases must be removed.
    if (!ExpParseFile)
      ThisConfig.raiseDiagEntry(std::move(ExpParseFile.error()));
    if (!ExpParseFile.has_value() || !ExpParseFile.value()) {
      ThisModule->setFailure(true);
      return false;
    }
    if (ELFOverridenWithBC) {
      return readAndProcessInput(Input, IsPostLto);
    }
    CurInput->setToSkip();
    ThisModule->getObjectList().push_back(CurInput);
    addInputFileToTar(CurInput, eld::MappingFile::Kind::ObjectFile);
  } else if (CurInput->getKind() == InputFile::BitcodeFileKind) {
    eld::RegisterTimer T("Read Bitcode Object Files", "Read all Input files",
                         ThisConfig.options().printTimingStats());
    if (layoutInfo)
      layoutInfo->recordInputKind(CurInput->getKind());
    if (IsPostLto)
      return true;
    CurInput->setToSkip();
    // LTO is needed.
    ThisModule->setLTONeeded();
    if (!getBitcodeReader()->readInput(*CurInput, LTOPlugin))
      return false;
    if (!isBackendInitialized()) {
      if (!initializeTarget(CurInput))
        return false;
    }

    ThisModule->getObjectList().push_back(CurInput);
    addInputFileToTar(CurInput, MappingFile::Bitcode);
  } else if (CurInput->getKind() == InputFile::ELFSymDefFileKind) {
    eld::RegisterTimer T("Read SymDef Object Files", "Read all Input files",
                         ThisConfig.options().printTimingStats());
    if (layoutInfo)
      layoutInfo->recordInputKind(CurInput->getKind());
    if (ThisConfig.codeGenType() != LinkerConfig::Exec) {
      ThisConfig.raise(Diag::symdef_incompatible_option);
      return false;
    }
    addInputFileToTar(CurInput, eld::MappingFile::Kind::SymDef);
    CurInput->setToSkip();
    // Just to make it simple, I just added template code.
    if (!getSymDefReader()->readHeader(*CurInput, IsPostLto))
      return false;
    if (!getSymDefReader()->readSections(*CurInput, IsPostLto))
      return false;
    if (!getSymDefReader()->readSymbols(*CurInput, IsPostLto)) {
      ThisConfig.raise(Diag::file_has_error) << Input->decoratedPath();
      return false;
    }
    if (layoutInfo)
      layoutInfo->recordInputActions(LayoutInfo::Load, Input);
  } else if (CurInput->getKind() == InputFile::ELFDynObjFileKind) {
    eld::RegisterTimer T("Read ELF Shared Object Files", "Read all Input files",
                         ThisConfig.options().printTimingStats());
    if (layoutInfo)
      layoutInfo->recordInputKind(CurInput->getKind());
    addInputFileToTar(CurInput, eld::MappingFile::SharedLibrary);
    CurInput->setToSkip();
    if (ThisConfig.isLinkPartial()) {
      ThisConfig.raise(Diag::err_shared_objects_in_partial_link)
          << Input->decoratedPath();
      return false;
    }
    if (Input->getAttribute().isStatic()) {
      ThisConfig.raise(Diag::err_mixed_shared_static_objects)
          << Input->decoratedPath();
      return false;
    }
    if (!isBackendInitialized()) {
      // Infer machine for selecting backend
      eld::Expected<uint16_t> Machine =
          getNewDynObjReader()->getMachine(*CurInput);
      if (!Machine) {
        ThisConfig.raiseDiagEntry(std::move(Machine.error()));
        return false;
      }
      llvm::cast<ObjectFile>(CurInput)->setMachine(*Machine);
      if (!initializeTarget(CurInput))
        return false;
    }
    auto ExpRead = getNewDynObjReader()->parseFile(*CurInput);
    // Currently, we have to consider two cases:
    // expRead returns an error: In this case, we report the error and return.
    // expRead returns false: Reading failed but we do not have error info
    // available. In this case, we just return false.
    if (!ExpRead.has_value() || !ExpRead.value()) {
      if (!ExpRead.has_value())
        ThisConfig.raiseDiagEntry(std::move(ExpRead.error()));
      return false;
    }
    ThisModule->getDynLibraryList().push_back(CurInput);
  } else if (CurInput->getKind() == InputFile::GNUArchiveFileKind) {
    eld::RegisterTimer T("Read Archive Files", "Read all Input files",
                         ThisConfig.options().printTimingStats());
    if (layoutInfo)
      layoutInfo->recordInputKind(CurInput->getKind());
    std::string NameSpecPath;
    if (Input->getInputType() == Input::Namespec)
      NameSpecPath = "-l" + Input->getFileName();
    ArchiveFile *CurArchive = llvm::dyn_cast<eld::ArchiveFile>(CurInput);
    if (ThisConfig.options().isInExcludeLIBS(Path, NameSpecPath))
      CurArchive->setNoExport();
    MemoryArea *MemArea = CurInput->getInput()->getMemArea();
    if (const ArchiveFile *AF = getArchiveFileFromMemoryAreaToAFMap(MemArea)) {
      ThisConfig.raise(Diag::verbose_reusing_archive_file_info)
          << Input->decoratedPath() << "\n";
      CurArchive->setArchiveFileInfo(AF->getArchiveFileInfo());
    }
    uint32_t NumObjects = 0;
    eld::Expected<uint32_t> ExpNumObjects =
        getArchiveParser()->parseFile(*CurInput);
    if (!ExpNumObjects) {
      ThisConfig.raise(Diag::error_read_archive) << Input->decoratedPath();
      ThisConfig.raiseDiagEntry(std::move(ExpNumObjects.error()));
      return false;
    }
    NumObjects = ExpNumObjects.value();
    if ((NumObjects == 0) && IsPostLto) {
      llvm::StringRef FileType = " (ELF)";
      if (CurArchive->isMixedArchive())
        FileType = " (Mixed)";
      else if (CurArchive->isBitcodeArchive())
        FileType = " (Bitcode)";
      else if (CurArchive->isELFArchive())
        FileType = " (ELF)";
      if (layoutInfo) {
        layoutInfo->recordInputActions(LayoutInfo::SkippedRescan, Input,
                                       FileType.str());
      }
    }
    ThisModule->getArchiveLibraryList().push_back(CurInput);
    addInputFileToTar(CurInput, MappingFile::Kind::Archive);
    addToMemoryAreaToAFMap(*CurArchive);
  }
  // try to parse input as a linker script
  else if (CurInput->getKind() == InputFile::GNULinkerScriptKind) {
    eld::RegisterTimer T("Read Linker Script", "Read all Input files",
                         ThisConfig.options().printTimingStats());
    if (layoutInfo)
      layoutInfo->recordInputKind(CurInput->getKind());
    addInputFileToTar(CurInput, eld::MappingFile::LinkerScript);
    CurInput->setToSkip();
    if (!readLinkerScript(CurInput)) {
      ThisModule->setFailure(true);
      return false;
    }
    LinkerScriptFile *LSFile = llvm::dyn_cast<eld::LinkerScriptFile>(CurInput);
    if (!LSFile->isAssignmentsProcessed())
      LSFile->processAssignments();
  }
  if (!ThisConfig.getDiagEngine()->diagnose()) {
    if (ThisModule->getPrinter()->isVerbose())
      ThisConfig.raise(Diag::function_has_error) << __PRETTY_FUNCTION__;
    return false;
  }
  return true;
}

bool ObjectLinker::overrideELFObjectWithBitCode(InputFile *CurInputFile) {
  // If -flto option is not passed to the linker, and there is not a list
  // to include, then just move on.
  if (!ThisConfig.options().hasLTO() && !LTOPatternList.size())
    return false;

  eld::RegisterTimer T("Read Mixed ELF/Bitcode Object Files",
                       "Read all Input files",
                       ThisConfig.options().printTimingStats());

  ELFObjectFile *EObj = llvm::dyn_cast<eld::ELFObjectFile>(CurInputFile);
  if (!EObj)
    return false;

  Input *CurInput = CurInputFile->getInput();

  std::string Path = CurInput->decoratedPath();

  ELFSection *LLVMBCSection = EObj->getLLVMBCSection();
  if (!LLVMBCSection)
    return false;

  bool MatchedPattern = false;
  if (LTOPatternList.size()) {
    uint64_t Hash = llvm::hash_combine(Path);
    // Check exclude list
    for (auto &I : LTOPatternList) {
      if (I->hasHash() && I->hashValue() == Hash) {
        MatchedPattern = true;
        break;
      }
      // Resolved Path Match.
      if (I->matched(Path) ||
          I->matched(CurInput->getResolvedPath().getFullPath())) {
        MatchedPattern = true;
        break;
      }
    }
  }

  // if -flto is used use --exclude-lto-filelist
  // if -flto is not used, use --include-lto-filelist
  if (ThisConfig.options().hasLTO()) {
    if (MatchedPattern)
      return false;
  } else {
    // The file needs to be included, if
    // -flto option is not used and there is a match.
    if (!MatchedPattern)
      return false;
  }

  if (ThisModule->getPrinter()->isVerbose())
    ThisConfig.raise(Diag::use_embedded_bitcode) << Path;

  // LTO is needed. Since Bitcode was chosen.
  ThisModule->setLTONeeded();

  llvm::StringRef LLVMBCContents =
      CurInputFile->getSlice(LLVMBCSection->offset(), LLVMBCSection->size());

  InputFile *OverrideBCFile = InputFile::createEmbedded(
      CurInput, LLVMBCContents, ThisConfig.getDiagEngine());

  ASSERT(OverrideBCFile->getKind() == InputFile::BitcodeFileKind,
         CurInput->decoratedPath());

  CurInput->overrideInputFile(OverrideBCFile);

  return true;
}

bool ObjectLinker::parseIncludeOrExcludeLTOfiles() {
  std::set<std::string> ListFiles;
  // if -flto is used use --exclude-lto-filelist
  // if -flto is not used, use --include-lto-filelist
  if (ThisConfig.options().hasLTO())
    ListFiles = ThisConfig.options().getExcludeLTOFiles();
  else
    ListFiles = ThisConfig.options().getIncludeLTOFiles();

  for (const std::string &Name : ListFiles) {
    std::unique_ptr<MemoryArea> List(new MemoryArea(Name));
    if (!List->Init(ThisConfig.getDiagEngine()))
      return false;
    llvm::StringRef Buffer = List->getContents();
    while (!Buffer.empty()) {
      std::pair<StringRef, StringRef> LineAndRest = Buffer.split('\n');
      StringRef Line = LineAndRest.first.trim();
      // Comment lines starts with #
      if (!Line.empty() || !Line.starts_with("#"))
        LTOPatternList.emplace_back(
            make<WildcardPattern>(ThisModule->saveString(Line)));
      Buffer = LineAndRest.second;
    }
  }
  return true;
}

bool ObjectLinker::provideGlobalSymbolAndContents(std::string Name, size_t Sz,
                                                  uint32_t Alignment) {
  LDSymbol *Sym = ThisModule->getNamePool().findSymbol(Name);

  if (!Sym || !Sym->resolveInfo() || !Sym->resolveInfo()->isUndef())
    return true;

  char *Buf = ThisModule->getUninitBuffer(Sz);
  ELFSection *InputSect = ThisModule->createInternalSection(
      Module::InternalInputType::GlobalDataSymbols, LDFileFormat::Regular,
      ".rodata.internal." + Name, llvm::ELF::SHT_PROGBITS, llvm::ELF::SHF_ALLOC,
      Alignment, 0);
  Fragment *F = make<RegionFragment>(llvm::StringRef(Buf, Sz), InputSect,
                                     Fragment::Type::Region, Alignment);
  InputSect->addFragmentAndUpdateSize(F);
  ELFObjectFile *EObj =
      llvm::dyn_cast<eld::ELFObjectFile>(InputSect->getInputFile());
  if (!EObj)
    return false;
  LayoutInfo *layoutInfo = ThisModule->getLayoutInfo();
  if (layoutInfo)
    layoutInfo->recordFragment(EObj, InputSect, F);
  LDSymbol *ProvideSym = ThisModule->getIRBuilder()->addSymbol(
      *EObj, Name, (ResolveInfo::Type)Sym->resolveInfo()->type(),
      ResolveInfo::Define, ResolveInfo::Global, Sz, 0, InputSect,
      Sym->resolveInfo()->visibility(), true, EObj->getSections().size(),
      EObj->getSymbols().size());
  EObj->addSymbol(ProvideSym);
  return true;
}

bool ObjectLinker::setCommonSectionsFallbackToBSS() {
  ObjectFile *CommonInput =
      llvm::dyn_cast<ObjectFile>(ThisModule->getCommonInternalInput());
  auto Iter = std::find_if(CommonInput->getSections().begin(),
                           CommonInput->getSections().end(), [](Section *S) {
                             return llvm::isa<CommonELFSection>(S) &&
                                    !S->getOutputSection();
                           });
  // Do not proceed if there are no unassigned common sections.
  if (Iter == CommonInput->getSections().end())
    return true;
  SectionMap &SectionMap = ThisModule->getScript().sectionMap();
  OutputSectionEntry *OutSecEntry = SectionMap.findOutputSectionEntry(".bss");
  ELFSection *OutSection = nullptr;
  RuleContainer *Rule = nullptr;

  if (OutSecEntry)
    OutSection = OutSecEntry->getSection();
  else {
    OutSection = ThisModule->createOutputSection(".bss", LDFileFormat::Regular,
                                                 /*pType=*/0, /*pFlag=*/0,
                                                 /*pAlign=*/0);
    OutSecEntry = OutSection->getOutputSection();
  }

  Rule = OutSecEntry->getLastRule();
  if (!Rule)
    Rule = OutSecEntry->createDefaultRule(*ThisModule);

  for (auto *S : CommonInput->getSections()) {
    if (!llvm::isa<CommonELFSection>(S) || S->getOutputSection())
      continue;
    S->setOutputSection(OutSecEntry);
    S->setMatchedLinkerScriptRule(Rule);
  }
  return true;
}

bool ObjectLinker::setCopyRelocSectionsFallbackToBSS() {
  ObjectFile *CopyRelocInput =
      llvm::dyn_cast<ObjectFile>(ThisModule->getInternalInput(
          Module::InternalInputType::CopyRelocSymbols));

  SectionMap &SectionMap = ThisModule->getScript().sectionMap();
  OutputSectionEntry *OutSecEntry = SectionMap.findOutputSectionEntry(".bss");
  ELFSection *OutSection = nullptr;
  RuleContainer *Rule = nullptr;

  if (OutSecEntry)
    OutSection = OutSecEntry->getSection();
  else {
    OutSection = ThisModule->createOutputSection(".bss", LDFileFormat::Regular,
                                                 /*pType=*/0, /*pFlag=*/0,
                                                 /*pAlign=*/0);
    OutSecEntry = OutSection->getOutputSection();
  }

  Rule = OutSecEntry->getLastRule();
  if (!Rule)
    Rule = OutSecEntry->createDefaultRule(*ThisModule);

  for (auto *S : CopyRelocInput->getSections()) {
    if (S->getOutputSection())
      continue;
    S->setOutputSection(OutSecEntry);
    S->setMatchedLinkerScriptRule(Rule);
  }
  return true;
}

void ObjectLinker::accountSymForSymStats(SymbolStats &SymbolStats,
                                         const ResolveInfo &RI) {
  SymbolStats.Local += RI.isLocal();
  // NOTE: We are not counting absolute symbols here.
  SymbolStats.Global += RI.isGlobal();
  SymbolStats.Weak += RI.isWeak();
  SymbolStats.Hidden += RI.isHidden();
  // NOTE: We have to explicitly add isFile() here because
  // we set absolute property to false for STT_File symbols.
  SymbolStats.Absolute += RI.isAbsolute() || RI.isFile();
  SymbolStats.ProtectedSyms += RI.isProtected();
  SymbolStats.File += RI.isFile();
}

void ObjectLinker::accountSymForTotalSymStats(const ResolveInfo &RI) {
  accountSymForSymStats(MTotalSymStats, RI);
}

void ObjectLinker::accountSymForDiscardedSymStats(const ResolveInfo &RI) {
  accountSymForSymStats(MDiscardedSymStats, RI);
}

const ObjectLinker::SymbolStats &ObjectLinker::getTotalSymbolStats() const {
  return MTotalSymStats;
}

const ObjectLinker::SymbolStats &ObjectLinker::getDiscardedSymbolStats() const {
  return MDiscardedSymStats;
}

void ObjectLinker::reportPendingPluginRuleInsertions() const {
  const LinkerScript &Script = ThisModule->getLinkerScript();
  const auto &PendingRuleInsertions = Script.getPendingRuleInsertions();
  for (const auto &Item : PendingRuleInsertions) {
    const plugin::LinkerWrapper *LW = Item.first;
    for (const RuleContainer *R : Item.second) {
      std::string RuleString;
      llvm::raw_string_ostream Ss(RuleString);
      R->desc()->dumpSpec(Ss);
      ThisConfig.raise(Diag::warn_pending_rule_insertion)
          << Ss.str() << LW->getPlugin()->getPluginName()
          << R->desc()->getOutputDesc().name();
    }
  }
}

void ObjectLinker::finalizeOutputSectionFlags(OutputSectionEntry *OSE) const {
  ELFSection *S = OSE->getSection();
  uint32_t OutFlags = S->getFlags();
  auto OutSectType = OSE->prolog().type();

  if (OutSectType == OutputSectDesc::Type::DSECT ||
      OutSectType == OutputSectDesc::Type::COPY ||
      OutSectType == OutputSectDesc::Type::INFO ||
      OutSectType == OutputSectDesc::Type::OVERLAY) {
    OutFlags = S->getFlags();
    S->setFlags(OutFlags & ~llvm::ELF::SHF_ALLOC);
  }
}

void ObjectLinker::collectEntrySections() const {
  const std::vector<InputFile *> &InputFiles = ThisModule->getObjectList();
  SectionMap &SM = ThisModule->getLinkerScript().sectionMap();
  std::vector<std::vector<ELFSection *>> InputToEntrySections(
      InputFiles.size());

  llvm::parallelFor(0, InputFiles.size(), [&](std::size_t I) {
    if (isPostLTOPhase() && InputFiles[I]->isBitcode())
      return;
    const ObjectFile *ObjFile = llvm::cast<ObjectFile>(InputFiles[I]);

    for (Section *S : ObjFile->getSections()) {
      ELFSection *ELFSect = llvm::dyn_cast<ELFSection>(S);
      if (!ELFSect)
        continue;
      RuleContainer *R = ELFSect->getMatchedLinkerScriptRule();

      if ((R && R->isEntry()) ||
          ELFSect->name().find("@") != llvm::StringRef::npos) {
        InputToEntrySections[I].push_back(ELFSect);
        continue;
      }

      for (auto *Entry : ThisConfig.targets().getEntrySections()) {
        if (SM.matched(*Entry, ELFSect->name(), ELFSect->getSectionHash())) {
          InputToEntrySections[I].push_back(ELFSect);
          break;
        }
      }
    }
  });

  for (const auto &EntrySections : InputToEntrySections) {
    for (ELFSection *ELFSect : EntrySections) {
      ThisConfig.raise(Diag::keeping_section)
          << ELFSect->name()
          << ELFSect->originalInput()->getInput()->decoratedPath();
      SM.addEntrySection(ELFSect);
    }
  }
}

ArchiveParser *ObjectLinker::createArchiveParser() {
  return make<eld::ArchiveParser>(*ThisModule);
}

ELFExecObjParser *ObjectLinker::createELFExecObjParser() {
  return make<eld::ELFExecObjParser>(*ThisModule);
}

BitcodeReader *ObjectLinker::createBitcodeReader() {
  return make<eld::BitcodeReader>(*ThisModule);
}

SymDefReader *ObjectLinker::createSymDefReader() {
  return make<eld::SymDefReader>(*ThisModule);
}

ELFDynObjParser *ObjectLinker::createDynObjReader() {
  return make<eld::ELFDynObjParser>(*ThisModule);
}

ELFRelocObjParser *ObjectLinker::createRelocObjParser() {
  return make<eld::ELFRelocObjParser>(*ThisModule);
}

BinaryFileParser *ObjectLinker::createBinaryFileParser() {
  return make<eld::BinaryFileParser>(*ThisModule);
}

ELFObjectWriter *ObjectLinker::createWriter() {
  return make<eld::ELFObjectWriter>(*ThisModule);
}

bool ObjectLinker::initializeTarget(InputFile *I) {
  ObjectFile *Obj = llvm::dyn_cast<ObjectFile>(I);
  if (!Obj)
    return {};
  uint16_t machine = Obj->getMachine();
  uint64_t is64bit = Obj->is64bit();
  if (!ThisModule->getLinker()->initializeTarget(machine, is64bit))
    return false;
  return true;
}
