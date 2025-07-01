//===- Linker.cpp----------------------------------------------------------===//
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
#include "eld/Core/Linker.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Fragment/FragmentRef.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/Input/InputBuilder.h"
#include "eld/Input/InternalInputFile.h"
#include "eld/LayoutMap/LayoutInfo.h"
#include "eld/Object/ObjectLinker.h"
#include "eld/Plugin/PluginManager.h"
#include "eld/PluginAPI/Expected.h"
#include "eld/PluginAPI/LinkerPlugin.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Readers/Relocation.h"
#include "eld/Script/ScriptAction.h"
#include "eld/Support/Memory.h"
#include "eld/Support/MsgHandling.h"
#include "eld/Support/ProgressBar.h"
#include "eld/Support/RegisterTimer.h"
#include "eld/Support/TargetRegistry.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/Target/GNULDBackend.h"
#include "llvm/Object/ELFTypes.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FileOutputBuffer.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdint>
#if !defined(_MSC_VER)
#include <unistd.h>
#else
#include <io.h>
#endif

using namespace eld;
using namespace llvm;

Linker::Linker(eld::Module &PModule, LinkerConfig &Config)
    : ThisModule(&PModule), ThisConfig(&Config), Backend(nullptr),
      ObjLinker(nullptr), IR(nullptr), LinkerProgress(nullptr),
      LinkTime(nullptr), TimingSectionTimer(nullptr), UnresolvedSymbolPolicy(0),
      BeginningOfTime(0) {
  IR = make<IRBuilder>(PModule, Config);
  ThisModule->setLinker(this);
  // Whenever you add an extra linker step, make sure you adjust the total
  // tick count in the progress bar.
  LinkerProgress =
      new ProgressBar(48, 80, ThisConfig->options().showProgressBar());
  LinkTime = make<llvm::Timer>("LinkTime", "LinkTime");
  BeginningOfTime = std::chrono::time_point_cast<std::chrono::microseconds>(
                        std::chrono::system_clock::now())
                        .time_since_epoch()
                        .count();
  LinkTime->startTimer();
  if (ThisConfig->options().getInsertTimingStats()) {
    TimingSectionTimer =
        make<llvm::Timer>("TimingSectionTimer", "TimingSectionTimer");
    TimingSectionTimer->startTimer();
  }
}

Linker::~Linker() { reset(); }

bool Linker::prepare(std::vector<InputAction *> &Actions,
                     const eld::Target *Target) {
  if (ThisModule->getPrinter()->isVerbose())
    ThisConfig->raise(Diag::initializing_linker);
  {
    eld::RegisterTimer F("Initialize Linker", "Link Summary",
                         ThisConfig->options().printTimingStats());
    if (!initEmulator(ThisModule->getScript(), Target))
      return false;
    if (!initBackend(Target))
      return false;

    if (!initializeInputTree(Actions))
      return false;
  }

  PluginManager &PM = ThisModule->getPluginManager();
  {
    if (ThisModule->getPrinter()->isVerbose())
      ThisConfig->raise(Diag::verbose_loading_universal_plugins);

    eld::RegisterTimer T("Load Linker Plugins", "Plugins",
                         ThisConfig->options().printTimingStats("Plugin"));

    if (!ThisModule->readPluginConfig()) {
      ThisModule->setFailure(true);
      return false;
    }

    if (!ThisModule->getScript().loadUniversalPlugins(*ThisModule)) {
      ThisModule->setFailure(true);
      return false;
    }

    PM.storeUniversalPlugins();

    // Find the AdvancedLTO plugin. This plugin is special mainly because only
    // one plugin of the LTO type can be present. Linker addresses this plugin
    // individually, not by broadcasting LTO hooks to all plugins.
    for (auto &P : PM.getUniversalPlugins()) {
      if (P->getName() == "AdvancedLTO") {
        ObjLinker->setLTOPlugin(
            *llvm::cast<plugin::LinkerPlugin>(P->getLinkerPlugin()));
        break;
      }
    }

    if (!PM.callInitHook())
      return false;
    if (!PM.processPluginCommandLineOptions(ThisConfig->options()))
      return false;
  }

  reportUnknownOptions();

  if (ThisModule->getPrinter()->isVerbose()) {
    ThisConfig->raise(Diag::entry_symbol) << Backend->getEntry();
    ThisConfig->raise(Diag::reading_input_files);
  }

  {
    eld::RegisterTimer F("Read all Input files", "Link Summary",
                         ThisConfig->options().printTimingStats());
    if (!normalize())
      return false;
  }
  return true;
}

