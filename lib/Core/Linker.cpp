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
#include "eld/LayoutMap/LayoutPrinter.h"
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

Linker::Linker(eld::Module &pModule, LinkerConfig &pConfig)
    : m_pModule(&pModule), m_pConfig(&pConfig), m_pBackend(nullptr),
      m_pObjLinker(nullptr), m_pBuilder(nullptr), m_pProgressBar(nullptr),
      m_LinkTime(nullptr), m_TimingSectionTimer(nullptr),
      UnresolvedSymbolPolicy(0), m_BeginningOfTime(0) {
  m_pBuilder = make<IRBuilder>(pModule, pConfig);
  m_pModule->setLinker(this);
  // Whenever you add an extra linker step, make sure you adjust the total
  // tick count in the progress bar.
  m_pProgressBar =
      new ProgressBar(48, 80, m_pConfig->options().showProgressBar());
  m_LinkTime = make<llvm::Timer>("LinkTime", "LinkTime");
  m_BeginningOfTime = std::chrono::time_point_cast<std::chrono::microseconds>(
                          std::chrono::system_clock::now())
                          .time_since_epoch()
                          .count();
  m_LinkTime->startTimer();
  if (m_pConfig->options().getInsertTimingStats()) {
    m_TimingSectionTimer =
        make<llvm::Timer>("TimingSectionTimer", "TimingSectionTimer");
    m_TimingSectionTimer->startTimer();
  }
}

Linker::~Linker() { reset(); }

bool Linker::prepare(std::vector<InputAction *> &actions,
                     const eld::Target *target) {
  if (m_pModule->getPrinter()->isVerbose())
    m_pConfig->raise(diag::initializing_linker);
  {
    eld::RegisterTimer F("Initialize Linker", "Link Summary",
                         m_pConfig->options().printTimingStats());
    if (!initEmulator(m_pModule->getScript(), target))
      return false;
    if (!initBackend(target))
      return false;

    if (!initializeInputTree(actions))
      return false;
  }

  PluginManager &PM = m_pModule->getPluginManager();
  {
    if (m_pModule->getPrinter()->isVerbose())
      m_pConfig->raise(diag::verbose_loading_universal_plugins);

    eld::RegisterTimer T("Load Linker Plugins", "Plugins",
                         m_pConfig->options().printTimingStats("Plugin"));

    if (!m_pModule->readPluginConfig()) {
      m_pModule->setFailure(true);
      return false;
    }

    if (!m_pModule->getScript().loadUniversalPlugins(*m_pModule)) {
      m_pModule->setFailure(true);
      return false;
    }

    PM.storeUniversalPlugins();

    // Find the AdvancedLTO plugin. This plugin is special mainly because only
    // one plugin of the LTO type can be present. Linker addresses this plugin
    // individually, not by broadcasting LTO hooks to all plugins.
    for (auto &P : PM.getUniversalPlugins()) {
      if (P->getName() == "AdvancedLTO") {
        m_pObjLinker->setLTOPlugin(
            *llvm::cast<plugin::LinkerPlugin>(P->getLinkerPlugin()));
        break;
      }
    }

    if (!PM.callInitHook())
      return false;
    if (!PM.processPluginCommandLineOptions(m_pConfig->options()))
      return false;
  }

  reportUnknownOptions();

  if (m_pModule->getPrinter()->isVerbose()) {
    m_pConfig->raise(diag::entry_symbol) << m_pBackend->getEntry();
    m_pConfig->raise(diag::reading_input_files);
  }

  {
    eld::RegisterTimer F("Read all Input files", "Link Summary",
                         m_pConfig->options().printTimingStats());
    if (!normalize())
      return false;
  }
  return true;
}

bool Linker::link() {
  if (m_pModule->getPrinter()->isVerbose()) {
    m_pConfig->raise(diag::beginning_link);
    m_pConfig->raise(diag::initializing_standard_sections);
  }
  // Set link launch directory
  llvm::SmallString<128> linkLaunchDirectory;
  llvm::sys::fs::current_path(linkLaunchDirectory);
  m_pConfig->options().setLinkLaunchDirectory(linkLaunchDirectory.str().str());
  {
    eld::RegisterTimer F("Apply Linker default sections", "Link Summary",
                         m_pConfig->options().printTimingStats());
    if (!m_pObjLinker->initStdSections())
      return false;
  }

  // Init per-file synthetic dynamic sections.
  if (LinkerConfig::Object != m_pConfig->codeGenType()) {
    for (auto &input : m_pModule->getObjectList())
      if (ELFObjectFile *ELFObj = llvm::dyn_cast<ELFObjectFile>(input))
        m_pBackend->initDynamicSections(*ELFObj);
  }

  if (m_pModule->getPrinter()->isVerbose())
    m_pConfig->raise(diag::merging_input_sections);
  {
    eld::RegisterTimer F("Merge input sections and prepare for layout",
                         "Link Summary",
                         m_pConfig->options().printTimingStats());
    if (!resolve()) {
      // llvm::errs() << "resolve returning false!\n";
      return false;
    }
  }

  if (m_pModule->getPrinter()->isVerbose())
    m_pConfig->raise(diag::begin_layout);

  {
    PluginManager &PM = m_pModule->getPluginManager();
    if (!PM.callActBeforePerformingLayoutHook()) {
      // llvm::errs() << "callActBeforePerformingLayoutHook returning false!\n";
      return false;
    }
  }

  {
    eld::RegisterTimer F("Prepare Output file", "Link Summary",
                         m_pConfig->options().printTimingStats());
    if (!layout()) {
      // llvm::errs() << "layout returning false!\n";
      return false;
    }
  }

  if (m_pModule->getPrinter()->isVerbose())
    m_pConfig->raise(diag::emit_output_file)
        << m_pConfig->options().outputFileName();
  {
    eld::RegisterTimer F("Emit output file", "Link Summary",
                         m_pConfig->options().printTimingStats());
    if (m_pConfig->options().getInsertTimingStats()) {
      emit();
      m_TimingSectionTimer->stopTimer();
      uint64_t durationMicro =
          m_TimingSectionTimer->getTotalTime().getWallTime() * 1000000;
      m_pObjLinker->getWriter()->writeLinkTimeStats(
          *m_pModule, m_BeginningOfTime, durationMicro);
      m_TimingSectionTimer->clear();
    }
    // llvm::errs() << "emit returning false!\n";
    return emit();
  }
}

void Linker::printLayout() {
  // FIXME: In which case would the backend and object linker
  // be nullptr here? Should't these be assert instead?
  if (!m_pBackend || !m_pObjLinker) {
    m_LinkTime->stopTimer();
    m_LinkTime->clear();
    return;
  }

  // FIXME: Verbose check can be removed.
  if (m_pModule->getPrinter()->isVerbose())
    m_pConfig->raise(diag::printing_layout);

  // Stop, record and clear the timer.
  m_LinkTime->stopTimer();
  LayoutPrinter *Printer = m_pModule->getLayoutPrinter();
  if (Printer) {
    Printer->recordLinkTime(m_LinkTime->getTotalTime().getWallTime());
    Printer->recordThreadCount();
  }
  m_LinkTime->clear();

  eld::RegisterTimer F("Emit Map file", "Link Summary",
                       m_pConfig->options().printTimingStats());
  m_pObjLinker->printlayout();
}

bool Linker::activateInputs(std::vector<InputAction *> &actions) {
  m_pProgressBar->incrementAndDisplayProgress();
  for (auto &action : actions) {
    if (!action->isScript()) {
      if (!action->activate(m_pBuilder->getInputBuilder()))
        return false;
      continue;
    }
    if (!action->activate(m_pBuilder->getInputBuilder()))
      return false;
    if (!m_pObjLinker->readLinkerScript(
            llvm::dyn_cast<eld::ScriptAction>(action)->getLinkerScriptFile())) {
      m_pModule->setFailure(true);
      return false;
    }
  }
  return true;
}