bool Linker::link() {
  if (ThisModule->getPrinter()->isVerbose()) {
    ThisConfig->raise(Diag::beginning_link);
    ThisConfig->raise(Diag::initializing_standard_sections);
  }
  // Set link launch directory
  llvm::SmallString<128> LinkLaunchDirectory;
  llvm::sys::fs::current_path(LinkLaunchDirectory);
  ThisConfig->options().setLinkLaunchDirectory(LinkLaunchDirectory.str().str());
  {
    eld::RegisterTimer F("Apply Linker default sections", "Link Summary",
                         ThisConfig->options().printTimingStats());
    if (!ObjLinker->initStdSections())
      return false;
  }

  // Init per-file synthetic dynamic sections.
  if (LinkerConfig::Object != ThisConfig->codeGenType()) {
    for (auto &Input : ThisModule->getObjectList())
      if (ELFObjectFile *ELFObj = llvm::dyn_cast<ELFObjectFile>(Input))
        Backend->initDynamicSections(*ELFObj);
  }

  if (ThisModule->getPrinter()->isVerbose())
    ThisConfig->raise(Diag::merging_input_sections);
  {
    eld::RegisterTimer F("Merge input sections and prepare for layout",
                         "Link Summary",
                         ThisConfig->options().printTimingStats());
    if (!resolve()) {
      // llvm::errs() << "resolve returning false!\n";
      return false;
    }
  }

  if (ThisModule->getPrinter()->isVerbose())
    ThisConfig->raise(Diag::begin_layout);

  {
    PluginManager &PM = ThisModule->getPluginManager();
    if (!PM.callActBeforePerformingLayoutHook()) {
      // llvm::errs() << "callActBeforePerformingLayoutHook returning false!\n";
      return false;
    }
  }

  {
    eld::RegisterTimer F("Prepare Output file", "Link Summary",
                         ThisConfig->options().printTimingStats());
    if (!layout()) {
      // llvm::errs() << "layout returning false!\n";
      return false;
    }
  }

  if (ThisModule->getPrinter()->isVerbose())
    ThisConfig->raise(Diag::emit_output_file)
        << ThisConfig->options().outputFileName();
  {
    eld::RegisterTimer F("Emit output file", "Link Summary",
                         ThisConfig->options().printTimingStats());
    if (ThisConfig->options().getInsertTimingStats()) {
      TimingSectionTimer->stopTimer();
      uint64_t DurationMicro =
          TimingSectionTimer->getTotalTime().getWallTime() * 1000000;
      ObjLinker->getWriter()->writeLinkTimeStats(*ThisModule, BeginningOfTime,
                                                 DurationMicro);
      TimingSectionTimer->clear();
    }
    // llvm::errs() << "emit returning false!\n";
    return emit();
  }
}

void Linker::printLayout() {
  // FIXME: In which case would the backend and object linker
  // be nullptr here? Should't these be assert instead?
  if (!Backend || !ObjLinker) {
    LinkTime->stopTimer();
    LinkTime->clear();
    return;
  }

  // FIXME: Verbose check can be removed.
  if (ThisModule->getPrinter()->isVerbose())
    ThisConfig->raise(Diag::printing_layout);

  // Stop, record and clear the timer.
  LinkTime->stopTimer();
  LayoutInfo *layoutInfo = ThisModule->getLayoutInfo();
  if (layoutInfo) {
    layoutInfo->recordLinkTime(LinkTime->getTotalTime().getWallTime());
    layoutInfo->recordThreadCount();
  }
  LinkTime->clear();

  eld::RegisterTimer F("Emit Map file", "Link Summary",
                       ThisConfig->options().printTimingStats());
  ObjLinker->printlayout();
}

bool Linker::activateInputs(std::vector<InputAction *> &Actions) {
  LinkerProgress->incrementAndDisplayProgress();
  for (auto &Action : Actions) {
    if (!Action->isScript()) {
      if (!Action->activate(IR->getInputBuilder()))
        return false;
      continue;
    }
    if (!Action->activate(IR->getInputBuilder()))
      return false;
    if (!ObjLinker->readLinkerScript(
            llvm::dyn_cast<eld::ScriptAction>(Action)->getLinkerScriptFile())) {
      ThisModule->setFailure(true);
      return false;
    }
  }
  return true;
}