bool Linker::initializeInputTree(std::vector<InputAction *> &actions) {
  {
    m_pProgressBar->incrementAndDisplayProgress();
    eld::RegisterTimer T("Input Activation", "Initialize",
                         m_pConfig->options().printTimingStats());
    m_pBackend->initializeAttributes();
  }

  {
    m_pProgressBar->incrementAndDisplayProgress();
    eld::RegisterTimer T("Create Internal Files", "Initialize",
                         m_pConfig->options().printTimingStats());
    m_pModule->createInternalInputs();
  }
  {
    m_pProgressBar->incrementAndDisplayProgress();
    eld::RegisterTimer T("Activate Inputs", "Initialize",
                         m_pConfig->options().printTimingStats());
    if (!activateInputs(actions))
      return false;
  }

  eld::RegisterTimer T("More Options", "Initialize",
                       m_pConfig->options().printTimingStats());
  m_pProgressBar->incrementAndDisplayProgress();
  m_pBackend->setOptions();

  if (!verifyLinkerScript())
    return false;

  return true;
}

/// normalize - to convert the command line language to the input tree.
bool Linker::normalize() {
  if (m_pModule->getPrinter()->traceCommandLine()) {
    m_pConfig->printOptions(llvm::outs(), *m_pBackend,
                            m_pConfig->options().color());
  }

  bool traceLTO = m_pConfig->options().traceLTO();

  // 1. - normalize the input tree
  //   read out sections and symbol/string tables (from the files) and
  //   set them in Module. When reading out the symbol, resolve their symbols
  //   immediately and set their ResolveInfo (i.e., Symbol Resolution).
  if (traceLTO)
    m_pConfig->raise(diag::note_lto_phase) << 1;

  {
    m_pProgressBar->incrementAndDisplayProgress();
    eld::RegisterTimer T("Read all Input files", "Read all Input files",
                         m_pConfig->options().printTimingStats());
    if (!m_pObjLinker->normalize())
      return false;
  }

  if (m_pModule->getPrinter()->isVerbose())
    m_pConfig->raise(diag::verbose_loading_non_universal_plugins);
  {
    m_pProgressBar->incrementAndDisplayProgress();
    eld::RegisterTimer T("Load Linker Plugins", "Plugins",
                         m_pConfig->options().printTimingStats("Plugin"));
    // Load plugins.
    if (!loadNonUniversalPlugins())
      return false;
  }

  // 2. - set up code position
  m_pProgressBar->incrementAndDisplayProgress();
  if (LinkerConfig::DynObj == m_pConfig->codeGenType() ||
      m_pConfig->options().isPIE()) {
    m_pConfig->setCodePosition(LinkerConfig::Independent);
  } else if (m_pModule->getDynLibraryList().empty()) {
    // If the output is dependent on its loaded address, and it does not need
    // to call outside functions, then we can treat the output static dependent
    // and perform better optimizations.
    m_pConfig->setCodePosition(LinkerConfig::StaticDependent);
  } else {
    m_pConfig->setCodePosition(LinkerConfig::DynamicDependent);
  }

  if ((m_pConfig->options().isPatchEnable() ||
       m_pConfig->options().getPatchBase()) &&
      !m_pConfig->isCodeStatic()) {
    m_pConfig->raise(diag::err_patch_not_static);
    return false;
  }

  setUnresolvePolicy(m_pConfig->options().reportUndefPolicy());

  {
    eld::RegisterTimer T("Parse External scripts ", "Read all Input files",
                         m_pConfig->options().printTimingStats());
    m_pProgressBar->incrementAndDisplayProgress();
    if (!m_pObjLinker->parseVersionScript())
      return false;
    if (m_pModule->getPrinter()->isVerbose())
      m_pConfig->raise(diag::parsed_version_script);
    m_pProgressBar->incrementAndDisplayProgress();
    if (m_pConfig->isCodeDynamic() || m_pConfig->options().forceDynamic()) {
      if (m_pModule->getPrinter()->isVerbose())
        m_pConfig->raise(diag::parsing_dynlist);
      m_pObjLinker->parseDynList();
    }
  }

  {
    m_pProgressBar->incrementAndDisplayProgress();
    eld::RegisterTimer T("Add Script Symbols", "Symbols from Linker Script",
                         m_pConfig->options().printTimingStats());
    // Add Linker script symbols.
    if (!m_pObjLinker->addScriptSymbols())
      return false;
    if (m_pModule->getPrinter()->isVerbose())
      m_pConfig->raise(diag::adding_script_symbols);
  }
  // LTO Specific Steps
  if (m_pModule->needLTOToBeInvoked() || m_pConfig->options().hasLTO()) {
    m_pProgressBar->addMoreTicks(3);
    m_pProgressBar->incrementAndDisplayProgress();
    eld::RegisterTimer T("Perform LTO", "LTO",
                         m_pConfig->options().printTimingStats());
    // a. Create LTO Object file from bitcode inputs
    if (!m_pObjLinker->createLTOObject())
      return false;

    if (m_pModule->getPrinter()->isVerbose())
      m_pConfig->raise(diag::beginning_post_LTO_phase);
    m_pProgressBar->incrementAndDisplayProgress();
    m_pObjLinker->beginPostLTO();
    // c. Read and resolve symbols for the new inputs discarding
    // bitcode files and including the generated LTO Object file
    if (traceLTO)
      m_pConfig->raise(diag::note_lto_phase) << 2;
    m_pProgressBar->incrementAndDisplayProgress();
    if (!m_pObjLinker->normalize())
      return false;
  }
  return true;
}