bool Linker::initializeInputTree(std::vector<InputAction *> &Actions) {

  // Prefer static libraries over dynamic libraries with omagic
  if (ThisConfig->options().isOMagic())
    IR->getInputBuilder().makeBStatic();

  {
    LinkerProgress->incrementAndDisplayProgress();
    eld::RegisterTimer T("Input Activation", "Initialize",
                         ThisConfig->options().printTimingStats());
    Backend->initializeAttributes();
  }

  {
    LinkerProgress->incrementAndDisplayProgress();
    eld::RegisterTimer T("Create Internal Files", "Initialize",
                         ThisConfig->options().printTimingStats());
    ThisModule->createInternalInputs();
  }
  {
    LinkerProgress->incrementAndDisplayProgress();
    eld::RegisterTimer T("Activate Inputs", "Initialize",
                         ThisConfig->options().printTimingStats());
    if (!activateInputs(Actions))
      return false;
  }

  eld::RegisterTimer T("More Options", "Initialize",
                       ThisConfig->options().printTimingStats());
  LinkerProgress->incrementAndDisplayProgress();
  Backend->setOptions();

  if (!verifyLinkerScript())
    return false;

  return true;
}

/// normalize - to convert the command line language to the input tree.
bool Linker::normalize() {
  if (ThisModule->getPrinter()->traceCommandLine()) {
    ThisConfig->printOptions(llvm::outs(), *Backend,
                             ThisConfig->options().color());
  }

  bool TraceLto = ThisConfig->options().traceLTO();

  // 1. - normalize the input tree
  //   read out sections and symbol/string tables (from the files) and
  //   set them in Module. When reading out the symbol, resolve their symbols
  //   immediately and set their ResolveInfo (i.e., Symbol Resolution).
  if (TraceLto)
    ThisConfig->raise(Diag::note_lto_phase) << 1;

  {
    LinkerProgress->incrementAndDisplayProgress();
    eld::RegisterTimer T("Read all Input files", "Read all Input files",
                         ThisConfig->options().printTimingStats());
    if (!ObjLinker->normalize())
      return false;
  }

  if (ThisModule->getPrinter()->isVerbose())
    ThisConfig->raise(Diag::verbose_loading_non_universal_plugins);
  {
    LinkerProgress->incrementAndDisplayProgress();
    eld::RegisterTimer T("Load Linker Plugins", "Plugins",
                         ThisConfig->options().printTimingStats("Plugin"));
    // Load plugins.
    if (!loadNonUniversalPlugins())
      return false;
  }

  // 2. - set up code position
  LinkerProgress->incrementAndDisplayProgress();
  if (LinkerConfig::DynObj == ThisConfig->codeGenType() ||
      ThisConfig->options().isPIE()) {
    ThisConfig->setCodePosition(LinkerConfig::Independent);
  } else if (ThisModule->getDynLibraryList().empty()) {
    // If the output is dependent on its loaded address, and it does not need
    // to call outside functions, then we can treat the output static dependent
    // and perform better optimizations.
    ThisConfig->setCodePosition(LinkerConfig::StaticDependent);
  } else {
    ThisConfig->setCodePosition(LinkerConfig::DynamicDependent);
  }

  if ((ThisConfig->options().isPatchEnable() ||
       ThisConfig->options().getPatchBase()) &&
      !ThisConfig->isCodeStatic()) {
    ThisConfig->raise(Diag::err_patch_not_static);
    return false;
  }

  setUnresolvePolicy(ThisConfig->options().reportUndefPolicy());

  {
    eld::RegisterTimer T("Parse External scripts ", "Read all Input files",
                         ThisConfig->options().printTimingStats());
    LinkerProgress->incrementAndDisplayProgress();
    if (!ObjLinker->parseVersionScript())
      return false;
    if (ThisModule->getPrinter()->isVerbose())
      ThisConfig->raise(Diag::parsed_version_script);
    LinkerProgress->incrementAndDisplayProgress();
    if (ThisConfig->isCodeDynamic() || ThisConfig->options().forceDynamic()) {
      if (ThisModule->getPrinter()->isVerbose())
        ThisConfig->raise(Diag::parsing_dynlist);
      ObjLinker->parseDynList();
    }

    {
      LinkerProgress->incrementAndDisplayProgress();
      eld::RegisterTimer T("Add Script Symbols", "Symbols from Linker Script",
                           ThisConfig->options().printTimingStats());
      // Add Linker script symbols.
      if (!ObjLinker->addScriptSymbols())
        return false;
      if (ThisModule->getPrinter()->isVerbose())
        ThisConfig->raise(Diag::adding_script_symbols);
    }
  }

  // LTO Specific Steps
  if (ThisModule->needLTOToBeInvoked() || ThisConfig->options().hasLTO()) {
    LinkerProgress->addMoreTicks(3);
    LinkerProgress->incrementAndDisplayProgress();
    eld::RegisterTimer T("Perform LTO", "LTO",
                         ThisConfig->options().printTimingStats());
    // a. Create LTO Object file from bitcode inputs
    if (!ObjLinker->createLTOObject())
      return false;

    if (ThisModule->getPrinter()->isVerbose())
      ThisConfig->raise(Diag::beginning_post_LTO_phase);
    LinkerProgress->incrementAndDisplayProgress();
    ObjLinker->beginPostLTO();
    // c. Read and resolve symbols for the new inputs discarding
    // bitcode files and including the generated LTO Object file
    if (TraceLto)
      ThisConfig->raise(Diag::note_lto_phase) << 2;
    LinkerProgress->incrementAndDisplayProgress();
    if (!ObjLinker->normalize())
      return false;
  }
  return true;
}