bool Linker::resolve() {
  {
    m_pProgressBar->incrementAndDisplayProgress();
    eld::RegisterTimer T("Read all relocations",
                         "Process Relocations from Input files",
                         m_pConfig->options().printTimingStats());
    m_pObjLinker->readRelocations();
  }

  if (!m_pConfig->getDiagEngine()->diagnose()) {
    if (m_pModule->getPrinter()->isVerbose())
      m_pConfig->raise(diag::function_has_error) << __PRETTY_FUNCTION__;
    return false;
  }

  {
    m_pProgressBar->incrementAndDisplayProgress();
    eld::RegisterTimer T("Allocate Common Symbols", "Common Symbols",
                         m_pConfig->options().printTimingStats());
    if (!m_pObjLinker->allocateCommonSymbols()) {
      return false;
    }
  }

  {
    m_pProgressBar->incrementAndDisplayProgress();
    eld::RegisterTimer T(
        "Assign output sections using default/linker script rules",
        "Match Default/Linker script rules",
        m_pConfig->options().printTimingStats());
    m_pProgressBar->incrementAndDisplayProgress();

    // Add dynamic section header inputs to the front of the input list.
    m_pModule->getObjectList().insert(
        m_pModule->getObjectList().begin(),
        m_pBackend->getDynamicSectionHeadersInputFile());

    // Add all internal inputs
    for (auto &obj : m_pModule->getInternalFiles()) {
      m_pModule->getObjectList().push_back(obj);
    }

    PluginManager &PM = m_pModule->getPluginManager();
    PM.callActBeforeRuleMatchingHook();

    // Assign output sections.
    m_pProgressBar->incrementAndDisplayProgress();
    m_pObjLinker->assignOutputSections(m_pModule->getObjectList());

    // Mark internal sections created by the linker and set to discard if any.
    m_pProgressBar->incrementAndDisplayProgress();
    m_pObjLinker->markDiscardFileFormatSections();

    // Targets can update any information, if they care about.
    m_pProgressBar->incrementAndDisplayProgress();
    m_pBackend->finishAssignOutputSections();
  }

  if (!m_pConfig->getDiagEngine()->diagnose()) {
    if (m_pModule->getPrinter()->isVerbose())
      m_pConfig->raise(diag::function_has_error) << __PRETTY_FUNCTION__;
    return false;
  }

  {
    eld::RegisterTimer T("Add Standard Symbols", "Add Default Standard Symbols",
                         m_pConfig->options().printTimingStats());
    m_pProgressBar->incrementAndDisplayProgress();
    // Add all symbols (Standard, Target specific symbols)
    if (!m_pObjLinker->addStandardSymbols() ||
        !m_pObjLinker->addTargetSymbols())
      return false;
  }

  {
    eld::RegisterTimer T("Target Specific Input processing",
                         "Target specific Input processing",
                         m_pConfig->options().printTimingStats());
    m_pProgressBar->incrementAndDisplayProgress();
    if (!m_pObjLinker->processInputFiles())
      return false;
  }

  // Garbage collect.
  m_pProgressBar->incrementAndDisplayProgress();
  if (LinkerConfig::Object != m_pConfig->codeGenType()) {
    m_pObjLinker->dataStrippingOpt();
  }

  recordCommonSymbols();

  // Run the section Iterator Plugin.
  {
    m_pProgressBar->incrementAndDisplayProgress();
    eld::RegisterTimer T("Run Section Iterator Plugin",
                         "Section Iterator Plugin",
                         m_pConfig->options().printTimingStats("Plugin"));
    if (!m_pObjLinker->runSectionIteratorPlugin()) {
      return false;
    }
  }

  // When linking the patch, most relocations need to resolve to the PLT stub
  // from the base image. The address of the stub is communicated as the value
  // of the `__llvm_patchable_` absolute symbol.
  if (m_pConfig->options().getPatchBase()) {
    for (auto &G : m_pModule->getNamePool().getGlobals()) {
      ResolveInfo *symInfo = G.getValue();
      // We look for an alias for EVERY symbol, not only for the patchable ones
      // because, during symbol resolution, if a patchable symbol is redefined
      // in the patch, its patchable definition from the base will be replaced
      // with the definition from the patch, which may not carry the patchable
      // attribute. This much depends on the details of the symbol resolution.
      // It may be possible to propagate the attribute during resolution.
      if (LDSymbol *PatchableAlias = m_pModule->getNamePool().findSymbol(
              std::string("__llvm_patchable_") + symInfo->name())) {
        m_pBackend->recordAbsolutePLT(symInfo, PatchableAlias->resolveInfo());
        symInfo->setReserved(symInfo->reserved() | Relocator::ReservePLT);
      }
    }
  }

  m_pProgressBar->incrementAndDisplayProgress();
  if (LinkerConfig::Object != m_pConfig->codeGenType()) {
    eld::RegisterTimer T("Scan Relocation Processing", "Relocation Processing",
                         m_pConfig->options().printTimingStats());
    bool success = m_pObjLinker->scanRelocations(false);
    if (!success)
      return false;
  }

  m_pProgressBar->incrementAndDisplayProgress();
  if (m_pConfig->isCodeDynamic() || m_pConfig->options().forceDynamic()) {
    eld::RegisterTimer T("Dynamic List Symbols", "Add Dynamic List Symbols",
                         m_pConfig->options().printTimingStats());
    if (!m_pObjLinker->addDynListSymbols())
      return false;
  }

  m_pProgressBar->incrementAndDisplayProgress();
  {
    eld::RegisterTimer T("Finalize Scan Relocation Processing",
                         "Relocation Processing",
                         m_pConfig->options().printTimingStats());
    if (!m_pObjLinker->finalizeScanRelocations())
      return false;
  }

  m_pProgressBar->incrementAndDisplayProgress();
  {
    eld::RegisterTimer T("Add Output symbols", "Output Symbols",
                         m_pConfig->options().printTimingStats());
    // Add symbols that we need to the output.
    if (!m_pObjLinker->addSymbolsToOutput())
      return false;
  }

  m_pProgressBar->incrementAndDisplayProgress();
  {
    eld::RegisterTimer T("Add Dynamic Output symbols", "Output Symbols",
                         m_pConfig->options().printTimingStats());
    // To allocate dynamic sections, we need dynamic symbols to be populated
    // properly.
    if (!m_pObjLinker->addDynamicSymbols())
      return false;
  }

  // Merge sections.
  {
    m_pProgressBar->incrementAndDisplayProgress();
    eld::RegisterTimer T("Merge Sections", "Perform Layout",
                         m_pConfig->options().printTimingStats());
    if (!m_pObjLinker->mergeSections())
      return false;
  }

  {
    m_pProgressBar->incrementAndDisplayProgress();
    eld::RegisterTimer T("Add Output Section symbols", "Output Symbols",
                         m_pConfig->options().printTimingStats());
    // Add all the section symbols
    if (!m_pObjLinker->addSectionSymbols())
      return false;
  }

  return true;
}

bool Linker::layout() {
  //  init relaxation stuff.
  {
    m_pProgressBar->incrementAndDisplayProgress();
    eld::RegisterTimer T("Initialize Stubs", "Perform Layout",
                         m_pConfig->options().printTimingStats());
    m_pObjLinker->initStubs();
  }

  {
    m_pProgressBar->incrementAndDisplayProgress();
    eld::RegisterTimer T("Do PreLayout", "Perform Layout",
                         m_pConfig->options().printTimingStats());
    m_pObjLinker->prelayout();
  }

  if (m_pModule->getPrinter()->isVerbose())
    m_pConfig->raise(diag::merging_strings);
  {
    eld::RegisterTimer F("Merge strings", "Link Summary",
                         m_pConfig->options().printTimingStats());
    m_pObjLinker->doMergeStrings();
  }

  {
    m_pProgressBar->incrementAndDisplayProgress();
    eld::RegisterTimer T("Establish Layout", "Perform Layout",
                         m_pConfig->options().printTimingStats());
    if (!m_pObjLinker->layout())
      return false;
  }

  {
    m_pProgressBar->incrementAndDisplayProgress();
    eld::RegisterTimer T("Scan Relocations", "Perform Layout",
                         m_pConfig->options().printTimingStats());
    if (LinkerConfig::Object == m_pConfig->codeGenType()) {
      if (!m_pObjLinker->scanRelocations(true))
        return false;
    }
  }

  {
    m_pProgressBar->incrementAndDisplayProgress();
    eld::RegisterTimer T("Create Output Section Table", "Perform Layout",
                         m_pConfig->options().printTimingStats());
    if (!m_pObjLinker->postlayout())
      return false;
  }

  {
    m_pProgressBar->incrementAndDisplayProgress();
    eld::RegisterTimer T("Finalize Target specific symbol Values",
                         "Perform Layout",
                         m_pConfig->options().printTimingStats());
    m_pBackend->finalizeSymbols();
  }

  {
    m_pProgressBar->incrementAndDisplayProgress();
    eld::RegisterTimer T("AfterLayout OutputSection Iterator", "Perform Layout",
                         m_pConfig->options().printTimingStats("plugin"));
    // Run the output section iterator plugin after all the layout is done.
    m_pModule->setState(plugin::LinkerWrapper::AfterLayout);
    m_pBackend->finalizeLayout();

    {
      if (!m_pObjLinker->runOutputSectionIteratorPlugin()) {
        m_pModule->setFailure(true);
        return false;
      }
    }
  }

  {
    m_pProgressBar->incrementAndDisplayProgress();
    eld::RegisterTimer T("Apply Relocation", "Perform Layout",
                         m_pConfig->options().printTimingStats());
    m_pObjLinker->relocation(m_pConfig->options().emitRelocs());
  }

  {
    m_pProgressBar->incrementAndDisplayProgress();
    eld::RegisterTimer T("Finalize Input Symbol Values", "Perform Layout",
                         m_pConfig->options().printTimingStats());
    m_pObjLinker->finalizeSymbolValues();
  }

  {
    m_pProgressBar->incrementAndDisplayProgress();
    eld::RegisterTimer T("Finalize Output File", "Perform Layout",
                         m_pConfig->options().printTimingStats());
    m_pObjLinker->finalizeBeforeWrite();
  }

  if (!m_pConfig->getDiagEngine()->diagnose()) {
    if (m_pModule->getPrinter()->isVerbose())
      m_pConfig->raise(diag::function_has_error) << __PRETTY_FUNCTION__;
    return false;
  }
  return true;
}