bool Linker::resolve() {
  {
    LinkerProgress->incrementAndDisplayProgress();
    eld::RegisterTimer T("Read all relocations",
                         "Process Relocations from Input files",
                         ThisConfig->options().printTimingStats());
    ObjLinker->readRelocations();
  }

  if (!ThisConfig->getDiagEngine()->diagnose()) {
    if (ThisModule->getPrinter()->isVerbose())
      ThisConfig->raise(Diag::function_has_error) << __PRETTY_FUNCTION__;
    return false;
  }

  {
    LinkerProgress->incrementAndDisplayProgress();
    eld::RegisterTimer T("Allocate Common Symbols", "Common Symbols",
                         ThisConfig->options().printTimingStats());
    if (!ObjLinker->allocateCommonSymbols()) {
      return false;
    }
  }

  {
    LinkerProgress->incrementAndDisplayProgress();
    eld::RegisterTimer T(
        "Assign output sections using default/linker script rules",
        "Match Default/Linker script rules",
        ThisConfig->options().printTimingStats());
    LinkerProgress->incrementAndDisplayProgress();

    // Add dynamic section header inputs to the front of the input list.
    ThisModule->getObjectList().insert(
        ThisModule->getObjectList().begin(),
        Backend->getDynamicSectionHeadersInputFile());

    // Add all internal inputs
    for (auto &Obj : ThisModule->getInternalFiles()) {
      ThisModule->getObjectList().push_back(Obj);
    }

    PluginManager &PM = ThisModule->getPluginManager();
    PM.callActBeforeRuleMatchingHook();

    // Assign output sections.
    LinkerProgress->incrementAndDisplayProgress();
    ObjLinker->assignOutputSections(ThisModule->getObjectList());

    // Mark internal sections created by the linker and set to discard if any.
    LinkerProgress->incrementAndDisplayProgress();
    ObjLinker->markDiscardFileFormatSections();

    // Targets can update any information, if they care about.
    LinkerProgress->incrementAndDisplayProgress();
    Backend->finishAssignOutputSections();
  }

  if (!ThisConfig->getDiagEngine()->diagnose()) {
    if (ThisModule->getPrinter()->isVerbose())
      ThisConfig->raise(Diag::function_has_error) << __PRETTY_FUNCTION__;
    return false;
  }

  {
    eld::RegisterTimer T("Add Standard Symbols", "Add Default Standard Symbols",
                         ThisConfig->options().printTimingStats());
    LinkerProgress->incrementAndDisplayProgress();
    // Add all symbols (Standard, Target specific symbols)
    if (!ObjLinker->addStandardSymbols() || !ObjLinker->addTargetSymbols())
      return false;
  }

  {
    eld::RegisterTimer T("Target Specific Input processing",
                         "Target specific Input processing",
                         ThisConfig->options().printTimingStats());
    LinkerProgress->incrementAndDisplayProgress();
    if (!ObjLinker->processInputFiles())
      return false;
  }

  // Garbage collect.
  LinkerProgress->incrementAndDisplayProgress();
  if (LinkerConfig::Object != ThisConfig->codeGenType()) {
    ObjLinker->dataStrippingOpt();
  }

  recordCommonSymbols();

  // Run the section Iterator Plugin.
  {
    LinkerProgress->incrementAndDisplayProgress();
    eld::RegisterTimer T("Run Section Iterator Plugin",
                         "Section Iterator Plugin",
                         ThisConfig->options().printTimingStats("Plugin"));
    if (!ObjLinker->runSectionIteratorPlugin()) {
      return false;
    }
  }

  // When linking the patch, most relocations need to resolve to the PLT stub
  // from the base image. The address of the stub is communicated as the value
  // of the `__llvm_patchable_` absolute symbol.
  if (ThisConfig->options().getPatchBase()) {
    for (auto &G : ThisModule->getNamePool().getGlobals()) {
      ResolveInfo *SymInfo = G.getValue();
      // We look for an alias for EVERY symbol, not only for the patchable ones
      // because, during symbol resolution, if a patchable symbol is redefined
      // in the patch, its patchable definition from the base will be replaced
      // with the definition from the patch, which may not carry the patchable
      // attribute. This much depends on the details of the symbol resolution.
      // It may be possible to propagate the attribute during resolution.
      if (LDSymbol *PatchableAlias = ThisModule->getNamePool().findSymbol(
              std::string("__llvm_patchable_") + SymInfo->name())) {
        Backend->recordAbsolutePLT(SymInfo, PatchableAlias->resolveInfo());
        SymInfo->setReserved(SymInfo->reserved() | Relocator::ReservePLT);
      }
    }
  }

  LinkerProgress->incrementAndDisplayProgress();
  if (LinkerConfig::Object != ThisConfig->codeGenType()) {
    eld::RegisterTimer T("Scan Relocation Processing", "Relocation Processing",
                         ThisConfig->options().printTimingStats());
    bool Success = ObjLinker->scanRelocations(false);
    if (!Success)
      return false;
  }

  LinkerProgress->incrementAndDisplayProgress();
  if (ThisConfig->isCodeDynamic() || ThisConfig->options().forceDynamic()) {
    eld::RegisterTimer T("Dynamic List Symbols", "Add Dynamic List Symbols",
                         ThisConfig->options().printTimingStats());
    if (!ObjLinker->addDynListSymbols())
      return false;
  }

  LinkerProgress->incrementAndDisplayProgress();
  {
    eld::RegisterTimer T("Finalize Scan Relocation Processing",
                         "Relocation Processing",
                         ThisConfig->options().printTimingStats());
    if (!ObjLinker->finalizeScanRelocations())
      return false;
  }

  LinkerProgress->incrementAndDisplayProgress();
  {
    eld::RegisterTimer T("Add Output symbols", "Output Symbols",
                         ThisConfig->options().printTimingStats());
    // Add symbols that we need to the output.
    if (!ObjLinker->addSymbolsToOutput())
      return false;
  }

  LinkerProgress->incrementAndDisplayProgress();
  {
    eld::RegisterTimer T("Add Dynamic Output symbols", "Output Symbols",
                         ThisConfig->options().printTimingStats());
    // To allocate dynamic sections, we need dynamic symbols to be populated
    // properly.
    if (!ObjLinker->addDynamicSymbols())
      return false;
  }

  // Merge sections.
  {
    LinkerProgress->incrementAndDisplayProgress();
    eld::RegisterTimer T("Merge Sections", "Perform Layout",
                         ThisConfig->options().printTimingStats());
    if (!ObjLinker->mergeSections())
      return false;
  }

  {
    LinkerProgress->incrementAndDisplayProgress();
    eld::RegisterTimer T("Add Output Section symbols", "Output Symbols",
                         ThisConfig->options().printTimingStats());
    // Add all the section symbols
    if (!ObjLinker->addSectionSymbols())
      return false;
  }

  return true;
}

bool Linker::layout() {
  //  init relaxation stuff.
  {
    LinkerProgress->incrementAndDisplayProgress();
    eld::RegisterTimer T("Initialize Stubs", "Perform Layout",
                         ThisConfig->options().printTimingStats());
    ObjLinker->initStubs();
  }

  {
    LinkerProgress->incrementAndDisplayProgress();
    eld::RegisterTimer T("Do PreLayout", "Perform Layout",
                         ThisConfig->options().printTimingStats());
    ObjLinker->prelayout();
  }

  if (ThisModule->getPrinter()->isVerbose())
    ThisConfig->raise(Diag::merging_strings);
  {
    eld::RegisterTimer F("Merge strings", "Link Summary",
                         ThisConfig->options().printTimingStats());
    ObjLinker->doMergeStrings();
  }

  {
    LinkerProgress->incrementAndDisplayProgress();
    eld::RegisterTimer T("Establish Layout", "Perform Layout",
                         ThisConfig->options().printTimingStats());
    if (!ObjLinker->layout())
      return false;
  }

  {
    LinkerProgress->incrementAndDisplayProgress();
    eld::RegisterTimer T("Scan Relocations", "Perform Layout",
                         ThisConfig->options().printTimingStats());
    if (LinkerConfig::Object == ThisConfig->codeGenType()) {
      if (!ObjLinker->scanRelocations(true))
        return false;
    }
  }

  {
    LinkerProgress->incrementAndDisplayProgress();
    eld::RegisterTimer T("Create Output Section Table", "Perform Layout",
                         ThisConfig->options().printTimingStats());
    if (!ObjLinker->postlayout())
      return false;
  }

  {
    LinkerProgress->incrementAndDisplayProgress();
    eld::RegisterTimer T("Finalize Target specific symbol Values",
                         "Perform Layout",
                         ThisConfig->options().printTimingStats());
    Backend->finalizeSymbols();
  }

  {
    LinkerProgress->incrementAndDisplayProgress();
    eld::RegisterTimer T("AfterLayout OutputSection Iterator", "Perform Layout",
                         ThisConfig->options().printTimingStats("plugin"));
    // Run the output section iterator plugin after all the layout is done.
    ThisModule->setState(plugin::LinkerWrapper::AfterLayout);
    Backend->finalizeLayout();

    {
      if (!ObjLinker->runOutputSectionIteratorPlugin()) {
        ThisModule->setFailure(true);
        return false;
      }
    }
  }

  {
    LinkerProgress->incrementAndDisplayProgress();
    eld::RegisterTimer T("Apply Relocation", "Perform Layout",
                         ThisConfig->options().printTimingStats());
    ObjLinker->relocation(ThisConfig->options().emitRelocs());
  }

  {
    LinkerProgress->incrementAndDisplayProgress();
    eld::RegisterTimer T("Finalize Input Symbol Values", "Perform Layout",
                         ThisConfig->options().printTimingStats());
    ObjLinker->finalizeSymbolValues();
  }

  {
    LinkerProgress->incrementAndDisplayProgress();
    eld::RegisterTimer T("Finalize Output File", "Perform Layout",
                         ThisConfig->options().printTimingStats());
    ObjLinker->finalizeBeforeWrite();
  }

  if (!ThisConfig->getDiagEngine()->diagnose()) {
    if (ThisModule->getPrinter()->isVerbose())
      ThisConfig->raise(Diag::function_has_error) << __PRETTY_FUNCTION__;
    return false;
  }
  return true;
}