bool Linker::emit() {
  eld::RegisterTimer T("Emit Output File", "Emit Output File",
                       m_pConfig->options().printTimingStats());
  std::string pPath = m_pConfig->options().outputFileName();

  int perm = 0;
  switch (m_pConfig->codeGenType()) {
  case eld::LinkerConfig::Unknown:
  case eld::LinkerConfig::Object:
    perm = 0x644;
    break;
  case eld::LinkerConfig::DynObj:
  case eld::LinkerConfig::Exec:
  case eld::LinkerConfig::Binary:
    perm = 0x755;
    break;
  default:
    assert(0 && "Unknown file type");
  }

  m_pProgressBar->incrementAndDisplayProgress();
  size_t outputFileSize = 0;
  if (m_pConfig->targets().is32Bits())
    outputFileSize =
        m_pObjLinker->getWriter()->getOutputSize<llvm::object::ELF32LE>(
            *m_pModule);
  else if (m_pConfig->targets().is64Bits())
    outputFileSize =
        m_pObjLinker->getWriter()->getOutputSize<llvm::object::ELF64LE>(
            *m_pModule);
  uint64_t maxImageSize =
      (m_pConfig->targets().is32Bits() ? UINT32_MAX : INT64_MAX);
  if (outputFileSize > maxImageSize) {
    m_pConfig->raise(diag::err_output_size_too_large) << outputFileSize;
    return false;
  }
  LayoutPrinter *printer = m_pModule->getLayoutPrinter();
  if (printer)
    printer->recordOutputFileSize(outputFileSize);

  {
    std::error_code ec;
    int outputFlag = 0;
    if (perm & 0x755)
      outputFlag = llvm::FileOutputBuffer::F_executable;
    auto outputOrError =
        llvm::FileOutputBuffer::create(pPath, outputFileSize, outputFlag);
    // If there is an error, return with a fatal error message.
    if (!outputOrError) {
      m_pConfig->raise(diag::fatal_unwritable_output)
          << pPath << llvm::toString(outputOrError.takeError());
      return false;
    }
    {
      m_pProgressBar->incrementAndDisplayProgress();
      eld::RegisterTimer T("Write All Sections", "Emit Output File",
                           m_pConfig->options().printTimingStats());
      m_pObjLinker->emitOutput(*outputOrError.get());
    }

    m_pProgressBar->incrementAndDisplayProgress();
    eld::Expected<void> expPostProcess =
        m_pObjLinker->postProcessing(*outputOrError.get());
    if (!expPostProcess) {
      m_pConfig->raiseDiagEntry(std::move(expPostProcess.error()));
      return false;
    }

    // Update Build ID and sync
    eld::Expected<void> E =
        m_pBackend->finalizeAndEmitBuildID(*outputOrError.get());
    if (!E) {
      m_pConfig->raiseDiagEntry(std::move(E.error()));
      return false;
    }

    {
      m_pProgressBar->incrementAndDisplayProgress();
      eld::RegisterTimer T("Commit File", "Emit Output File",
                           m_pConfig->options().printTimingStats());
      if (auto E = (*outputOrError)->commit()) {
        m_pConfig->raise(diag::unable_to_write_output_file)
            << pPath << llvm::toString(std::move(E));
        return false;
      }
    }
  }

  m_pProgressBar->incrementAndDisplayProgress();
  if ((pPath != "/dev/null") && (m_pConfig->options().verifyLink())) {
    llvm::sys::fs::file_status file_status;
    std::error_code ec = llvm::sys::fs::status(pPath, file_status);
    if (ec != std::error_code()) {
      m_pConfig->raise(diag::output_file_error) << pPath << ec.message();
      return false;
    }
    if (file_status.getSize() != outputFileSize) {
      std::stringstream s1;
      std::stringstream s2;
      s1 << outputFileSize;
      s2 << file_status.getSize();
      m_pConfig->raise(diag::output_file_not_complete)
          << pPath << s1.str() << s2.str();
      return false;
    }
  }
  if (!m_pConfig->getDiagEngine()->diagnose()) {
    if (m_pModule->getPrinter()->isVerbose())
      m_pConfig->raise(diag::function_has_error) << __PRETTY_FUNCTION__;
    return false;
  }
  return true;
}

bool Linker::reset() {
  m_pBackend = nullptr;
  // initEmulator does not create the ObjectLinker
  if (m_pObjLinker) {
    m_pObjLinker->close();
    m_pObjLinker = nullptr;
  }
  delete m_pProgressBar;
  return true;
}

bool Linker::initBackend(const eld::Target *pTarget) {
  if (m_pModule->getPrinter()->isVerbose())
    m_pConfig->raise(diag::initializing_backend);
  eld::RegisterTimer T("Initialize Backend", "Initialize",
                       m_pConfig->options().printTimingStats());
  m_pBackend = pTarget->createLDBackend(*m_pModule);
  m_pProgressBar->incrementAndDisplayProgress();
  bool hasError = false;
  if (nullptr == m_pBackend || !pTarget->IsImplemented) {
    std::string AvailableTargets;
    for (auto target = TargetRegistry::begin(); target != TargetRegistry::end();
         target++) {
      if ((*target)->IsImplemented)
        AvailableTargets += (*target)->name() + std::string(" ");
    }
    m_pConfig->raise(diag::err_cannot_init_backend)
        << m_pConfig->targets().triple().str() << AvailableTargets;
    hasError = true;
  }
  if ((m_pBackend && m_pConfig->options().validateArchOptions() &&
       !m_pBackend->validateArchOpts()) ||
      hasError)
    return false;

  if (m_pModule->getPrinter()->isVerbose())
    m_pConfig->raise(diag::initializing_object_linker);
  m_pObjLinker = make<ObjectLinker>(*m_pConfig, *m_pBackend);
  m_pObjLinker->initialize(*m_pModule, *m_pBuilder);

  m_pBackend->setDefaultConfigs();
  return true;
}