bool Linker::emit() {
  eld::RegisterTimer T("Emit Output File", "Emit Output File",
                       ThisConfig->options().printTimingStats());
  std::string Path = ThisConfig->options().outputFileName();

  int Perm = 0;
  switch (ThisConfig->codeGenType()) {
  case eld::LinkerConfig::Unknown:
  case eld::LinkerConfig::Object:
    Perm = 0x644;
    break;
  case eld::LinkerConfig::DynObj:
  case eld::LinkerConfig::Exec:
  case eld::LinkerConfig::Binary:
    Perm = 0x755;
    break;
  default:
    assert(0 && "Unknown file type");
  }

  LinkerProgress->incrementAndDisplayProgress();
  size_t OutputFileSize = 0;
  if (ThisConfig->targets().is32Bits())
    OutputFileSize =
        ObjLinker->getWriter()->getOutputSize<llvm::object::ELF32LE>(
            *ThisModule);
  else if (ThisConfig->targets().is64Bits())
    OutputFileSize =
        ObjLinker->getWriter()->getOutputSize<llvm::object::ELF64LE>(
            *ThisModule);
  uint64_t MaxImageSize =
      (ThisConfig->targets().is32Bits() ? UINT32_MAX : INT64_MAX);
  if (OutputFileSize > MaxImageSize) {
    ThisConfig->raise(Diag::err_output_size_too_large) << OutputFileSize;
    return false;
  }
  LayoutInfo *layoutInfo = ThisModule->getLayoutInfo();
  if (layoutInfo)
    layoutInfo->recordOutputFileSize(OutputFileSize);

  {
    std::error_code Ec;
    int OutputFlag = 0;
    if (Perm & 0x755)
      OutputFlag = llvm::FileOutputBuffer::F_executable;
    auto OutputOrError =
        llvm::FileOutputBuffer::create(Path, OutputFileSize, OutputFlag);
    // If there is an error, return with a fatal error message.
    if (!OutputOrError) {
      ThisConfig->raise(Diag::fatal_unwritable_output)
          << Path << llvm::toString(OutputOrError.takeError());
      return false;
    }
    {
      LinkerProgress->incrementAndDisplayProgress();
      eld::RegisterTimer T("Write All Sections", "Emit Output File",
                           ThisConfig->options().printTimingStats());
      ObjLinker->emitOutput(*OutputOrError.get());
    }

    LinkerProgress->incrementAndDisplayProgress();
    eld::Expected<void> ExpPostProcess =
        ObjLinker->postProcessing(*OutputOrError.get());
    if (!ExpPostProcess) {
      ThisConfig->raiseDiagEntry(std::move(ExpPostProcess.error()));
      return false;
    }

    // Update Build ID and sync
    eld::Expected<void> E =
        Backend->finalizeAndEmitBuildID(*OutputOrError.get());
    if (!E) {
      ThisConfig->raiseDiagEntry(std::move(E.error()));
      return false;
    }

    {
      LinkerProgress->incrementAndDisplayProgress();
      eld::RegisterTimer T("Commit File", "Emit Output File",
                           ThisConfig->options().printTimingStats());
      if (auto E = (*OutputOrError)->commit()) {
        ThisConfig->raise(Diag::unable_to_write_output_file)
            << Path << llvm::toString(std::move(E));
        return false;
      }
    }
  }

  LinkerProgress->incrementAndDisplayProgress();
  if ((Path != "/dev/null") && (ThisConfig->options().verifyLink())) {
    llvm::sys::fs::file_status FileStatus;
    std::error_code Ec = llvm::sys::fs::status(Path, FileStatus);
    if (Ec != std::error_code()) {
      ThisConfig->raise(Diag::output_file_error) << Path << Ec.message();
      return false;
    }
    if (FileStatus.getSize() != OutputFileSize) {
      std::stringstream S1;
      std::stringstream S2;
      S1 << OutputFileSize;
      S2 << FileStatus.getSize();
      ThisConfig->raise(Diag::output_file_not_complete)
          << Path << S1.str() << S2.str();
      return false;
    }
  }
  if (!ThisConfig->getDiagEngine()->diagnose()) {
    if (ThisModule->getPrinter()->isVerbose())
      ThisConfig->raise(Diag::function_has_error) << __PRETTY_FUNCTION__;
    return false;
  }
  return true;
}

bool Linker::reset() {
  Backend = nullptr;
  // initEmulator does not create the ObjectLinker
  if (ObjLinker) {
    ObjLinker->close();
    ObjLinker = nullptr;
  }
  delete LinkerProgress;
  return true;
}

bool Linker::initBackend(const eld::Target *PTarget) {
  if (ThisModule->getPrinter()->isVerbose())
    ThisConfig->raise(Diag::initializing_backend);
  eld::RegisterTimer T("Initialize Backend", "Initialize",
                       ThisConfig->options().printTimingStats());
  Backend = PTarget->createLDBackend(*ThisModule);
  LinkerProgress->incrementAndDisplayProgress();
  bool HasError = false;
  if (nullptr == Backend || !PTarget->IsImplemented) {
    std::string AvailableTargets;
    for (auto Target = TargetRegistry::begin(); Target != TargetRegistry::end();
         Target++) {
      if ((*Target)->IsImplemented)
        AvailableTargets += (*Target)->name() + std::string(" ");
    }
    ThisConfig->raise(Diag::err_cannot_init_backend)
        << ThisConfig->targets().triple().str() << AvailableTargets;
    HasError = true;
  }
  if ((Backend && ThisConfig->options().validateArchOptions() &&
       !Backend->validateArchOpts()) ||
      HasError)
    return false;

  if (ThisModule->getPrinter()->isVerbose())
    ThisConfig->raise(Diag::initializing_object_linker);
  ObjLinker = make<ObjectLinker>(*ThisConfig, *Backend);
  ObjLinker->initialize(*ThisModule, *IR);

  Backend->setDefaultConfigs();
  return true;
}