bool Linker::initEmulator(LinkerScript &pScript, const eld::Target *pTarget) {
  eld::RegisterTimer T("Initialize Emulator", "Initialize",
                       m_pConfig->options().printTimingStats());
  if (m_pModule->getPrinter()->isVerbose())
    m_pConfig->raise(diag::initializing_emulator);
  m_pProgressBar->incrementAndDisplayProgress();
  return pTarget->emulate(pScript, *m_pConfig);
}

bool Linker::verifyLinkerScript() {
  LinkerScript &pScript = m_pModule->getScript();
  if (pScript.phdrsSpecified() && !pScript.linkerScriptHasSectionsCommand()) {
    m_pConfig->raise(diag::linker_script_uses_phdrs_no_sections);
    m_pModule->setFailure(true);
    return false;
  }
  return true;
}

bool Linker::loadNonUniversalPlugins() {
  ASSERT(m_pModule, "Module must not be null!");
  if (m_pModule->getPrinter()->isVerbose())
    m_pConfig->raise(diag::verbose_loading_non_universal_plugins);

  if (!m_pModule->getScript().loadNonUniversalPlugins(*m_pModule)) {
    m_pModule->setFailure(true);
    return false;
  }
  return true;
}

void Linker::unloadPlugins() {
  if (m_pModule->getPrinter()->isVerbose())
    m_pConfig->raise(diag::unloading_plugins);
  m_pModule->getScript().unloadPlugins(m_pModule);
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
      m_pConfig->isBuildingExecutable())
    UnresolvedSymbolPolicy = UnresolvedPolicy::ReportAll;
}

void Linker::recordCommonSymbol(const ResolveInfo *R) {
  LayoutPrinter *printer = m_pModule->getLayoutPrinter();
  if (!printer)
    return;
  LDSymbol *sym = R->outSymbol();
  Fragment *F = sym->fragRef()->frag();
  ASSERT(F, "All non-bitcode common symbols should have a corresponding "
            "fragment!");
  ELFSection *S = F->getOwningSection();
  printer->recordFragment(sym->resolveInfo()->resolvedOrigin(), S, F);
  printer->recordSymbol(F, sym);
}

void Linker::recordCommonSymbols() {
  if (!m_pModule->getLayoutPrinter())
    return;
  // Common symbols are only allocated for partial links if define common
  // option is specified.
  if (m_pConfig->isLinkPartial() && !m_pConfig->options().isDefineCommon())
    return;
  for (const auto &G : m_pModule->getNamePool().getGlobals()) {
    ResolveInfo *R = G.getValue();
    // Skip bitcode common symbols.
    if (!R->isCommon() || R->isBitCode())
      continue;
    recordCommonSymbol(R);
  }
}

void Linker::reportUnknownOptions() const {
  const auto &options = m_pConfig->options();
  if (options.shouldIgnoreUnknownOptions())
    return;
  const auto &unknownOptions = options.getUnknownOptions();
  for (const auto &option : unknownOptions)
    m_pConfig->raise(diag::warn_unsupported_option) << option;
}