bool Linker::initEmulator(LinkerScript &CurScript, const eld::Target *PTarget) {
  eld::RegisterTimer T("Initialize Emulator", "Initialize",
                       ThisConfig->options().printTimingStats());
  if (ThisModule->getPrinter()->isVerbose())
    ThisConfig->raise(Diag::initializing_emulator);
  LinkerProgress->incrementAndDisplayProgress();
  return PTarget->emulate(CurScript, *ThisConfig);
}

bool Linker::verifyLinkerScript() {
  LinkerScript &CurScript = ThisModule->getScript();
  if (CurScript.phdrsSpecified() &&
      !CurScript.linkerScriptHasSectionsCommand()) {
    ThisConfig->raise(Diag::linker_script_uses_phdrs_no_sections);
    ThisModule->setFailure(true);
    return false;
  }
  return true;
}

bool Linker::loadNonUniversalPlugins() {
  ASSERT(ThisModule, "Module must not be null!");
  if (ThisModule->getPrinter()->isVerbose())
    ThisConfig->raise(Diag::verbose_loading_non_universal_plugins);

  if (!ThisModule->getScript().loadNonUniversalPlugins(*ThisModule)) {
    ThisModule->setFailure(true);
    return false;
  }
  return true;
}

void Linker::unloadPlugins() {
  if (ThisModule->getPrinter()->isVerbose())
    ThisConfig->raise(Diag::unloading_plugins);
  ThisModule->getScript().unloadPlugins(ThisModule);
}

void Linker::setUnresolvePolicy(llvm::StringRef Option) {
  UnresolvedSymbolPolicy =
      llvm::StringSwitch<int>(Option)
          .Case("ignore-all", UnresolvedPolicy::IgnoreAll)
          .Case("report-all", UnresolvedPolicy::ReportAll)
          .Case("ignore-in-object-files", UnresolvedPolicy::IgnoreInObjectFiles)
          .Case("ignore-in-shared-libs",
                UnresolvedPolicy::IgnoreInSharedLibrary)
          .Default(UnresolvedPolicy::NotSet);
  if (UnresolvedSymbolPolicy == UnresolvedPolicy::NotSet &&
      ThisConfig->isBuildingExecutable())
    UnresolvedSymbolPolicy = UnresolvedPolicy::ReportAll;
}

void Linker::recordCommonSymbol(const ResolveInfo *R) {
  LayoutInfo *layoutInfo = ThisModule->getLayoutInfo();
  if (!layoutInfo)
    return;
  LDSymbol *Sym = R->outSymbol();
  Fragment *F = Sym->fragRef()->frag();
  ASSERT(F, "All non-bitcode common symbols should have a corresponding "
            "fragment!");
  ELFSection *S = F->getOwningSection();
  layoutInfo->recordFragment(Sym->resolveInfo()->resolvedOrigin(), S, F);
  layoutInfo->recordSymbol(F, Sym);
}

void Linker::recordCommonSymbols() {
  if (!ThisModule->getLayoutInfo())
    return;
  // Common symbols are only allocated for partial links if define common
  // option is specified.
  if (ThisConfig->isLinkPartial() && !ThisConfig->options().isDefineCommon())
    return;
  for (const auto &G : ThisModule->getNamePool().getGlobals()) {
    ResolveInfo *R = G.getValue();
    // Skip bitcode common symbols.
    if (!R->isCommon() || R->isBitCode())
      continue;
    recordCommonSymbol(R);
  }
}

void Linker::reportUnknownOptions() const {
  const auto &Options = ThisConfig->options();
  if (Options.shouldIgnoreUnknownOptions())
    return;
  const auto &UnknownOptions = Options.getUnknownOptions();
  for (const auto &Option : UnknownOptions)
    ThisConfig->raise(Diag::warn_unsupported_option) << Option;
}
