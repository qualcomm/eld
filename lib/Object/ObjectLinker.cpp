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
#include "eld/GarbageCollection/GarbageCollection.h"
#include "eld/Input/ArchiveMemberInput.h"
#include "eld/Input/BitcodeFile.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/Input/InputTree.h"
#include "eld/Input/InternalInputFile.h"
#include "eld/Input/LinkerScriptFile.h"
#include "eld/Input/ObjectFile.h"
#include "eld/LayoutMap/LayoutPrinter.h"
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
static DiagnosticEngine *sDiagEngineForLTO = nullptr;
class PrepareDiagEngineForLTO {
public:
  PrepareDiagEngineForLTO(DiagnosticEngine *diagEngine) {
    m_Mutex.lock();
    sDiagEngineForLTO = diagEngine;
  }
  ~PrepareDiagEngineForLTO() {
    sDiagEngineForLTO = nullptr;
    m_Mutex.unlock();
  }

private:
  std::mutex m_Mutex;
};

codegen::RegisterCodeGenFlags CGF;

} // namespace

//===----------------------------------------------------------------------===//
// ObjectLinker
//===----------------------------------------------------------------------===//
ObjectLinker::ObjectLinker(LinkerConfig &pConfig, GNULDBackend &pLDBackend)
    : m_Config(pConfig), m_pModule(nullptr), m_pBuilder(nullptr),
      m_LDBackend(pLDBackend), m_pGroupReader(nullptr),
      m_pScriptReader(nullptr), m_pWriter(nullptr), m_pBitcodeReader(nullptr),
      m_pSymDefReader(nullptr), m_postLTOPhase(false), m_SaveTemps(false),
      m_TraceLTO(false) {
  m_SaveTemps = m_Config.options().getSaveTemps();
  m_TraceLTO = m_Config.options().traceLTO();
  m_LTOTempPrefix = getLTOTempPrefix();
}

void ObjectLinker::close() {
  // Cleanup all the temporary files created as a result of LTO.
  if (!m_SaveTemps) {
    for (auto &p : filesToRemove) {
      if (!p.empty()) {
        if (m_TraceLTO || m_Config.getPrinter()->isVerbose())
          m_Config.raise(diag::lto_deleting_temp_file) << p;
        llvm::sys::fs::remove(p);
      }
    }
  }
}

bool ObjectLinker::initialize(eld::Module &pModule, eld::IRBuilder &pBuilder) {
  m_pModule = &pModule;
  m_pBuilder = &pBuilder;

  m_pBitcodeReader = m_LDBackend.createBitcodeReader();
  // initialize the readers and writers
  m_NewRelocObjParser = m_LDBackend.createNewRelocObjParser();
  m_NewDynObjReader = m_LDBackend.createNewDynObjReader();
  m_ArchiveParser = m_LDBackend.createArchiveParser();
  m_ELFExecObjParser = m_LDBackend.createELFExecObjParser();
  m_BinaryFileParser = m_LDBackend.createBinaryFileParser();
  // SymDef Reader.
  m_pSymDefReader = m_LDBackend.createSymDefReader();
  m_pGroupReader = make<GroupReader>(*m_pModule, this);
  m_pScriptReader = make<ScriptReader>();
  m_pWriter = m_LDBackend.createWriter();
  // initialize Relocator
  m_LDBackend.initRelocator();

  return true;
}

/// initStdSections - initialize standard sections
bool ObjectLinker::initStdSections() {
  ObjectBuilder builder(m_Config, *m_pModule);

  // initialize standard sections
  eld::Expected<void> E = m_LDBackend.initStdSections();
  if (!E) {
    m_Config.raiseDiagEntry(std::move(E.error()));
    return false;
  }

  // initialize dynamic sections
  if (LinkerConfig::Object != m_Config.codeGenType()) {
    m_LDBackend.initDynamicSections(
        *m_LDBackend.getDynamicSectionHeadersInputFile());

    // Note that patch section are only created in one internal input file
    // (DynamicSectionHeadersInputFile).
    if (m_Config.options().isPatchEnable())
      m_LDBackend.initPatchSections(
          *m_LDBackend.getDynamicSectionHeadersInputFile());
  }

  // initialize target-dependent sections
  m_LDBackend.initTargetSections(builder);

  return true;
}

// Read Linker script Helper.
bool ObjectLinker::readLinkerScript(InputFile *input) {

  LinkerScriptFile *LSFile = llvm::dyn_cast<eld::LinkerScriptFile>(input);

  if (LSFile->isParsed())
    return true;

  // Record the linker script in the Map file.
  LayoutPrinter *printer = m_pModule->getLayoutPrinter();
  if (printer)
    printer->recordLinkerScript(input->getInput()->getFileName());

  m_pModule->getScript().addToHash(input->getInput()->decoratedPath());

  ScriptFile script(ScriptFile::LDScript, *m_pModule, *LSFile,
                    m_pBuilder->getInputBuilder(), getTargetBackend());
  bool successFullInParse = getScriptReader()->readScript(m_Config, script);
  if (printer)
    printer->closeLinkerScript();

  // Error if the linker script has an issue parsing.
  if (!successFullInParse) {
    m_Config.raise(diag::file_has_error)
        << input->getInput()->getResolvedPath();
    return false;
  }
  LSFile->setParsed();
  // Update the caller with information if the linker script had sections et
  // all.
  if (script.linkerScriptHasSectionsCommand())
    m_pModule->getScript().setHasSectionsCmd();

  // Activate the Linker script.
  eld::Expected<void> E = script.activate(*m_pModule);
  if (!E) {
    m_Config.raiseDiagEntry(std::move(E.error()));
    if (!m_Config.getDiagEngine()->diagnose())
      return false;
  }

  input->setUsed(true);

  // Error if the linker script had semantic errors .
  if (m_LDBackend.checkForLinkerScriptErrors()) {
    m_Config.raise(diag::file_has_error)
        << input->getInput()->getResolvedPath();
    return false;
  }
  return true;
}

bool ObjectLinker::readInputs(const std::vector<Node *> &inputVector) {
  typedef std::vector<Node *>::const_iterator Iter;

  for (Iter begin = inputVector.begin(), end = inputVector.end(); begin != end;
       ++begin) {
    // is a group node
    if ((*begin)->kind() == Node::GroupStart) {
      eld::RegisterTimer T("Read Start Group and End Group",
                           "Read all Input files",
                           m_Config.options().printTimingStats());
      getGroupReader()->readGroup(begin, m_pBuilder->getInputBuilder(),
                                  m_Config, m_postLTOPhase);
      continue;
    }

    FileNode *node = llvm::dyn_cast<FileNode>(*begin);

    if (!node)
      continue;

    Input *input = node->getInput();
    // Resolve the path.
    if (!input->resolvePath(m_Config)) {
      m_pModule->setFailure(true);
      return false;
    }
    if (input->isAlreadyReleased())
      continue;
    if (!readAndProcessInput(input, m_postLTOPhase))
      return false;
    if (input->getInputFile()->getKind() == InputFile::GNULinkerScriptKind) {
      // Read inputs that the script contains.
      if (!readInputs(
              llvm::dyn_cast<eld::LinkerScriptFile>(input->getInputFile())
                  ->getNodes())) {
        return false;
      }
    }
  } // end of for
  return true;
}

bool ObjectLinker::normalize() {
  if (m_postLTOPhase) {
    if (!insertPostLTOELF())
      return false;
  } else {
    parseIncludeOrExcludeLTOfiles();
    m_pModule->getNamePool().setupNullSymbol();
    addUndefSymbols();
    addDuplicateCodeInsteadOfTrampolines();
    addNoReuseOfTrampolines();
  }

  // Read all the inputs
  auto readSuccess = readInputs(m_pBuilder->getInputBuilder().getInputs());
  if (!readSuccess) {
    return false;
  }

  // Create patch base input.
  if (const auto &PatchBase = m_Config.options().getPatchBase()) {
    Input *input = make<Input>(*PatchBase, m_Config.getDiagEngine());
    // Resolve the path.
    if (!input->resolvePath(m_Config)) {
      m_pModule->setFailure(true);
      return false;
    }
    input->getAttribute().setPatchBase();
    if (!readAndProcessInput(input, m_postLTOPhase))
      return false;
  }

  m_LDBackend.addSymbols();

  // Initialize Default section mappings.
  if (!m_pModule->getScript().linkerScriptHasSectionsCommand())
    m_LDBackend.getInfo().InitializeDefaultMappings(*m_pModule);
  return true;
}

bool ObjectLinker::parseVersionScript() {
  if (!m_Config.options().hasVersionScript())
    return true;
  LayoutPrinter *printer = m_pModule->getLayoutPrinter();
  for (const auto &list : m_Config.options().getVersionScripts()) {
    Input *versionScriptInput =
        eld::make<Input>(list, m_Config.getDiagEngine(), Input::Script);
    if (!versionScriptInput->resolvePath(m_Config))
      return false;
    // Create an Input file and set the input file to be of kind DynamicList
    InputFile *versionScriptInputFile =
        InputFile::Create(versionScriptInput, InputFile::GNULinkerScriptKind,
                          m_Config.getDiagEngine());
    addInputFileToTar(versionScriptInputFile, eld::MappingFile::VersionScript);
    versionScriptInput->setInputFile(versionScriptInputFile);
    // Record the dynamic list script in the Map file.
    if (printer)
      printer->recordVersionScript(list);
    // Read the dynamic List file
    ScriptFile versionScriptReader(
        ScriptFile::VersionScript, *m_pModule,
        *(llvm::dyn_cast<eld::LinkerScriptFile>(versionScriptInputFile)),
        m_pBuilder->getInputBuilder(), getTargetBackend());
    bool successFullInParse =
        getScriptReader()->readScript(m_Config, versionScriptReader);
    if (!successFullInParse)
      return false;
    m_pModule->addVersionScript(versionScriptReader.getVersionScript());
    for (auto &versionScriptNode :
         versionScriptReader.getVersionScript()->getNodes()) {
      if (!versionScriptNode->isAnonymous()) {
        m_Config.raise(diag::unsupported_version_node)
            << versionScriptInput->decoratedPath();
        continue;
      }
      if (versionScriptNode->hasDependency()) {
        m_Config.raise(diag::unsupported_dependent_node)
            << versionScriptInput->decoratedPath();
        continue;
      }
      if (versionScriptNode->hasError()) {
        m_Config.raise(diag::error_parsing_version_script)
            << versionScriptInput->decoratedPath();
        return false;
      }
      m_pModule->addVersionScriptNode(versionScriptNode);
    }
  }
  auto &SymbolScopes = m_LDBackend.symbolScopes();
  for (auto &G : m_pModule->getNamePool().getGlobals()) {
    ResolveInfo *R = G.getValue();
    for (auto &versionScriptNode : m_pModule->getVersionScriptNodes()) {
      if (versionScriptNode->getGlobalBlock()) {
        for (auto Sym : versionScriptNode->getGlobalBlock()->getSymbols()) {
          if (Sym->getSymbolPattern()->matched(*R)) {
            m_LDBackend.addSymbolScope(R, Sym);
          } // end Symbol Match
        } // end Symbols
      } // end Global
      if (SymbolScopes.find(R) != SymbolScopes.end())
        continue;
      if (versionScriptNode->getLocalBlock()) {
        for (auto Sym : versionScriptNode->getLocalBlock()->getSymbols()) {
          if (Sym->getSymbolPattern()->matched(*R)) {
            m_LDBackend.addSymbolScope(R, Sym);
          } // end Symbol Match
        } // end Symbols
      } // end Local
    } // end all Nodes
  } // end Globals
  return true;
}

std::string ObjectLinker::getLTOTempPrefix() const {
  std::string OutFileName = m_Config.options().outputFileName();
  auto &saveTempsDir = m_Config.options().getSaveTempsDir();
  if (saveTempsDir) {
    llvm::SmallString<256> Path(*saveTempsDir);
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
  if (m_SaveTemps) {
    FileName =
        (Twine(m_LTOTempPrefix) + Twine(Task) + Twine(".") + Twine(Suffix))
            .str();
    EC = llvm::sys::fs::openFileForWrite(FileName, FD);
    ErrMsg = std::string(FileName);
  } else {
    EC = llvm::sys::fs::createTemporaryFile("llvm-lto", Suffix, FD, FileName);
    ErrMsg = "Could not create unique LTO object file";
  }
  if (EC) {
    ErrMsg += ": " + EC.message();
    m_Config.raise(diag::fatal_no_codegen_compile) << ErrMsg;
    return make_error<StringError>(EC);
  }

  return std::make_unique<llvm::raw_fd_ostream>(FD, true);
}

void ObjectLinker::addInputFileToTar(const std::string &name,
                                     eld::MappingFile::Kind K) {
  auto outputTar = m_pModule->getOutputTarWriter();
  if (!outputTar || !llvm::sys::fs::exists(name))
    return;
  outputTar->createAndAddFile(name, name, K, /*isLTO*/ false);
}

void ObjectLinker::parseDynList() {
  if (!m_Config.options().hasDynamicList())
    return;

  GeneralOptions::const_dyn_list_iterator it =
      m_Config.options().dyn_list_begin();
  GeneralOptions::const_dyn_list_iterator ie =
      m_Config.options().dyn_list_end();
  for (; it != ie; it++) {
    if (!parseListFile(*it, ScriptFile::DynamicList))
      return;
  }
}

void ObjectLinker::dataStrippingOpt() {
  // Garbege collection
  if (m_pModule->getIRBuilder()->shouldRunGarbageCollection())
    runGarbageCollection("GC");
}

void ObjectLinker::runGarbageCollection(const std::string &Phase,
                                        bool CommonSectionsOnly) {
  eld::RegisterTimer T("Perform Garbage collection", "Garbage Collection",
                       m_Config.options().printTimingStats());
  GarbageCollection GC(m_Config, m_LDBackend, *m_pModule);
  GC.run(Phase, CommonSectionsOnly);
  m_GCHasRun = true;
}

bool ObjectLinker::getInputs(std::vector<InputFile *> &inputs) {
  eld::Module::obj_iterator obj, objEnd = m_pModule->obj_end();
  for (obj = m_pModule->obj_begin(); obj != objEnd; ++obj)
    inputs.push_back(*obj);
  return true;
}

///
/// readRelocations - read all relocation entries
///
bool ObjectLinker::readRelocations() {
  std::vector<InputFile *> inputs;
  getInputs(inputs);
  for (auto ai : inputs) {
    if (ai->getInput()->getAttribute().isPatchBase()) {
      if (auto *ELFFile = llvm::dyn_cast<ELFFileBase>(ai)) {
        eld::Expected<bool> exp =
            getELFExecObjParser()->parsePatchBase(*ELFFile);
        if (!exp.has_value())
          m_Config.raiseDiagEntry(std::move(exp.error()));
        if (!exp.has_value() || !exp.value())
          return false;
      }
    }
    if (!ai->isObjectFile())
      continue;
    // Dont read relocations from inputs that are specified
    // with just symbols attribute
    if (ai->getInput()->getAttribute().isJustSymbols())
      continue;
    ObjectFile *Obj = llvm::dyn_cast<ObjectFile>(ai);
    if (Obj->isInputRelocsRead())
      continue;
    if (m_pModule->getPrinter()->isVerbose())
      m_Config.raise(diag::reading_relocs) << Obj->getInput()->decoratedPath();
    Obj->setInputRelocsRead();
    eld::Expected<bool> expReadRelocs =
        getNewRelocObjParser()->readRelocations(*ai);
    if (!expReadRelocs.has_value())
      m_Config.raiseDiagEntry(std::move(expReadRelocs.error()));
    if (!expReadRelocs.has_value() || !expReadRelocs.value())
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
  ObjectBuilder Builder(m_Config, *m_pModule);
  /// We can multithread across output sections as there is no shared state
  /// between them wrt string merging. When global string merging is enabled,
  /// strings would need to be placed in one Module, so threads should
  /// not be used.
  bool UseThreads = m_Config.options().numThreads() > 1;
  bool GlobalMerge = m_Config.options().shouldGlobalStringMerge();
  llvm::ThreadPoolInterface *Pool = m_pModule->getThreadPool();
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
  for (ELFSection *S : *m_pModule) {
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
  for (OutputSectionEntry *O : m_pModule->getScript().sectionMap()) {
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
  for (InputFile *I : m_pModule->getObjectList()) {
    if (I->isInternal())
      continue;
    ELFObjectFile *Obj = llvm::dyn_cast<ELFObjectFile>(I);
    if (!Obj)
      continue;
    for (ELFSection *S : Obj->getRelocationSections()) {
      if (m_pModule->getPrinter()->isVerbose() ||
          m_pModule->getPrinter()->traceMergeStrings())
        m_Config.raise(diag::handling_merge_strings_for_section)
            << S->getDecoratedName(m_Config.options())
            << S->getInputFile()->getInput()->decoratedPath(true);
      m_LDBackend.getRelocator()->doMergeStrings(S);
    }
  }
}

void ObjectLinker::doMergeStrings() {
  if (m_Config.isLinkPartial())
    return;
  mergeIdenticalStrings();
  fixMergeStringRelocations();
}

void ObjectLinker::assignOutputSections(std::vector<eld::InputFile *> &inputs) {
  ObjectBuilder builder(m_Config, *m_pModule);
  auto start = std::chrono::steady_clock::now();
  builder.assignOutputSections(inputs, m_postLTOPhase);
  // FIXME: Perhaps transfer entry section ownership to GarbageCollection as
  // Entry sections are only relevant with garbage collection.
  // Currently, entry section are computed even if garbage-collection is not
  // enabled.
  collectEntrySections();
  LayoutPrinter *layoutPrinter = m_pModule->getLayoutPrinter();
  if (layoutPrinter && layoutPrinter->LayoutPrinter::showInitialLayout()) {
    TextLayoutPrinter *textMapPrinter = m_pModule->getTextMapPrinter();
    if (textMapPrinter) {
      // FIXME: ideally, we should not need 'updateMatchedSections' call here.
      // However, we need it because currently we do not maintain the list of
      // matched input sections for rule containers consistently.
      RuleContainer::updateMatchedSections(*m_pModule);
      textMapPrinter->printLayout(*m_pModule);
    }
  }
  // FIXME: SectionMatcher plugins should not consume time under
  // 'LinkerScriptRuleMatch' timing category!
  if (!builder.InitializePluginsAndProcess(inputs,
                                           plugin::Plugin::SectionMatcher))
    return;
  auto end = std::chrono::steady_clock::now();
  if (m_pModule->getPrinter()->allStats())
    m_Config.raise(diag::linker_script_rule_matching_time)
        << (int)std::chrono::duration<double, std::milli>(end - start).count();
}

// Sections in ELFFileFormat are not internal but are Output sections.
// At the moment, there is  no way being marked with assignOutputSections
// We traverse them explicitly to mark them as ignore before placing them.
void ObjectLinker::markDiscardFileFormatSections() {
  auto &sectionMap = m_pModule->getScript().sectionMap();
  SectionMap::iterator it = sectionMap.findIter("/DISCARD/");
  bool isGNUCompatible =
      (m_Config.options().getScriptOption() == GeneralOptions::MatchGNU);
  for (auto &sec : m_LDBackend.getOutputFormat()->getSections()) {
    SectionMap::mapping pair =
        sectionMap.findIn(it, "internal", *sec, false, "internal",
                          sec->sectionNameHash(), 0, 0, isGNUCompatible);
    if (pair.first && pair.first->isDiscard()) {
      sec->setKind(LDFileFormat::Ignore);
      if (m_Config.options().isSectionTracingRequested() &&
          m_Config.options().traceSection(pair.first->name().str())) {
        m_Config.raise(diag::discarded_section_info)
            << pair.first->getSection()->getDecoratedName(m_Config.options());
      }
    }
  }
}

bool ObjectLinker::mayBeSortSections(std::vector<Section *> &sections) {
  // If no linker scripts, we dont store the original input. Lets not sort.
  if (!m_pModule->getScript().linkerScriptHasSectionsCommand())
    return true;
  if (m_Config.options().disableLTOLinkOrder())
    return true;
  // If we are doing partial link, lets not sort it.
  bool isPartialLink = (LinkerConfig::Object == m_Config.codeGenType());
  if (isPartialLink || ltoObjects.empty())
    return true;
  std::stable_sort(
      sections.begin(), sections.end(), [](Section *A, Section *B) {
        ELFSection *a = llvm::dyn_cast<ELFSection>(A);
        ELFSection *b = llvm::dyn_cast<ELFSection>(B);
        if (a == nullptr or b == nullptr)
          return false;
        if (!a->originalInput())
          return false;
        if (!b->originalInput())
          return false;
        if ((a->name().starts_with(".ctors")) ||
            (b->name().starts_with(".ctors")))
          return false;
        if ((a->name().starts_with(".dtors")) ||
            (b->name().starts_with(".dtors")))
          return false;
        int64_t aOrdinal = a->originalInput()->getInput()->getInputOrdinal();
        int64_t bOrdinal = b->originalInput()->getInput()->getInputOrdinal();
        if (aOrdinal == bOrdinal)
          return false;
        return (aOrdinal < bOrdinal);
      });
  return true;
}

bool ObjectLinker::mergeInputSections(ObjectBuilder &builder,
                                      std::vector<Section *> &sections) {
  bool isPartialLink = m_Config.isLinkPartial();
  for (auto &section : sections) {
    if (section->isBitcode())
      continue;
    ELFSection *sect = llvm::dyn_cast<ELFSection>(section);
    bool hasSectionData = sect->hasSectionData();
    if (sect->isIgnore() || sect->isDiscard())
      continue;
    switch (sect->getKind()) {
    // Some *INPUT sections should not be merged.
    case LDFileFormat::Null:
    case LDFileFormat::NamePool:
    case LDFileFormat::Discard:
      // skip
      continue;
    case LDFileFormat::Relocation:
    case LDFileFormat::LinkOnce: {
      if (sect->getLink()->isIgnore() || sect->getLink()->isDiscard())
        sect->setKind(LDFileFormat::Ignore);
      break;
    }
    case LDFileFormat::Target:
      if (m_LDBackend.DoesOverrideMerge(sect)) {
        m_LDBackend.mergeSection(sect);
        break;
      }
      LLVM_FALLTHROUGH;
    case LDFileFormat::EhFrame: {
      if (sect->getKind() == LDFileFormat::EhFrame) {
        if (!llvm::dyn_cast<eld::EhFrameSection>(sect)->splitEhFrameSection())
          return false;
        if (!llvm::dyn_cast<eld::EhFrameSection>(sect)
                 ->createCIEAndFDEFragments())
          return false;
        llvm::dyn_cast<eld::EhFrameSection>(sect)->finishAddingFragments();
        if (m_LDBackend.getEhFrameHdr() &&
            sect->getKind() == LDFileFormat::EhFrame) {
          m_LDBackend.getEhFrameHdr()->addCIE(
              llvm::dyn_cast<eld::EhFrameSection>(sect)->getCIEs());
          // Since we found an EhFrame section, lets go ahead and start creating
          // the fragments necessary to create the .eh_frame_hdr section and
          // the filler eh_frame section.
          m_LDBackend.createEhFrameFillerAndHdrSection();
        }
      }
    }
      LLVM_FALLTHROUGH;
    default: {
      if (!(hasSectionData || (isPartialLink && sect->isWanted())))
        continue; // skip

      ELFSection *out_sect = nullptr;
      if (nullptr != (out_sect = builder.MergeSection(m_LDBackend, sect))) {
        builder.updateSectionFlags(out_sect, sect);
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
                       m_Config.options().printTimingStats());
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

    WildcardPattern &pattern =
        llvm::cast<WildcardPattern>(*I->spec().sections().front());
    P = pattern.sortPolicy();
    if (P == WildcardPattern::SortPolicy::SORT_NONE &&
        m_Config.options().isSortSectionEnabled()) {
      if (m_Config.options().isSortSectionByName())
        P = WildcardPattern::SORT_BY_NAME;
      else if (m_Config.options().isSortSectionByAlignment())
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

bool ObjectLinker::createOutputSection(ObjectBuilder &builder,
                                       OutputSectionEntry *output,
                                       bool postLayout) {
  uint64_t out_align = 0x0, in_align = 0x0;
  bool isPartialLink = (LinkerConfig::Object == m_Config.codeGenType());

  ELFSection *out_sect = output->getSection();
  out_sect->setOutputSection(output);
  if (out_sect->getKind() != LDFileFormat::NamePool)
    out_sect->setAddrAlign(0);
  OutputSectionEntry::iterator in, inBegin, inEnd;
  inBegin = output->begin();
  inEnd = output->end();
  bool hasSubAlign = false;

  // force input alignment from ldscript if any
  if (output->prolog().hasSubAlign()) {
    output->prolog().subAlign().eval();
    output->prolog().subAlign().commit();
    in_align = output->prolog().subAlign().result();
    hasSubAlign = true;
  }

  // force output alignment from ldscript if any
  if (output->prolog().hasAlign()) {
    output->prolog().align().eval();
    output->prolog().align().commit();
    out_align = output->prolog().align().result();
    if (out_align && !isPowerOf2_64(out_align)) {
      m_Config.raise(diag::error_non_power_of_2_value_to_align_output_section)
          << output->prolog().align().getContext() << utility::toHex(out_align)
          << out_sect->name();
      return false;
    }
    if (out_sect->getAddrAlign() < out_align)
      out_sect->setAddrAlign(out_align);
  }

  /// FIXME: this vector is unused?
  std::vector<RuleContainer *> inputsWithNoData;
  RuleContainer *FirstNonEmptyRule = nullptr;

  for (in = inBegin; in != inEnd; ++in) {
    ELFSection *in_sect = (*in)->getSection();
    bool isDirty = (*in)->isDirty();
    if (isDirty) {
      in_sect->setType(0);
      in_sect->setFlags(0);
    }
    if (in_sect->isWanted())
      out_sect->setWanted(true);
    // Recalculate alignment if the input rule is dirty.
    if (isDirty) {
      size_t Alignment = 1;
      for (auto &F : in_sect->getFragmentList()) {
        if (Alignment < F->alignment())
          Alignment = F->alignment();
        F->getOwningSection()->setMatchedLinkerScriptRule(*in);
        F->getOwningSection()->setOutputSection(output);
        builder.mayChangeSectionTypeOrKind(in_sect, F->getOwningSection());
        builder.updateSectionFlags(in_sect, F->getOwningSection());
      }
      in_sect->setAddrAlign(Alignment);
    }
    if (hasSubAlign && (in_sect->getAddrAlign() < in_align)) {
      if (in_sect->getFragmentList().size()) {
        in_sect->getFragmentList().front()->setAlignment(in_align);
        in_sect->setAddrAlign(in_align);
      }
    }
    if (in_sect->getFragmentList().size() && !FirstNonEmptyRule)
      FirstNonEmptyRule = *in;

    if (builder.MoveIntoOutputSection(in_sect, out_sect)) {
      builder.mayChangeSectionTypeOrKind(out_sect, in_sect);
      builder.updateSectionFlags(out_sect, in_sect);
    }
    in_sect->setOutputSection(output);
  }

  finalizeOutputSectionFlags(output);

  // Set the first fragment in the output section.
  output->setFirstNonEmptyRule(FirstNonEmptyRule);

  output->setOrder(m_LDBackend.getSectionOrder(*out_sect));

  // Assign offsets.
  assignOffset(output);

  if (out_sect->size() || out_sect->isWanted()) {
    std::lock_guard<std::mutex> Guard(Mutex);
    if (isPartialLink)
      out_sect->setWanted(true);
    m_pModule->addOutputSectionToTable(out_sect);
  }

  return true;
}

bool ObjectLinker::initializeMerge() {
  if (AllInputSections.size())
    return true;
  {
    eld::RegisterTimer T("Prepare Input Files For Merge", "Merge Sections",
                         m_Config.options().printTimingStats());
    // Gather all input sections.
    eld::Module::obj_iterator obj, objEnd = m_pModule->obj_end();
    for (obj = m_pModule->obj_begin(); obj != objEnd; ++obj) {
      ObjectFile *ObjFile = llvm::dyn_cast<ObjectFile>(*obj);
      if (!ObjFile)
        ASSERT(0, "input is not an object file :" +
                      (*obj)->getInput()->decoratedPath());
      for (auto &sect : ObjFile->getSections()) {
        addInputSection(sect);
        if (!m_pModule->getLayoutPrinter())
          continue;
        m_pModule->getLayoutPrinter()->recordSectionStat(sect);
      }
    }
  }
  {
    eld::RegisterTimer T("Sort sections if LTO enabled", "Merge Sections",
                         m_Config.options().printTimingStats());
    // Sort sections if we have LTO enabled.
    mayBeSortSections(AllInputSections);
  }
  return true;
}

bool ObjectLinker::updateInputSectionMappingsForPlugin() {
  eld::RegisterTimer T("Update Input Rules For Plugin", "Merge Sections",
                       m_Config.options().printTimingStats());
  // Initialize merging of sections.
  initializeMerge();

  // Clear the vector of sections collected by the rules.
  for (auto &Out : m_pModule->getScript().sectionMap()) {
    for (auto &In : *Out)
      In->clearSections();
  }
  for (auto &section : AllInputSections) {
    if (section->isBitcode())
      continue;
    ELFSection *sect = llvm::dyn_cast<ELFSection>(section);
    switch (sect->getKind()) {
    // Some *INPUT sections should not be merged.
    case LDFileFormat::Null:
    case LDFileFormat::NamePool:
      continue;
    default:
      if (!sect->getMatchedLinkerScriptRule())
        continue;
      sect->getMatchedLinkerScriptRule()->addMatchedSection(sect);
    }
  }

  // Sort the rule.
  for (auto &Out : m_pModule->getScript().sectionMap()) {
    for (auto &In : *Out)
      sortSections(In, true);
  }
  return true;
}

bool ObjectLinker::finishAssignOutputSections() {
  eld::RegisterTimer T("Update Input Rules From Plugin", "Perform Layout",
                       m_Config.options().printTimingStats());
  ObjectBuilder builder(m_Config, *m_pModule);
  // Override output sections again!!
  builder.reAssignOutputSections(/*LW=*/nullptr);
  // Clear the section match map.
  m_pModule->getScript().clearAllSectionOverrides();
  updateInputSectionMappingsForPlugin();
  return true;
}

bool ObjectLinker::finishAssignOutputSections(const plugin::LinkerWrapper *LW) {
  eld::RegisterTimer T("Update Input Rules From Plugin", "Perform Layout",
                       m_Config.options().printTimingStats());
  ObjectBuilder builder(m_Config, *m_pModule);
  // Update section mapping of pending section overrides associated with the
  // LinkerWrapper LW.
  builder.reAssignOutputSections(LW);
  // Clear section overrides associated with the LinkerWrapper LW.
  m_pModule->getScript().clearSectionOverrides(LW);
  updateInputSectionMappingsForPlugin();
  return true;
}

bool ObjectLinker::runOutputSectionIteratorPlugin() {
  LinkerScript::PluginVectorT PluginList =
      m_pModule->getScript().getPluginForType(
          plugin::Plugin::OutputSectionIterator);
  if (!PluginList.size())
    return true;

  for (auto &P : PluginList) {
    if (!P->Init(m_pModule->getOutputTarWriter()))
      return false;
  }

  for (auto &P : PluginList) {
    plugin::PluginBase *L = P->getLinkerPlugin();
    for (auto &Out : m_pModule->getScript().sectionMap())
      (llvm::dyn_cast<plugin::OutputSectionIteratorPlugin>(L))
          ->processOutputSection(plugin::OutputSection(Out));
  }

  for (auto &P : PluginList) {
    if (!P->Run(m_pModule->getScript().getPluginRunList())) {
      m_pModule->setFailure(true);
      return false;
    }
  }

  if (PluginList.size()) {
    for (auto &P : PluginList) {
      if (!P->Destroy()) {
        m_pModule->setFailure(true);
        return false;
      }
    }
  }
  // Fragment movement verification is only done for CreatingSections link state
  // because fragments cannot be moved in any other link state.
  if (m_pModule->getState() == plugin::LinkerWrapper::State::CreatingSections) {
    for (auto P : PluginList) {
      auto expVerifyFragmentMoves = P->verifyFragmentMovements();
      if (!expVerifyFragmentMoves) {
        m_Config.raiseDiagEntry(std::move(expVerifyFragmentMoves.error()));
        m_pModule->setFailure(true);
        return false;
      }
    }
  }
  return !m_pModule->linkFail();
}

/// mergeSections - put allinput sections into output sections
bool ObjectLinker::mergeSections() {
  ObjectBuilder builder(m_Config, *m_pModule);
  // Output section iterator plugin.
  {
    eld::RegisterTimer T("Init", "Merge Sections",
                         m_Config.options().printTimingStats());
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
                         m_Config.options().printTimingStats());
    auto &PM = m_pModule->getPluginManager();
    if (!PM.callActBeforeSectionMergingHook())
      return false;
  }

  // Output section iterator plugin.
  {
    eld::RegisterTimer T("Plugin: Output Section Iterator Before Layout",
                         "Merge Sections",
                         m_Config.options().printTimingStats());
    if (!runOutputSectionIteratorPlugin())
      return false;
  }

  // Merge all the input sections.
  {
    eld::RegisterTimer T("Merge Input Sections", "Merge Sections",
                         m_Config.options().printTimingStats());
    mergeInputSections(builder, AllInputSections);
  }

  // Update plugins from the YAML config file specified.
  {
    eld::RegisterTimer T("Update Output Sections With Plugins",
                         "Merge Sections",
                         m_Config.options().printTimingStats());
    if (!m_pModule->updateOutputSectionsWithPlugins()) {
      m_pModule->setFailure(true);
      return false;
    }
  }

  // Create output sections.
  SectionMap::iterator out, outBegin, outEnd;
  outBegin = m_pModule->getScript().sectionMap().begin();
  outEnd = m_pModule->getScript().sectionMap().end();

  m_pModule->setState(plugin::LinkerWrapper::CreatingSections);
  if (!m_Config.getDiagEngine()->diagnose())
    return false;

  {
    eld::RegisterTimer T("Plugin: Output Section Iterator Creating Sections",
                         "Merge Sections",
                         m_Config.options().printTimingStats());
    if (!initializeOutputSectionsAndRunPlugin())
      return false;
  }

  reportPendingPluginRuleInsertions();

  {
    eld::RegisterTimer T("Create Output Section", "Merge Sections",
                         m_Config.options().printTimingStats());
    std::vector<OutputSectionEntry *> OutSections;
    for (out = outBegin; out != outEnd; ++out) {
      OutSections.push_back(*out);
    }
    for (auto &S : *m_pModule) {
      OutputSectionEntry *O = S->getOutputSection();
      if (O)
        OutSections.push_back(O);
    }

    if (m_Config.options().numThreads() <= 1 ||
        !m_Config.isCreateOutputSectionsMultiThreaded()) {
      for (auto &O : OutSections)
        createOutputSection(builder, O);
    } else {
      llvm::ThreadPoolInterface *Pool = m_pModule->getThreadPool();
      for (auto &O : OutSections)
        Pool->async([&] { createOutputSection(builder, O); });
      Pool->wait();
    }

    if (!m_Config.getDiagEngine()->diagnose()) {
      if (m_pModule->getPrinter()->isVerbose())
        m_Config.raise(diag::function_has_error) << __PRETTY_FUNCTION__;
      return false;
    }
    {
      eld::RegisterTimer T("Assign Group Sections Offset", "Merge Sections",
                           m_Config.options().printTimingStats());
      assignOffsetToGroupSections();
    }
  }
  return true;
}

bool ObjectLinker::initializeOutputSectionsAndRunPlugin() {
  for (auto &O : m_pModule->getScript().sectionMap()) {
    for (auto &in : *O)
      sortSections(in, false);
  }
  return runOutputSectionIteratorPlugin();
}

void ObjectLinker::assignOffset(OutputSectionEntry *Out) {
  int64_t O = 0;
  const ELFSection *outSection = Out->getSection();
  bool hasRules = m_pModule->getScript().linkerScriptHasRules();
  for (auto &C : *Out) {
    C->getSection()->setOffset(O);
    for (auto &F : C->getSection()->getFragmentList()) {
      if (!F->isNull()) {
        F->setOffset(O);
        O = (F->getOffset(m_Config.getDiagEngine()) + F->size());
      }

      // Warn if any non-allocatable input section has been assigned to an
      // allocatable output section. Only check for this diagnostic if
      // section mapping rules are defined in the linker script.
      if (!hasRules)
        continue;
      const ELFSection *owningSection = F->getOwningSection();
      if (outSection->isAlloc() && !owningSection->isAlloc())
        m_Config.raise(
            diag::non_allocatable_section_in_allocatable_output_section)
            << owningSection->name()
            << owningSection->getInputFile()->getInput()->decoratedPath()
            << outSection->name();
    }
    C->getSection()->setSize(O - C->getSection()->offset());
  }
  Out->getSection()->setSize(O);
}

void ObjectLinker::assignOffset(ELFSection *C) {
  int64_t O = 0;
  for (auto &F : C->getFragmentList()) {
    F->setOffset(O);
    O = (F->getOffset(m_Config.getDiagEngine()) + F->size());
  }
  C->setSize(O);
}

void ObjectLinker::assignOffsetToGroupSections() {
  for (auto &S : *m_pModule) {
    if (S->hasNoFragments())
      continue;
    if (!S->hasSectionData())
      continue;
    if (!S->isGroupKind())
      m_Config.raise(diag::expected_group_section)
          << S->getDecoratedName(m_Config.options());
    assignOffset(S);
  }
}

bool ObjectLinker::parseListFile(std::string Filename, uint32_t Type) {
  LayoutPrinter *printer = m_pModule->getLayoutPrinter();
  Input *SymbolListInput =
      eld::make<Input>(Filename, m_Config.getDiagEngine(), Input::Script);
  if (!SymbolListInput->resolvePath(m_Config))
    return false;
  // Create an Input file and set the input file to be of kind ExternList
  InputFile *SymbolListInputFile =
      InputFile::Create(SymbolListInput, InputFile::GNULinkerScriptKind,
                        m_Config.getDiagEngine());
  eld::MappingFile::Kind K = MappingFile::Other;
  if (Type == ScriptFile::ExternList)
    K = MappingFile::ExternList;
  else if (Type == ScriptFile::DynamicList)
    K = MappingFile::DynamicList;
  SymbolListInputFile->setMappingFileKind(K);
  addInputFileToTar(SymbolListInputFile, K);
  SymbolListInput->setInputFile(SymbolListInputFile);
  // Record the dynamic list script in the Map file.
  if (printer)
    printer->recordLinkerScript(SymbolListInput->decoratedPath());
  // Read the dynamic List file
  ScriptFile SymbolListReader(
      (ScriptFile::Kind)Type, *m_pModule,
      *(llvm::dyn_cast<eld::LinkerScriptFile>(SymbolListInputFile)),
      m_pBuilder->getInputBuilder(), getTargetBackend());
  bool parseSuccess = getScriptReader()->readScript(m_Config, SymbolListReader);
  if (!parseSuccess)
    return false;
  if (Type == ScriptFile::DynamicList && SymbolListReader.getDynamicList()) {
    for (const auto &Sym : *SymbolListReader.getDynamicList()) {
      m_pModule->addScriptSymbolForDynamicListFile(SymbolListInputFile, Sym);
      m_pModule->dynListSyms().push_back(Sym);
    }
  }
  if (Type == ScriptFile::ExternList || Type == ScriptFile::DynamicList) {
    if (!SymbolListReader.activate(*m_pModule))
      return false;
  } else
    for (const auto &Sym : SymbolListReader.getExternList()) {
      if (Type == ScriptFile::DuplicateCodeList)
        m_pModule->addToCopyFarCallSet(Sym->name());
      else if (Type == ScriptFile::NoReuseTrampolineList)
        m_pModule->addToNoReuseOfTrampolines(Sym->name());
    }
  return true;
}

void ObjectLinker::addDuplicateCodeInsteadOfTrampolines() {
  if (m_Config.options().hasNoCopyFarCallsFromFile())
    return;
  parseListFile(m_Config.options().copyFarCallsFromFile(),
                ScriptFile::DuplicateCodeList);
}

void ObjectLinker::addNoReuseOfTrampolines() {
  if (m_Config.options().hasNoReuseOfTrampolinesFile())
    return;
  parseListFile(m_Config.options().noReuseOfTrampolinesFile(),
                ScriptFile::NoReuseTrampolineList);
}

/// addUndefSymbols - add any symbols specified by the -u flag
///   @return true if symbols added
bool ObjectLinker::addUndefSymbols() {
  GeneralOptions::const_ext_list_iterator it =
      m_Config.options().ext_list_begin();
  GeneralOptions::const_ext_list_iterator ie =
      m_Config.options().ext_list_end();

  for (; it != ie; it++) {
    if (!parseListFile(*it, ScriptFile::ExternList))
      return false;
  }

  LDSymbol *output_sym = nullptr;
  Resolver::Result result;
  // Add the entry symbol.
  if (m_Config.options().hasEntry()) {
    if (string::isValidCIdentifier(m_Config.options().entry()))
      m_Config.options().getUndefSymList().emplace_back(
          eld::make<StrToken>(m_Config.options().entry()));
  }

  if (!m_Config.options().getUndefSymList().empty()) {
    GeneralOptions::const_undef_sym_iterator undef_sym,
        undef_symEnd = m_Config.options().undef_sym_end();
    for (undef_sym = m_Config.options().undef_sym_begin();
         undef_sym != undef_symEnd; ++undef_sym) {
      InputFile *I = m_pModule->getInternalInput(
          eld::Module::InternalInputType::ExternList);
      m_pModule->getNamePool().insertSymbol(
          I, (*undef_sym)->name(), false, eld::ResolveInfo::NoType,
          eld::ResolveInfo::Undefined, eld::ResolveInfo::Global, 0, 0,
          eld::ResolveInfo::Default, nullptr, result,
          false /* isPostLTOPhase */, false, 0, false /* isPatchable */,
          m_pModule->getPrinter());
      // create a output LDSymbol. All external symbols are entry symbols.
      output_sym = make<LDSymbol>(result.info, false);
      result.info->setOutSymbol(output_sym);
      // Initialize origin.
      result.info->setResolvedOrigin(I);
      m_pModule->addNeededSymbol((*undef_sym)->name());
    }
  }

  auto &DynExpSymbols = m_Config.options().getExportDynSymList();
  for (const auto &S : DynExpSymbols) {
    InputFile *I = m_pModule->getInternalInput(
        eld::Module::InternalInputType::DynamicExports);
    m_pModule->getNamePool().insertSymbol(
        I, S->name(), false, eld::ResolveInfo::NoType,
        eld::ResolveInfo::Undefined, eld::ResolveInfo::Global, 0, 0,
        eld::ResolveInfo::Default, NULL, result, false /* isPostLTOPhase */,
        false, 0, false /* isPatchable */, m_pModule->getPrinter());
    // create a output LDSymbol. All external symbols are entry symbols.
    output_sym = make<LDSymbol>(result.info, false);
    result.info->setOutSymbol(output_sym);
    // Initialize origin.
    result.info->setResolvedOrigin(I);
    m_pModule->addNeededSymbol(S->name());
  }
  return true;
}

bool ObjectLinker::addSymbolToOutput(const ResolveInfo &pInfo) const {
  // Dont add bitcode symbols to output.
  if (pInfo.isBitCode())
    return false;

  // Dont add section symbols.
  if (pInfo.isSection())
    return false;

  // Dont add internal symbols.
  if (pInfo.visibility() == ResolveInfo::Internal)
    return false;

  // Dont add undefined symbols, that are garbage collected
  if (pInfo.isUndef() && pInfo.outSymbol() && pInfo.outSymbol()->shouldIgnore())
    return false;

  // If its the dot symbol, dont add to output.
  if (pInfo.outSymbol() && pInfo.outSymbol() == m_pModule->getDotSymbol())
    return false;

  // Dont add symbols for which we dont have an outsymbol.
  if (nullptr == pInfo.outSymbol())
    return false;

  if (pInfo.outSymbol()->fragRef() && pInfo.outSymbol()->fragRef()->isDiscard())
    return false;

  // if the symbols defined in the Ignore sections (e.g. discared by GC), then
  // not to put them to output
  if ((pInfo.outSymbol()->hasFragRef() && pInfo.getOwningSection()->isIgnore()))
    return false;

  // if the symbols defined in the Ignore sections (e.g. discared by Plugin),
  // then not to put them to output.
  if ((pInfo.outSymbol()->hasFragRef() &&
       pInfo.getOwningSection()->isDiscard()))
    return false;

  // Set the Used property.
  InputFile *resolvedOrigin = pInfo.resolvedOrigin();

  if (resolvedOrigin) {
    if (!pInfo.isFile())
      resolvedOrigin->setUsed(true);
  }

  static InputFile *I = m_pModule->getInternalInput(
      eld::Module::InternalInputType::DynamicExports);
  if (pInfo.isUndef() && resolvedOrigin && resolvedOrigin == I)
    return false;

  if ((resolvedOrigin && resolvedOrigin->isInternal()) &&
      (!pInfo.isWeak() && pInfo.isUndef() &&
       (m_Config.options().getErrorStyle() == GeneralOptions::llvm))) {
    m_Config.raise(diag::symbol_undefined_by_user)
        << pInfo.name() << resolvedOrigin->getInput()->decoratedPath();
    return false;
  }

  // If this is a undefined reference to wrapper in LTO generated native
  // object, it is handled with references. It is now an artifact from its
  // bitcode file. So ignore this symbol
  if (pInfo.isUndef() && resolvedOrigin &&
      m_pModule->hasWrapReference(pInfo.name()) && resolvedOrigin->isBitcode())
    return false;

  // Let the backend choose to add the symbol to the output.
  if (!m_LDBackend.addSymbolToOutput(const_cast<ResolveInfo *>(&pInfo)))
    return false;

  return true;
}

bool ObjectLinker::mayBeAddSectionSymbol(ELFSection *S) {
  if (S->isRelocationKind())
    return false;
  InputFile *I =
      m_pModule->getInternalInput(Module::InternalInputType::Sections);
  if (!S->isWanted() && !S->size())
    return false;
  ResolveInfo *sym_info = m_pModule->getNamePool().createSymbol(
      I, S->name().str(), false, ResolveInfo::Section, ResolveInfo::Define,
      ResolveInfo::Local,
      0x0, // size
      ResolveInfo::Default, false /*isPostLTOPhase*/);
  LDSymbol *sym = make<LDSymbol>(sym_info, false);
  sym_info->setOutSymbol(sym);
  Fragment *F = S->getOutputSection()->getFirstFrag();
  FragmentRef *FragRef = FragmentRef::Null();
  if (!F)
    return false;
  FragRef = make<FragmentRef>(*F, 0);
  sym->setFragmentRef(FragRef);
  // Record the section for lookup.
  m_pModule->recordSectionSymbol(S, sym_info);
  // Add to the symbol table.
  m_pModule->addSymbol(sym_info);
  return true;
}

bool ObjectLinker::addSectionSymbols() {
  // create and add section symbols for each output section
  for (auto &S : m_pModule->getScript().sectionMap())
    mayBeAddSectionSymbol(S->getSection());
  for (auto &S : *m_pModule)
    mayBeAddSectionSymbol(S);
  return true;
}

bool ObjectLinker::addSymbolsToOutput() {

  // Traverse all the free ResolveInfo and add the output symobols to output
  for (auto &L : m_pModule->getNamePool().getLocals()) {
    accountSymForTotalSymStats(*L);
    if (addSymbolToOutput(*L))
      m_pModule->addSymbol(L);
    else
      accountSymForDiscardedSymStats(*L);
  }

  // We should not be adding any symbol from dynamic shared libraries into
  // getGlobals.
  // Traverse all the resolveInfo and add the output symbol to output
  for (auto &G : m_pModule->getNamePool().getGlobals()) {
    ResolveInfo *R = G.getValue();
    accountSymForTotalSymStats(*R);
    if (addSymbolToOutput(*R))
      m_pModule->addSymbol(R);
    else
      accountSymForDiscardedSymStats(*R);
  }

  // The option --unresolved-symbols is ignored for partial linking
  if (LinkerConfig::Object == m_Config.codeGenType())
    return true;

  // Note the string version is checked here. This is to make sure the default
  // behavior is not affected by this functionality. The default behavior is
  // really used by so many customers and lets not break it.
  if (m_Config.options().reportUndefPolicy().empty() ||
      (m_Config.options().reportUndefPolicy() == "ignore-all"))
    return true;

  bool hasError = false;
  for (auto &G : m_pModule->getNamePool().getGlobals()) {
    ResolveInfo *R = G.getValue();
    bool AddSymbolToOutput = addSymbolToOutput(*R);
    // All dynamic symbols in shared libraries are processed here.
    if (!AddSymbolToOutput) {
      // Skip if the symbol is not dynamic.
      if (!R->isDyn())
        continue;
      // Unfortunately we need to check again.
      if (R->outSymbol() && R->outSymbol() == m_pModule->getDotSymbol())
        continue;
      // If the symbol is undef and not weak, and the symbol needs to be
      // reported, lets report it
      if (R->isUndef() && !R->isWeak()) {
        if (!m_pModule->getLinker()->shouldIgnoreUndefine(R->isDyn())) {
          hasError = true;
          m_Config.raise(diag::undefined_symbol)
              << R->name() << R->resolvedOrigin()->getInput()->decoratedPath();
        }
      }
      continue;
    }
    // If the undefined symbol needs to be reported because the linker
    // was passed an option for unresolved symbol policy, lets do that.
    if (m_LDBackend.canIssueUndef(R)) {
      hasError = true;
      m_Config.raise(diag::undefined_symbol)
          << R->name() << R->resolvedOrigin()->getInput()->decoratedPath();
    }
  }

  if (hasError)
    m_pModule->setFailure(true);

  return !hasError;
}

/// addStandardSymbols - shared object and executable files need some
/// standard symbols
///   @return if there are some input symbols with the same name to the
///   standard symbols, return false
bool ObjectLinker::addStandardSymbols() {
  return m_LDBackend.initStandardSymbols();
}

/// addTargetSymbols - some targets, such as MIPS and ARM, need some
/// target-dependent symbols
///   @return if there are some input symbols with the same name to the
///   target symbols, return false
bool ObjectLinker::addTargetSymbols() {
  m_LDBackend.initTargetSymbols();
  return true;
}

bool ObjectLinker::processInputFiles() {
  std::vector<InputFile *> inputs;
  getInputs(inputs);
  return m_LDBackend.processInputFiles(inputs);
}

/// addScriptSymbols - define symbols from the command line option or linker
/// scripts.
bool ObjectLinker::addScriptSymbols() {
  LinkerScript &script = m_pModule->getScript();
  LinkerScript::Assignments::iterator it, ie = script.assignments().end();
  uint64_t symValue = 0x0;
  LDSymbol *symbol = nullptr;
  bool ret = true;
  // go through the entire symbol assignments
  for (it = script.assignments().begin(); it != ie; ++it) {
    Assignment *assignCmd = (*it).second;
    InputFile *ScriptInput = m_pModule->getInternalInput(eld::Module::Script);
    // FIXME: Ideally, assignCmd should always have a context. We should perhaps
    // add an internal error if the context is missing.
    if (assignCmd->hasInputFileInContext())
      ScriptInput = assignCmd->getInputFileInContext();
    if (assignCmd->level() == Assignment::OUTSIDE_SECTIONS) {
      if (assignCmd->type() == Assignment::ASSERT)
        continue;
      (*it).first = nullptr;
    }
    llvm::StringRef symName = assignCmd->name();
    ResolveInfo::Type type = ResolveInfo::NoType;
    ResolveInfo::Visibility vis = ResolveInfo::Default;
    size_t size = 0;
    ResolveInfo *old_info = m_pModule->getNamePool().findInfo(symName.str());
    symbol = nullptr;

    if (assignCmd->isProvideOrProvideHidden()) {
      if (m_LDBackend.isSymInProvideMap(symName)) {
        if (m_Config.showLinkerScriptWarnings())
          m_Config.raise(diag::warn_provide_sym_redecl)
              << assignCmd->getContext() << symName;
        continue;
      }
      m_LDBackend.addProvideSymbol(symName, assignCmd);
      if (!m_pModule->getAssignmentForSymbol(symName))
        m_pModule->addAssignment(symName, assignCmd);
      continue;
    }
    // if the symbol does not exist, we can set type to NOTYPE
    // else we retain its type, same goes for size - 0 or retain old value
    // and visibility - Default or retain
    if (old_info != nullptr) {
      type = static_cast<ResolveInfo::Type>(old_info->type());
      vis = old_info->visibility();
      size = old_info->size();

      if (old_info->outSymbol() && old_info->outSymbol()->hasFragRefSection()) {
        if (old_info->isPatchable())
          m_Config.raise(diag::error_patchable_script)
              << old_info->outSymbol()->name();
        else
          m_Config.raise(diag::warn_gc_duplicate_symbol)
              << old_info->outSymbol()->name();
      }
    }
    PluginManager &PM = m_pModule->getPluginManager();
    SymbolInfo symInfo(ScriptInput, size, ResolveInfo::Absolute, type, vis,
                       ResolveInfo::Define,
                       /*isBitCode=*/false);
    // We do not create input symbols for non object file symbols!
    DiagnosticPrinter *DP = m_Config.getPrinter();
    auto oldErrorCount = DP->getNumErrors() + DP->getNumFatalErrors();
    PM.callVisitSymbolHook(/*sym=*/nullptr, symName, symInfo);
    auto newErrorCount = DP->getNumErrors() + DP->getNumFatalErrors();
    if (newErrorCount > oldErrorCount)
      ret = false;
    // Add symbol and refine the visibility if needed
    switch ((*it).second->type()) {
    case Assignment::HIDDEN:
      vis = ResolveInfo::Hidden;
      LLVM_FALLTHROUGH;
    // Fall through
    case Assignment::DEFAULT:
      symbol =
          m_pBuilder
              ->AddSymbol<eld::IRBuilder::Force, eld::IRBuilder::Unresolve>(
                  ScriptInput, symName.str(), type, ResolveInfo::Define,
                  ResolveInfo::Absolute, size, symValue, FragmentRef::Null(),
                  vis, true /*PostLTOPhase*/);
      break;
    case Assignment::PROVIDE_HIDDEN:
    case Assignment::PROVIDE:
    case Assignment::FILL:
    case Assignment::ASSERT:
      continue;
    }
    if (symbol) {
      symbol->setShouldIgnore(false);
      symbol->resolveInfo()->setResolvedOrigin(ScriptInput);
      symbol->setScriptDefined();
      symbol->resolveInfo()->setInBitCode(false);
    }
    // Set symbol of this assignment.
    (*it).first = symbol;

    // Record that there was an assignment for this symbol.
    // If there is a relocation to this symbol, the symbols contained in the
    // assignment also need to be considered as part of the list of symbols
    // that will be live.
    if (symbol)
      m_pModule->addAssignment(symbol->resolveInfo()->name(), (*it).second);
  }

  Resolver::Result resolved_result;
  InputFile *I =
      m_pModule->getInternalInput(eld::Module::InternalInputType::Script);
  m_pModule->getNamePool().insertSymbol(
      I, ".", true, ResolveInfo::NoType, ResolveInfo::Undefined,
      ResolveInfo::NoneBinding, 0, 0, ResolveInfo::Hidden, nullptr,
      resolved_result, true, false, 0, false /* isPatchable */,
      m_pModule->getPrinter());
  LDSymbol *dot_sym = make<LDSymbol>(resolved_result.info, true);
  dot_sym->setFragmentRef(FragmentRef::Null());
  dot_sym->setValue(0);
  resolved_result.info->setOutSymbol(dot_sym);
  m_pModule->setDotSymbol(dot_sym);
  return ret;
}

bool ObjectLinker::addDynListSymbols() {
  // Dynamic list is only for non-shared libraries that use dynamic symbol
  // tables
  if (m_Config.codeGenType() == LinkerConfig::DynObj &&
      !m_Config.options().isPIE())
    return true;
  bool noError = true;
  std::vector<ScriptSymbol *> &dynListSyms = m_pModule->dynListSyms();
  std::unordered_set<ScriptSymbol *> UsedPatterns;
  for (auto &G : m_pModule->getNamePool().getGlobals()) {
    ResolveInfo *R = G.getValue();
    LDSymbol *sym = m_pModule->getNamePool().findSymbol(R->name());
    bool ShouldExport = !R->isUndef() &&
                        R->visibility() == ResolveInfo::Default && sym &&
                        !sym->shouldIgnore();
    if (m_DynlistExports.count(R->name())) {
      if (ShouldExport)
        R->setExportToDyn();
      continue;
    }
    for (auto &Pattern : dynListSyms) {
      if (Pattern->matched(*R)) {
        if (ShouldExport)
          R->setExportToDyn();
        UsedPatterns.insert(Pattern);
        break;
      }
    }
  }
  if (m_Config.options().getErrorStyle() == GeneralOptions::llvm) {
    for (const auto &E : dynListSyms)
      if (!UsedPatterns.count(E)) {
        noError = false;
        m_Config.raise(diag::dynlist_symbol_undefined_by_user) << E->name();
      }
  }
  // No further processing is done for dynamic list symbols
  dynListSyms.clear();
  m_DynlistExports.clear();
  return noError;
}

size_t ObjectLinker::getRelocSectSize(uint32_t type, uint32_t count) {
  return count * (type == llvm::ELF::SHT_RELA ? m_LDBackend.getRelaEntrySize()
                                              : m_LDBackend.getRelEntrySize());
}

void ObjectLinker::createRelocationSections() {

  if (!m_Config.options().emitRelocs())
    return;

  // SectionName, contentPermissions, filepath
  struct SectionKey {
    SectionKey() : _name(""), _type(-1) {}

    SectionKey(llvm::StringRef name, int32_t type) : _name(name), _type(type) {}

    // Data members
    StringRef _name;
    int32_t _type;
  };

  struct SectionKeyInfo {
    static SectionKey getEmptyKey() { return SectionKey(); }
    static SectionKey getTombstoneKey() { return SectionKey(); }
    static unsigned getHashValue(const SectionKey &k) {
      return llvm::hash_combine(k._name, k._type);
    }
    static bool isEqual(const SectionKey &lhs, const SectionKey &rhs) {
      return ((lhs._name == rhs._name) && (lhs._type == rhs._type));
    }
  };

  // apply all relocations of all inputs
  llvm::DenseMap<SectionKey, uint32_t, SectionKeyInfo> outputRelocCount;
  llvm::DenseMap<SectionKey, uint32_t, SectionKeyInfo> outputRelocAlignment;
  llvm::DenseMap<SectionKey, ELFSection *, SectionKeyInfo>
      outputRelocTargetSect;

  eld::Module::obj_iterator input, inEnd = m_pModule->obj_end();
  for (input = m_pModule->obj_begin(); input != inEnd; ++input) {
    ELFObjectFile *ObjFile = llvm::dyn_cast<ELFObjectFile>(*input);
    if (!ObjFile)
      continue;
    for (auto &rs : ObjFile->getRelocationSections()) {
      if (rs->isIgnore())
        continue;
      if (rs->isDiscard())
        continue;

      for (auto &relocation : rs->getLink()->getRelocations()) {
        if (m_LDBackend.maySkipRelocProcessing(relocation))
          continue;

        auto countRelocEntries = [&](Relocation *relocation) -> void {
          ELFSection *output_sect =
              relocation->targetRef()->frag()->getOwningSection();

          if (output_sect->getOutputSection())
            output_sect = output_sect->getOutputSection()->getSection();

          ELFSection *targetSection = relocation->targetSection();

          if (targetSection &&
              (!targetSection->isDiscard() && !targetSection->isIgnore())) {
            ELFSection *targetOutputSection =
                targetSection->getOutputELFSection();
            targetOutputSection->setWantedInOutput();
          }

          // Count the num of entries that refers to that section.
          SectionKey sectKey(output_sect->name(),
                             m_LDBackend.getRelocator()->relocType());
          outputRelocCount[sectKey] += 1;
          outputRelocAlignment[sectKey] =
              std::max(rs->getAddrAlign(), outputRelocAlignment[sectKey]);
          outputRelocTargetSect[sectKey] = output_sect;
        };
        countRelocEntries(relocation);
      } // for all relocations
    } // for all relocation section
  } // for all inputs
  for (const auto &kv : outputRelocCount) {
    ELFSection *output_sect = m_pModule->createOutputSection(
        m_LDBackend.getOutputRelocSectName(kv.first._name.str(),
                                           kv.first._type),
        LDFileFormat::Relocation, kv.first._type /* Reloc Kind */, 0x0,
        outputRelocAlignment[kv.first]);
    output_sect->setEntSize(kv.first._type == llvm::ELF::SHT_RELA
                                ? m_LDBackend.getRelaEntrySize()
                                : m_LDBackend.getRelEntrySize());
    outputRelocTargetSect[kv.first]->setWantedInOutput(true);
    output_sect->setLink(outputRelocTargetSect[kv.first]);
    output_sect->setSize(getRelocSectSize(kv.first._type, kv.second));
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
    LDSymbol &GlobalSymbol =
        m_LDBackend.defineSymbolforCopyReloc(*m_pBuilder, AliasSym, &Sym);
    GlobalSymbol.resolveInfo()->setResolvedOrigin(Sym.resolvedOrigin());
    addCopyReloc(m_LDBackend, *GlobalSymbol.resolveInfo(), Type);
  }
  LDSymbol &CopySym =
      m_LDBackend.defineSymbolforCopyReloc(*m_pBuilder, &Sym, &Sym);
  if (!AliasSym)
    addCopyReloc(m_LDBackend, *CopySym.resolveInfo(), Type);
}

void ObjectLinker::scanRelocationsHelper(InputFile *input, bool isPartialLink,
                                         LinkerScript::PluginVectorT PVect,
                                         Relocator::CopyRelocs &CopyRelocs) {
  ELFObjectFile *ObjFile = llvm::dyn_cast<ELFObjectFile>(input);
  if (!ObjFile)
    return;
  uint32_t NumPlugins = PVect.size();
  for (auto &rs : ObjFile->getRelocationSections()) {
    if (rs->isIgnore())
      continue;
    if (rs->isDiscard())
      continue;
    for (auto &relocation : rs->getLink()->getRelocations()) {
      // Skip unneeded relocations
      if (m_LDBackend.maySkipRelocProcessing(relocation))
        continue;
      if (NumPlugins) {
        for (auto &P : PVect)
          P->callReloc(relocation->type(), relocation);
      }
      ELFSection *relocSection = rs;
      // scan relocation
      if (!isPartialLink)
        m_LDBackend.getRelocator()->scanRelocation(
            *relocation, *m_pBuilder, *relocSection, *input, CopyRelocs);
      else
        m_LDBackend.getRelocator()->partialScanRelocation(*relocation,
                                                          *relocSection);
    } // for all relocations
  } // for all relocation section
}

LinkerScript::PluginVectorT ObjectLinker::getLinkerPluginWithLinkerConfigs() {
  const LinkerScript::PluginVectorT PluginVect =
      m_pModule->getScript().getPlugins();
  LinkerScript::PluginVectorT PluginVectHavingLinkerConfigs;
  for (auto &P : PluginVect) {
    if (P->getLinkerPluginConfig())
      PluginVectHavingLinkerConfigs.push_back(P);
  }
  return PluginVectHavingLinkerConfigs;
}

bool ObjectLinker::scanRelocations(bool isPartialLink) {
  LinkerScript::PluginVectorT PluginVect = getLinkerPluginWithLinkerConfigs();

  m_LDBackend.provideSymbols();

  std::vector<std::unique_ptr<Relocator::CopyRelocs>> AllCopyRelocs;
  if (m_Config.options().numThreads() <= 1 ||
      !m_Config.isScanRelocationsMultiThreaded()) {
    if (m_pModule->getPrinter()->traceThreads())
      m_Config.raise(diag::threads_disabled) << "ScanRelocations";
    for (auto &input : m_pModule->getObjectList()) {
      auto CopyRelocs = std::make_unique<Relocator::CopyRelocs>();
      scanRelocationsHelper(input, isPartialLink, PluginVect, *CopyRelocs);
      AllCopyRelocs.push_back(std::move(CopyRelocs));
    }
  } else {
    if (m_pModule->getPrinter()->traceThreads())
      m_Config.raise(diag::threads_enabled)
          << "ScanRelocations" << m_Config.options().numThreads();
    llvm::ThreadPoolInterface *Pool = m_pModule->getThreadPool();
    for (auto &input : m_pModule->getObjectList()) {
      auto CopyRelocs = std::make_unique<Relocator::CopyRelocs>();
      auto &CopyRelocsRef = *CopyRelocs; // must dereference in the main thread
      Pool->async([&] {
        scanRelocationsHelper(input, isPartialLink, PluginVect, CopyRelocsRef);
      });
      AllCopyRelocs.push_back(std::move(CopyRelocs));
    }
    Pool->wait();
  }
  // assume there is only one copy relocation type per target
  Relocation::Type CopyRelocType = m_LDBackend.getCopyRelType();
  for (const auto &RelocVec : AllCopyRelocs)
    for (auto &Reloc : *RelocVec)
      createCopyRelocation(*Reloc, CopyRelocType);

  // Merge per-file relocations
  if (!isPartialLink) {
    ELFObjectFile *RelocInput = m_LDBackend.getDynamicSectionHeadersInputFile();
    auto MergeRelocs = [](llvm::SmallVectorImpl<Relocation *> &To,
                          const llvm::SmallVectorImpl<Relocation *> &From) {
      To.insert(To.end(), From.begin(), From.end());
    };
    for (auto &input : m_pModule->getObjectList())
      if (ELFObjectFile *Obj = llvm::dyn_cast<ELFObjectFile>(input))
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
  if (!m_Config.getDiagEngine()->diagnose()) {
    if (m_pModule->getPrinter()->isVerbose())
      m_Config.raise(diag::function_has_error) << __PRETTY_FUNCTION__;
    return false;
  }
  return true;
}

bool ObjectLinker::finalizeScanRelocations() {
  m_LDBackend.finalizeScanRelocations();
  if (!m_Config.getDiagEngine()->diagnose()) {
    if (m_pModule->getPrinter()->isVerbose())
      m_Config.raise(diag::function_has_error) << __PRETTY_FUNCTION__;
    return false;
  }
  return true;
}

/// initStubs - initialize stub-related stuff.
bool ObjectLinker::initStubs() {
  // initialize BranchIslandFactory
  m_LDBackend.initBRIslandFactory();

  // initialize StubFactory
  m_LDBackend.initStubFactory();

  // initialize target stubs
  m_LDBackend.initTargetStubs();
  return true;
}

bool ObjectLinker::allocateCommonSymbols() {
  for (auto &G : m_pModule->getNamePool().getGlobals()) {
    ResolveInfo *R = G.getValue();
    if (R->isCommon() && !R->isBitCode())
      m_pModule->addCommonSymbol(R);
  }

  // Do not allocate common symbols for partial link unless define common
  // option is provided.
  if (LinkerConfig::Object == m_Config.codeGenType() &&
      !m_Config.options().isDefineCommon())
    return true;
  if (!m_pModule->sortCommonSymbols())
    return false;
  return m_LDBackend.allocateCommonSymbols();
}

bool ObjectLinker::addDynamicSymbols() {
  m_LDBackend.sizeDynNamePools();
  return true;
}

/// prelayout - help backend to do some modification before layout
bool ObjectLinker::prelayout() {
  m_LDBackend.preLayout();

  if (!m_Config.getDiagEngine()->diagnose())
    return false;

  m_LDBackend.sizeDynamic();

  m_LDBackend.initSymTab();

  createRelocationSections();

  return true;
}

/// layout - linearly layout all output sections and reserve some space
/// for GOT/PLT
///   Because we do not support instruction relaxing in this early version,
///   if there is a branch can not jump to its target, we return false
///   directly
bool ObjectLinker::layout() { return m_LDBackend.layout(); }

/// prelayout - help backend to do some modification after layout
bool ObjectLinker::postlayout() { return m_LDBackend.postLayout(); }

bool ObjectLinker::printlayout() { return m_LDBackend.printLayout(); }

bool ObjectLinker::finalizeBeforeWrite() {
  m_LDBackend.finalizeBeforeWrite();
  return true;
}

/// finalizeSymbolValue - finalize the resolved symbol value.
///   Before relocate(), after layout(), ObjectLinker should correct value of
///   all
///   symbol.
bool ObjectLinker::finalizeSymbolValues() {
  for (auto i : m_pModule->getSymbols())
    finalizeSymbolValue(i);
  return true;
}

/// Finalize one symbol.
void ObjectLinker::finalizeSymbolValue(ResolveInfo *i) const {
  if (i->isAbsolute() || i->type() == ResolveInfo::File)
    return;

  if (i->type() == ResolveInfo::ThreadLocal) {
    i->outSymbol()->setValue(m_LDBackend.finalizeTLSSymbol(i->outSymbol()));
    return;
  }

  if (i->outSymbol()->hasFragRef() &&
      i->outSymbol()->fragRef()->frag()->getOwningSection()->isDiscard())
    return;

  if (i->outSymbol()->hasFragRef()) {
    // set the virtual address of the symbol. If the output file is
    // relocatable object file, the section's virtual address becomes zero.
    // And the symbol's value become section relative offset.
    uint64_t value = i->outSymbol()->fragRef()->getOutputOffset(*m_pModule);
    assert(nullptr != i->outSymbol()->fragRef()->frag());
    // For sections that are zero in size, there is no output section. Just
    // rely on the owning section name for now.
    ELFSection *section = i->getOwningSection();
    if (section->getOutputSection())
      section = section->getOutputSection()->getSection();
    if (section->isAlloc())
      i->outSymbol()->setValue(value + section->addr());
    else
      i->outSymbol()->setValue(value);
  }
}

/// relocate - applying relocation entries and create relocation
/// section in the output files
/// Create relocation section, asking GNULDBackend to
/// read the relocation information into RelocationEntry
/// and push_back into the relocation section
bool ObjectLinker::relocation(bool emitRelocs) {
  // when producing relocatables, no need to apply relocation
  if (LinkerConfig::Object == m_Config.codeGenType())
    return true;

  // Mapping section to count and max_size.
  llvm::DenseMap<ELFSection *, unsigned> reloc_count, max_sect_size;

  auto emitOneReloc = [&](Relocation *relocation) -> bool {
    if (!emitRelocs)
      return true;
    ResolveInfo *info = relocation->symInfo();
    ELFSection *output_sect =
        m_pModule->getSection(m_LDBackend.getOutputRelocSectName(
            relocation->targetRef()->getOutputELFSection()->name().str(),
            m_LDBackend.getRelocator()->relocType()));

    if (!output_sect)
      return true;

    // Make sure we have an entry for each section because it's possible
    // that the whole section has no any valid relocs.
    if (reloc_count.find(output_sect) == reloc_count.end()) {
      reloc_count[output_sect] = 0;
      max_sect_size[output_sect] = output_sect->size();
    }
    Relocation *r = Relocation::Create(
        relocation->type(),
        m_LDBackend.getRelocator()->getSize(relocation->type()),
        relocation->targetRef(), relocation->addend());

    ResolveInfo *sym_info = info;

    if (info->type() == ResolveInfo::Section) {
      sym_info = m_pModule->getSectionSymbol(relocation->outputSection());
      // Get symbol offset from the relocation itself.
      if (!m_Config.options().emitGNUCompatRelocs()) {
        r->setAddend(relocation->symOffset(*m_pModule));
      }
    }
    // set relocation target symbol to the output section symbol's
    // resolveInfo
    r->setSymInfo(sym_info);
    output_sect->addRelocation(r);
    reloc_count[output_sect] += 1;
    return true;
  };

  auto processObjectFile = [&](ObjectFile *ObjFile) -> bool {
    for (auto &sect : ObjFile->getSections()) {
      if (sect->isBitcode())
        continue;
      ELFSection *section = llvm::dyn_cast<eld::ELFSection>(sect);
      if (!section->hasRelocData())
        continue;
      if (section->getKind() != LDFileFormat::Internal)
        continue;
      // Skip internal relocation sections.
      if (section->isRelocationSection())
        continue;
      for (auto &relocation : section->getRelocations()) {
        relocation->apply(*m_LDBackend.getRelocator());
      }
    }

    if (!m_Config.getDiagEngine()->diagnose()) {
      return false;
    }

    ELFFileBase *EObjFile = llvm::dyn_cast<ELFFileBase>(ObjFile);

    if (!EObjFile)
      return false;

    for (auto &rs : EObjFile->getRelocationSections()) {
      if (rs->isIgnore())
        continue;
      if (rs->isDiscard())
        continue;
      auto processReloc = [&](Relocation *relocation) -> bool {
        // bypass the reloc if the symbol is in the discarded input section
        ResolveInfo *info = relocation->symInfo();

        // Dont process relocations that are relaxed.
        if (m_LDBackend.isRelocationRelaxed(relocation))
          return false;

        if (!info->outSymbol()->hasFragRef() &&
            ResolveInfo::Section == info->type() &&
            ResolveInfo::Undefined == info->desc())
          return false;

        if ((info->outSymbol()->hasFragRef() && info->outSymbol()
                                                    ->fragRef()
                                                    ->frag()
                                                    ->getOwningSection()
                                                    ->isDiscard()))
          return false;

        ELFSection *applySect =
            relocation->targetRef()->frag()->getOwningSection();
        // bypass the reloc if the section where it sits will be discarded.
        if (applySect->isIgnore())
          return false;
        if (applySect->isDiscard())
          return false;
        ELFSection *targetSect = relocation->targetSection();
        if (info->outSymbol()->shouldIgnore() ||
            (info->outSymbol()->fragRef() &&
             info->outSymbol()->fragRef()->isDiscard()) ||
            (targetSect && targetSect->isIgnore())) {
          if (m_pModule->getPrinter()->isVerbose())
            m_Config.raise(diag::applying_endof_image_address)
                << info->name() << relocation->getTargetPath(m_Config.options())
                << relocation->getSourcePath(m_Config.options());
          relocation->target() =
              m_LDBackend.getValueForDiscardedRelocations(relocation);
          return false;
        }
        return true;
      };
      for (auto &relocation : rs->getLink()->getRelocations()) {
        if (processReloc(relocation)) {
          if (emitRelocs)
            emitOneReloc(relocation);
          relocation->apply(*m_LDBackend.getRelocator());
        }
      } // for all relocations
    } // for all relocation section

    if (!m_Config.getDiagEngine()->diagnose()) {
      return false;
    }

    if (m_Config.options().emitRelocs()) {
      for (auto &kv : reloc_count)
        kv.first->setSize(getRelocSectSize(kv.first->getType(), kv.second));
    }
    return true;
  };

  // apply all relocations of all inputs
  if (m_Config.options().numThreads() <= 1 ||
      !m_Config.isApplyRelocationsMultiThreaded()) {
    if (m_pModule->getPrinter()->traceThreads())
      m_Config.raise(diag::threads_disabled) << "ApplyRelocations";
    for (auto &input : m_pModule->getObjectList()) {
      ObjectFile *ObjFile = llvm::dyn_cast<ObjectFile>(input);
      if (!ObjFile)
        continue;
      processObjectFile(ObjFile);
    }
  } else {
    if (m_pModule->getPrinter()->traceThreads())
      m_Config.raise(diag::threads_enabled)
          << "ApplyRelocations" << m_Config.options().numThreads();
    llvm::ThreadPoolInterface *Pool = m_pModule->getThreadPool();
    for (auto &input : m_pModule->getObjectList()) {
      Pool->async([&] {
        ObjectFile *ObjFile = llvm::dyn_cast<ObjectFile>(input);
        if (!ObjFile)
          return;
        processObjectFile(ObjFile);
      });
    }
    Pool->wait();
  }

  // apply relocations created by relaxation
  SectionMap::iterator out, outBegin, outEnd;
  typedef std::vector<BranchIsland *>::iterator branch_island_iter;
  outBegin = m_pModule->getScript().sectionMap().begin();
  outEnd = m_pModule->getScript().sectionMap().end();
  for (out = outBegin; out != outEnd; ++out) {
    branch_island_iter bi = (*out)->islands_begin();
    branch_island_iter be = (*out)->islands_end();
    for (; bi != be; ++bi) {
      BranchIsland::reloc_iterator iter, iterEnd = (*bi)->reloc_end();
      for (iter = (*bi)->reloc_begin(); iter != iterEnd; ++iter) {
        (*iter)->apply(*m_LDBackend.getRelocator());
      }
    }
  }

  // apply linker created relocations
  for (auto R : m_LDBackend.getInternalRelocs()) {
    R->apply(*m_LDBackend.getRelocator());
  }

  // apply relocations
  if (!m_Config.getDiagEngine()->diagnose()) {
    return false;
  }
  return true;
}

/// emitOutput - emit the output file.
bool ObjectLinker::emitOutput(llvm::FileOutputBuffer &pOutput) {
  return std::error_code() == getWriter()->writeObject(*m_pModule, pOutput);
}

/// postProcessing - do modification after all processes
eld::Expected<void>
ObjectLinker::postProcessing(llvm::FileOutputBuffer &pOutput) {
  {
    eld::RegisterTimer T("Sync Relocations", "Emit Output File",
                         m_Config.options().printTimingStats());
    syncRelocations(pOutput.getBufferStart());
  }

  {
    eld::RegisterTimer T("Post Process Output File", "Emit Output File",
                         m_Config.options().printTimingStats());
    eld::Expected<void> expPostProcess = m_LDBackend.postProcessing(pOutput);
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expPostProcess);
  }
  return {};
}

void ObjectLinker::syncRelocations(uint8_t *buffer) {

  // We MUST write relocation by relaxation before those
  // from the inputs because something like R_AARCH64_COPY_INSN
  // must be written before the input relocation overwritten the same
  // location again.
  auto syncBranchIslandsForOutputSection = [&](OutputSectionEntry *O) -> void {
    typedef std::vector<BranchIsland *>::iterator branch_island_iter;
    branch_island_iter bi = O->islands_begin();
    branch_island_iter be = O->islands_end();
    for (; bi != be; ++bi) {
      BranchIsland::reloc_iterator iter, iterEnd = (*bi)->reloc_end();
      for (iter = (*bi)->reloc_begin(); iter != iterEnd; ++iter)
        writeRelocationResult(**iter, buffer);
    }
  };
  // sync relocations created by relaxation
  if (m_Config.options().numThreads() <= 1 ||
      !m_Config.isSyncRelocationsMultiThreaded()) {
    if (m_pModule->getPrinter()->traceThreads())
      m_Config.raise(diag::threads_disabled) << "SyncRelocations";
    for (auto &out : m_pModule->getScript().sectionMap())
      syncBranchIslandsForOutputSection(out);
    // sync linker created internal relocations
    for (auto R : m_LDBackend.getInternalRelocs()) {
      writeRelocationResult(*R, buffer);
    }
    for (auto &input : m_pModule->getObjectList()) {
      syncRelocationResult(buffer, input);
    }
  } else {
    llvm::ThreadPoolInterface *Pool = m_pModule->getThreadPool();
    if (m_pModule->getPrinter()->traceThreads())
      m_Config.raise(diag::threads_enabled)
          << "SyncRelocations" << m_Config.options().numThreads();
    // QTOOL-112094: When R_AARCH_COPY_INSN is used, it may point to the same
    // target as other input relocations. Since R_AARCH_COPY_INSN is created for
    // a branch island, it may be written from a different thread than the input
    // relocation. This creates a race condition, which may lead to the result
    // of the input relocation to be overwritten by R_AARCH_COPY_INSN.
    // Therefore, a barrier is needed between writing branch island
    // relocations and input relocations.
    for (auto &out : m_pModule->getScript().sectionMap()) {
      Pool->async([&] { syncBranchIslandsForOutputSection(out); });
    }
    Pool->wait();
    // sync linker created internal relocations
    for (auto &R : m_LDBackend.getInternalRelocs()) {
      Pool->async([this, &R, &buffer] { writeRelocationResult(*R, buffer); });
    }
    Pool->wait();
    for (auto &input : m_pModule->getObjectList()) {
      Pool->async(
          [this, &buffer, input] { syncRelocationResult(buffer, input); });
    }
    Pool->wait();
  }
}

void ObjectLinker::syncRelocationResult(uint8_t *data, InputFile *input) {
  ObjectFile *ObjFile = llvm::dyn_cast<ObjectFile>(input);
  if (!ObjFile)
    return;
  for (auto &sect : ObjFile->getSections()) {
    if (sect->isBitcode())
      continue;
    ELFSection *section = llvm::dyn_cast<eld::ELFSection>(sect);
    if (!section->hasRelocData())
      continue;
    if (section->getKind() != LDFileFormat::Internal)
      continue;
    // Skip internal relocation sections.
    if (section->isRelocationSection())
      continue;
    for (auto &relocation : section->getRelocations()) {
      writeRelocationResult(*relocation, data);
    }
  }
  ELFFileBase *EObjFile = llvm::dyn_cast<ELFFileBase>(ObjFile);
  if (!EObjFile)
    return;
  for (auto &rs : EObjFile->getRelocationSections()) {
    if (rs->isIgnore())
      continue;
    if (rs->isDiscard())
      continue;

    for (auto &relocation : rs->getLink()->getRelocations()) {

      auto syncOneReloc = [&](Relocation *relocation) -> bool {
        // bypass the reloc if the symbol is in the discarded input section
        ResolveInfo *info = relocation->symInfo();

        if (!info->outSymbol()->hasFragRef() &&
            ResolveInfo::Section == info->type() &&
            ResolveInfo::Undefined == info->desc())
          return false;

        if (relocation->targetRef()->frag()->getOwningSection()->isIgnore())
          return false;

        if (relocation->targetRef()->frag()->getOwningSection()->isDiscard())
          return false;

        // bypass the relocation with NONE type. This is to avoid overwrite
        // the target result by NONE type relocation if there is a place which
        // has two relocations to apply to, and one of it is NONE type. The
        // result we want is the value of the other relocation result. For
        // example, in .exidx, there are usually an R_ARM_NONE and
        // R_ARM_PREL31 apply to the same place
        if (0x0 == relocation->type())
          return false;

        return true;
      };
      uint64_t ModifiedRelocData = 0;
      if (m_pModule->getRelocationDataForSync(relocation, ModifiedRelocData))
        writeRelocationData(*relocation, ModifiedRelocData, data);
      else if (syncOneReloc(relocation))
        writeRelocationResult(*relocation, data);
    } // for all relocations
  } // for all relocation section
}

void ObjectLinker::writeRelocationResult(Relocation &pReloc, uint8_t *pOutput) {
  // Certain relocations such as R_RISCV_RELAX and R_RISCV_ALIGN do not
  // really apply. They are way to communicate linker to do certain
  // optimizations. Applying them may overrite the immediate bits based on
  // sequence in which they appear w.r.t. "real" relocation
  if (m_LDBackend.shouldIgnoreRelocSync(&pReloc))
    return;

  writeRelocationData(pReloc, pReloc.target(), pOutput);
}

void ObjectLinker::writeRelocationData(Relocation &pReloc, uint64_t Data,
                                       uint8_t *pOutput) {

  if (!pReloc.targetRef()->getOutputELFSection()->hasOffset())
    return;

  FragmentRef::Offset Off = pReloc.targetRef()->getOutputOffset(*m_pModule);
  if (Off == (FragmentRef::Offset)-1)
    return;

  // get output file offset
  size_t out_offset = pReloc.targetRef()->getOutputELFSection()->offset() + Off;

  uint8_t *target_addr = pOutput + out_offset;

  Off = pReloc.targetRef()->getOutputOffset(*m_pModule);

  std::memcpy(target_addr, &Data, pReloc.size(*m_LDBackend.getRelocator()) / 8);
}

static void ltoDiagnosticHandler(const llvm::DiagnosticInfo &DI) {
  ASSERT(sDiagEngineForLTO, "sDiagEngineForLTO is not set!!");
  std::string ErrStorage;
  {
    llvm::raw_string_ostream OS(ErrStorage);
    llvm::DiagnosticPrinterRawOStream DP(OS);
    DI.print(DP);
  }
  switch (DI.getSeverity()) {
  case DS_Error: {
    sDiagEngineForLTO->raise(diag::lto_codegen_error) << ErrStorage;
    break;
  }
  case DS_Warning:
    sDiagEngineForLTO->raise(diag::lto_codegen_warning) << ErrStorage;
    break;
  case DS_Note:
    sDiagEngineForLTO->raise(diag::lto_codegen_note) << ErrStorage;
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
    sDiagEngineForLTO->raise(diag::lto_codegen_remark) << ErrStorage;
    break;
  }
}

bool ObjectLinker::runAssembler(
    std::vector<std::string> &files, std::string RelocModel,
    const std::vector<std::string> &asmFilesFromLTO) {
  std::vector<std::string> fileList;

  if (m_Config.options().hasLTOAsmFile()) {
    for (auto &f : m_Config.options().ltoAsmFile()) {
      m_Config.raise(diag::using_lto_asm_file) << f;
      fileList.push_back(f);
    }
  } else {
    for (auto &f : asmFilesFromLTO) {
      if (!f.empty())
        fileList.push_back(f);
    }
  }

  if (m_Config.options().hasLTOOutputFile()) {
    for (auto &f : m_Config.options().ltoOutputFile()) {
      m_Config.raise(diag::using_lto_output_file) << f;
      files.push_back(f);
    }
    return true;
  }

  uint32_t Count = 0;
  for (auto f : fileList) {
    SmallString<256> outputFileName;
    auto OS = createLTOTempFile(Count, "o", outputFileName);
    if (!OS)
      return false;
    files.push_back(std::string(outputFileName));

    if (!m_LDBackend.ltoCallExternalAssembler(f, RelocModel,
                                              std::string(outputFileName)))
      return false;

    ++Count;
  }
  return true;
}

std::unique_ptr<llvm::lto::LTO> ObjectLinker::LTOInit(llvm::lto::Config Conf) {
  // Parse codegen options and pre-initialize the config
  eld::RegisterTimer T("Initialize LTO", "LTO",
                       m_Config.options().printTimingStats());
  if (m_TraceLTO) {
    if (m_Config.options().codegenOpts()) {
      std::stringstream ss;
      for (auto ai : m_Config.options().codeGenOpts())
        ss << ai << " ";
      m_Config.raise(diag::codegen_options)
          << m_Config.targets().triple().str() << ss.str();
    } else {
      m_Config.raise(diag::codegen_options)
          << m_Config.targets().triple().str() << "none";
    }
  }

  Conf.DiagHandler = ltoDiagnosticHandler;
  if (m_Config.options().hasLTOOptRemarksFile()) {
    std::string OptYamlFileName =
        m_Config.options().outputFileName() + std::string("-LTO.opt.yaml");
    Conf.RemarksFilename = OptYamlFileName;
    if (m_Config.options().hasLTOOptRemarksDisplayHotness())
      Conf.RemarksWithHotness = true;
  }
  std::string Cpu = m_LDBackend.getInfo().getOutputMCPU().str();
  if (!Cpu.empty()) {
    Conf.CPU = Cpu;
    if (m_TraceLTO)
      m_Config.raise(diag::set_codegen_mcpu) << Cpu;
  }

  Conf.DefaultTriple = m_Config.targets().triple().str();

  if (m_LDBackend.ltoNeedAssembler())
    Conf.CGFileType = llvm::CodeGenFileType::AssemblyFile;

  auto Model = ltoCodegenModel();

  Conf.RelocModel = Model.first;

  // If save-temps is enabled, get LTO to write out the module
  // TODO: This will write out *lots* of files at different stages. We may
  // want to curtail this to just two as before (for RegularLTO) and perhaps
  // more for ThinLTO.
  if (m_TraceLTO || m_SaveTemps) {
    if (llvm::Error E = Conf.addSaveTemps(m_LTOTempPrefix)) {
      m_Config.raise(diag::lto_cannot_save_temps)
          << llvm::toString(std::move(E));
      return {};
    }
    // FIXME: Output actual file names (this will go with the above TODO).
    m_Config.raise(diag::note_temp_lto_bitcode) << m_LTOTempPrefix + "*";
  }

  // Set the number of backend threads to use in ThinLTO
  unsigned NumThreads = 1;
  if (m_Config.options().threadsEnabled()) {
    NumThreads = m_Config.options().numThreads();
    if (!NumThreads)
      NumThreads = std::thread::hardware_concurrency();
    if (!NumThreads)
      NumThreads = 4; // if hardware_concurrency returns 0
    m_Config.raise(diag::note_lto_threads) << NumThreads;
  }

  // Initialize the LTO backend
  llvm::lto::ThinBackend Backend = llvm::lto::createInProcessThinBackend(
      llvm::heavyweight_hardware_concurrency(NumThreads));
  return std::make_unique<llvm::lto::LTO>(std::move(Conf), std::move(Backend));
}

bool ObjectLinker::FinalizeLTOSymbolResolution(
    llvm::lto::LTO &LTO, const std::vector<BitcodeFile *> &bitCodeInputs) {

  eld::RegisterTimer T("Finalize Symbol Resolution", "LTO",
                       m_Config.options().printTimingStats());
  bool isPreserveAllSet = m_Config.options().preserveAllLTO();
  bool isPreserveGlobals = m_Config.options().exportDynamic();

  bool traceWrap = m_pModule->getPrinter()->traceWrapSymbols();

  std::set<std::string> symbolsToPreserve;
  std::set<ResolveInfo *> PreserveSyms;

  for (auto &L : m_pModule->getNamePool().getLocals()) {
    if (L->shouldPreserve() || (isPreserveAllSet && L->isBitCode()))
      PreserveSyms.insert(L);
  }

  // Traverse all the resolveInfo and add the output symbol to output
  for (auto &G : m_pModule->getNamePool().getGlobals()) {
    ResolveInfo *info = G.getValue();
    // preserve all defined global non-hidden symbol in bitcode when building
    // shared library.
    if (m_Config.options().hasShared()) {
      // Symbols with reduced scope in version script must be skipped
      auto symbolScopes = m_LDBackend.symbolScopes();
      const auto &found = symbolScopes.find(info);
      bool DoNotPreserve = found != symbolScopes.end() &&
                           found->second->isLocal() && !info->isUndef();

      if (info->isBitCode() &&
          (info->desc() == ResolveInfo::Define ||
           info->desc() == ResolveInfo::Common) &&
          info->binding() != ResolveInfo::Local &&
          (info->visibility() == ResolveInfo::Default ||
           info->visibility() == ResolveInfo::Protected) &&
          !DoNotPreserve) {
        PreserveSyms.insert(info);
      }
    }
    // preserve all defined global symbol in bitcode when building
    // relocatable.
    if (m_Config.codeGenType() == LinkerConfig::Object) {
      if (info->isBitCode() &&
          (info->desc() == ResolveInfo::Define ||
           info->desc() == ResolveInfo::Common) &&
          info->binding() != ResolveInfo::Local)
        info->shouldPreserve(true);
    }
    if (info->shouldPreserve() ||
        ((isPreserveAllSet || isPreserveGlobals) && info->isBitCode())) {
      if (info->outSymbol() &&
          !(m_GCHasRun && info->outSymbol()->shouldIgnore()))
        PreserveSyms.insert(info);
      else if (m_TraceLTO)
        m_Config.raise(diag::note_not_preserving_symbol) << info->name();
    }
    if (info->isBitCode() && info->isCommon() &&
        m_pModule->getScript().linkerScriptHasSectionsCommand()) {
      // never internalize common symbols.
      m_pModule->recordCommon(info->name(), info->resolvedOrigin());
      if (m_TraceLTO)
        m_Config.raise(diag::note_preserving_common)
            << info->name() << "COMMON"
            << info->resolvedOrigin()->getInput()->decoratedPath();
      PreserveSyms.insert(info);
    }
    if (!(m_GCHasRun && info->outSymbol()->shouldIgnore())) {
      std::vector<ScriptSymbol *> &dynListSyms = m_pModule->dynListSyms();
      for (auto &Pattern : dynListSyms) {
        if (Pattern->matched(*info)) {
          PreserveSyms.insert(info);
          if (m_TraceLTO)
            m_Config.raise(diag::preserve_dyn_list_sym) << info->name();
        }
      }
    }
    // Wrapped functions using --wrap need to be preserved as well
    if (info->isBitCode() && !m_Config.options().renameMap().empty()) {
      llvm::StringMap<std::string>::iterator renameSym =
          m_Config.options().renameMap().find(info->name());
      if (m_Config.options().renameMap().end() != renameSym) {
        if (m_TraceLTO || traceWrap)
          m_Config.raise(diag::preserve_wrap) << info->name();
        PreserveSyms.insert(info);
        ResolveInfo *wrapper =
            m_pModule->getNamePool().findInfo(renameSym->second);
        if (wrapper && wrapper->isBitCode()) {
          PreserveSyms.insert(wrapper);
          if (m_TraceLTO || traceWrap)
            m_Config.raise(diag::preserve_wrap) << wrapper->name();
        }
      }
    }
  }

  // Symbols that are preserved from Bitcode files are set to be used.
  for (auto &S : PreserveSyms) {
    InputFile *ResolvedOrigin = S->resolvedOrigin();
    if (ResolvedOrigin)
      ResolvedOrigin->setUsed(true);
    symbolsToPreserve.insert(S->name());
  }
  if (m_Config.options().preserveSymbolsLTO()) {
    for (auto sym : m_Config.options().getPreserveList())
      symbolsToPreserve.insert(sym);
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

  bool HasSectionsCmd = m_pModule->getScript().linkerScriptHasSectionsCommand();

  // Add the input files
  for (BitcodeFile *inp : bitCodeInputs) {

    // Compute the LTO resolutions
    std::vector<llvm::lto::SymbolResolution> LTOResolutions;
    for (const auto &Sym : inp->getInputFile().symbols()) {

      // Only needed: name, isCommon, isUndefined

      llvm::lto::SymbolResolution LTORes;

      ResolveInfo *Info =
          m_pModule->getNamePool().findInfo(Sym.getName().str());

      if (!Info) {
        llvm_unreachable("Global LTO symbol not in namepool");
      } else if (!Sym.isUndefined()) {

        // If this definition is chosen, set the prevailing property.
        if (Info->resolvedOrigin() == inp)
          LTORes.Prevailing = true;
        // If a symbol needs to be preserved, because its being referenced
        // from regular object files, set the VisibletoRegularObj property
        // appropriately.
        if (symbolsToPreserve.count(Sym.getName().str())) {
          if (m_TraceLTO)
            m_Config.raise(diag::note_preserve_symbol) << Sym.getName();
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
        llvm::StringMap<std::string>::iterator renameSym =
            m_Config.options().renameMap().find(WrapSymbolName);
        if (m_Config.options().renameMap().end() != renameSym) {
          LTORes.Prevailing = true;
          LTORes.VisibleToRegularObj = true;
          if (m_TraceLTO || traceWrap)
            m_Config.raise(diag::preserve_wrapper_def) << Sym.getName();
          // If the symbol is part of --wrap mechanism, mark it as linker
          // renamed
          // and save its binding. Post LTO phase will restore the binding to
          // this
          // original value.
          if (Info->isDefine()) {
            LTORes.LinkerRedefined = true;
            m_pModule->saveWrapSymBinding(Info->name(), Info->binding());
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
            (m_Config.codeGenType() == LinkerConfig::Exec ||
             m_Config.options().isPIE() ||
             Info->visibility() != ResolveInfo::Default))
          LTORes.FinalDefinitionInLinkageUnit = true;
      }

      if (m_TraceLTO)
        m_Config.raise(diag::lto_resolution)
            << Sym.getName() << LTORes.Prevailing << LTORes.VisibleToRegularObj
            << LTORes.FinalDefinitionInLinkageUnit
            << (Info ? Info->resolvedOrigin()->getInput()->decoratedPath()
                     : "N/A")
            << LTORes.LinkerRedefined;

      LTOResolutions.push_back(LTORes);
    }

    if (m_TraceLTO)
      m_Config.raise(diag::adding_module) << inp->getInput()->decoratedPath();

    if (llvm::Error E = LTO.add(inp->takeLTOInputFile(), LTOResolutions)) {
      m_Config.raise(diag::fatal_lto_merge_error)
          << llvm::toString(std::move(E));
      m_pModule->setFailure(true);
      return false;
    }
  }

  return true;
}

void ObjectLinker::addTempFilesToTar(size_t maxTasks) {
  if (!m_SaveTemps)
    return;
  std::vector<std::string> suffix = {".0.preopt.bc",      ".1.promote.bc",
                                     ".2.internalize.bc", ".3.import.bc",
                                     ".4.opt.bc",         ".5.precodegen.bc"};
  auto prefix = m_Config.options().outputFileName();
  prefix += ".llvm-lto.";
  // not a bitcode file but should probably be placed in Bitcode category
  addInputFileToTar(prefix + "resolution.txt", eld::MappingFile::Kind::Bitcode);
  for (size_t task = 0; task <= maxTasks; ++task)
    for (auto &s : suffix)
      addInputFileToTar(prefix + llvm::utostr(task) + s,
                        eld::MappingFile::Kind::Bitcode);
}

void ObjectLinker::addLTOOutputToTar() {
  if (!m_pModule->getOutputTarWriter())
    return;
  for (auto &ipt : LTOELFFiles) {
    eld::InputFile *iptFile = ipt->getInputFile();
    iptFile->setMappedPath(ipt->getName());
    iptFile->setMappingFileKind(eld::MappingFile::Kind::ObjectFile);
    m_pModule->getOutputTarWriter()->addInputFile(iptFile,
                                                  /*isLTO*/ true);
  }
}

bool ObjectLinker::DoLTO(llvm::lto::LTO &LTO) {
  eld::RegisterTimer T("Invoke LTO", "LTO",
                       m_Config.options().printTimingStats());
  // Run LTO
  std::vector<std::string> files;
  if (!m_Config.options().hasLTOOutputFile())
    files.resize(LTO.getMaxTasks());
  filesToRemove.resize(LTO.getMaxTasks());

  auto AddStream = [&](size_t Task, const Twine &moduleName)
      -> llvm::Expected<std::unique_ptr<llvm::CachedFileStream>> {
    SmallString<256> ObjFileName;
    auto OS = createLTOTempFile(
        Task, m_LDBackend.ltoNeedAssembler() ? "s" : "o", ObjFileName);

    if (!OS) {
      m_pModule->setFailure(true);
      return OS.takeError();
    }

    assert(files[Task].empty() && "LTO task already produced an output!");
    files[Task] = std::string(ObjFileName);
    if (!m_SaveTemps)
      filesToRemove[Task] = std::string(ObjFileName);
    return std::make_unique<llvm::CachedFileStream>(std::move(*OS));
  };

  // Callback to add a file from the cache
  auto AddBuffer = [&](size_t Task, const Twine &moduleName,
                       std::unique_ptr<MemoryBuffer> MB) {
    auto S = AddStream(Task, moduleName);
    if (!S)
      report_fatal_error(Twine("unable to add memory buffer: ") +
                         toString(S.takeError()));
    *(*S)->OS << MB->getBuffer();
  };

  llvm::FileCache ThinLTOCache;
  std::string CacheDirectory;
  if (m_Config.options().isLTOCacheEnabled()) {
    if (m_Config.options().getLTOCacheDirectory().empty())
      CacheDirectory =
          (llvm::Twine(m_Config.options().outputFileName() + ".ltocache"))
              .str();
    else
      CacheDirectory = m_Config.options().getLTOCacheDirectory().str();

    llvm::Expected<llvm::FileCache> LC =
        llvm::localCache("ThinLTO", "Thin", CacheDirectory, AddBuffer);

    if (!LC) {
      m_Config.raise(diag::fatal_lto_cache_error)
          << CacheDirectory << llvm::toString(LC.takeError());
      m_pModule->setFailure(true);
      return false;
    } else if (m_TraceLTO)
      m_Config.raise(diag::note_lto_cache) << CacheDirectory;

    ThinLTOCache = *LC;
  }

  if (m_LDBackend.ltoNeedAssembler()) {
    // If the output files haven't already been provided, run compilation now
    std::vector<std::string> LTOAsmOutput;
    if (!m_Config.options().hasLTOOutputFile() &&
        !m_Config.options().hasLTOAsmFile()) {
      if (llvm::Error E = LTO.run(AddStream, ThinLTOCache)) {
        m_Config.raise(diag::fatal_no_codegen_compile)
            << llvm::toString(std::move(E));
        m_pModule->setFailure(true);
        return false;
      }
      // AddStream adds the LTO output files to 'files', but in this case
      // they're just the input files to the assembler so we need to move them
      // to their own array. ltoRunAssembler will add the assembled objects to
      // files.
      LTOAsmOutput = std::move(files);
    }
    if (!runAssembler(files, ltoCodegenModel().second, LTOAsmOutput)) {
      m_Config.raise(diag::lto_codegen_error) << "Assembler error occurred";
      m_pModule->setFailure(true);
      return false;
    }

    if (!m_SaveTemps && !m_Config.options().hasLTOOutputFile())
      filesToRemove.insert(filesToRemove.end(), files.begin(), files.end());
  } else {
    // If the output files haven't already been provided, run compilation now
    if (!m_Config.options().hasLTOOutputFile()) {
      if (llvm::Error E = LTO.run(AddStream, ThinLTOCache)) {
        m_Config.raise(diag::fatal_no_codegen_compile)
            << llvm::toString(std::move(E));
        m_pModule->setFailure(true);
        return false;
      }
    } else {
      for (auto &f : m_Config.options().ltoOutputFile()) {
        m_Config.raise(diag::using_lto_output_file) << f;
        files.push_back(f);
      }
    }
  }
  if (!m_Config.getDiagEngine()->diagnose()) {
    if (m_pModule->getPrinter()->isVerbose())
      m_Config.raise(diag::function_has_error) << __PRETTY_FUNCTION__;
    return false;
  }

  // Add the generated object files into the InputBuilder structure
  for (auto &f : files) {
    if (f.empty())
      continue;
    ltoObjects.push_back(f);
    if (m_SaveTemps)
      m_Config.raise(diag::note_temp_lto_object) << f;
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
    for (std::pair<StringRef, StringRef> o = getToken(Arg); !o.first.empty();
         o = getToken(o.second)) {

      if (o.first.starts_with("-O")) {
        auto ParsedCGOptLevel =
            llvm::StringSwitch<std::optional<CodeGenOptLevel>>(
                o.first.substr(2))
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
              << o.first << "\n";
      } else if (o.first.starts_with("debug-pass-manager") ||
                 o.first.starts_with("-debug-pass-manager")) {
        // TODO: Clean up compiler code that uses "debug-pass-manager".
        // The compiler also passes all kinds of options in codegen=,
        // even those that are not codegen options. That's why we
        // have to special case these here.
        Conf.DebugPassManager = true;
      } else if (o.first.consume_front("-mcpu=")) {
        Conf.CPU = o.first.str();
      } else if (o.first.consume_front("-mattr=")) {
        Conf.MAttrs.push_back(o.first.str());
      } else
        UserArgs.push_back(o.first.str());
    }
  }
}

} // namespace

void ObjectLinker::setLTOPlugin(plugin::LinkerPlugin &LTOPlugin) {
  this->LTOPlugin = &LTOPlugin;
}

bool ObjectLinker::createLTOObject(void) {
  std::vector<BitcodeFile *> bitCodeInputs;
  std::vector<InputFile *> allInputs;

  eld::Module::const_obj_iterator input, inEnd = m_pModule->obj_end();
  for (input = m_pModule->obj_begin(); input != inEnd; ++input) {
    if ((*input)->isBitcode()) {
      bitCodeInputs.push_back(llvm::dyn_cast<eld::BitcodeFile>(*input));
    }
    // Dont assign output sections to input files that are specified
    // with just symbols
    if ((*input)->getInput()->getAttribute().isJustSymbols())
      continue;
    allInputs.push_back(*input);
  }

  if (!bitCodeInputs.size())
    return true;
  {
    eld::RegisterTimer T("Assign Output sections to Bitcode sections and ELF",
                         "LTO", m_Config.options().printTimingStats());
    // Centralize assigning output sections.
    assignOutputSections(allInputs);
  }

  llvm::lto::Config Conf;
  std::vector<std::string> Options;
  processCodegenOptions(m_Config.options().codeGenOpts(), Conf, Options);

  m_LDBackend.AddLTOOptions(Options);

  if (LTOPlugin)
    LTOPlugin->ModifyLTOOptions(Conf, Options);

  // Config.MllvmArgs are ignored, call cl::ParseCommandLineOptions instead.
  std::vector<const char *> CodegenArgv(1, "LTOCodeGenOptions");
  for (const std::string &Arg : Options)
    CodegenArgv.push_back(Arg.c_str());
  cl::ParseCommandLineOptions(CodegenArgv.size(), CodegenArgv.data());

  Conf.Options = llvm::codegen::InitTargetOptionsFromCodeGenFlags(
      m_Config.targets().triple());

  if (LTOPlugin) {

    // TODO: Why do we read all relocations here? Get rid of?
    if (!readRelocations())
      return false;

    LTOPlugin->ActBeforeLTO(Conf);
  }

  PrepareDiagEngineForLTO prepareDiagnosticsForLTO(m_Config.getDiagEngine());

  std::unique_ptr<llvm::lto::LTO> LTO = LTOInit(std::move(Conf));
  if (!LTO)
    return false;

  if (!FinalizeLTOSymbolResolution(*LTO, bitCodeInputs))
    return false;

  if (!DoLTO(*LTO)) {
    m_pModule->setFailure(true);
    return false;
  }
  return true;
}

void ObjectLinker::beginPostLTO() {
  TextLayoutPrinter *printer = m_pModule->getTextMapPrinter();
  if (printer) {
    printer->addLayoutMessage("Pre-LTO Map records\n");
    printer->printArchiveRecords(*m_pModule);
    printer->printInputActions();
    printer->clearInputRecords();
    printer->addLayoutMessage("Post-LTO Map records");
  }
  if (m_Config.options().cref())
    m_LDBackend.printCref(m_postLTOPhase);
  m_postLTOPhase = true;
}

bool ObjectLinker::insertPostLTOELF() {
  if (ltoObjects.empty())
    return true;

  eld::Module::obj_iterator obj, bitcodeObj, objEnd = m_pModule->obj_end();

  InputFile *BitcodeObject = nullptr;
  for (obj = m_pModule->obj_begin(); obj != objEnd; ++obj) {
    if ((*obj)->isBitcode()) {
      if (!BitcodeObject) {
        BitcodeObject = (*obj);
        bitcodeObj = obj;
      }
      // Release memory with Any Bitcode files.
      BitcodeFile *BCFile = llvm::dyn_cast<eld::BitcodeFile>(*obj);
      if (BCFile && BCFile->canReleaseMemory())
        BCFile->releaseMemory(m_pModule->getPrinter()->isVerbose());
    }
  }

  assert(bitcodeObj != objEnd);

  for (auto &ltoobj : ltoObjects) {
    sys::fs::Path path = ltoobj;
    Input *In = m_pBuilder->getInputBuilder().createInput(ltoobj);
    if (!In->resolvePath(m_Config)) {
      m_pModule->setFailure(true);
      return false;
    }
    InputFile *I = InputFile::Create(In, m_Config.getDiagEngine());
    if (I->getKind() == InputFile::ELFObjFileKind) {
      // FIXME: We should convert all llvm::dyn_cast which should always be
      // successful to llvm::cast.
      ELFObjectFile *ELFObj = llvm::dyn_cast<ELFObjectFile>(I);
      ELFObj->setLTOObject();
      bool ELFOverridenWithBC = false;
      eld::Expected<bool> expParseFile =
          getNewRelocObjParser()->parseFile(*I, ELFOverridenWithBC);
      ASSERT(!ELFOverridenWithBC, "Invalid ELF override to BC operation!");
      // Currently, we have to consider two cases:
      // expParseFile returns an error: In this case, we report the error and
      // return false.
      // expParseFile returns false: Reading failed but we do not
      // have error info available. In this case, we just return false.
      // There should ideally be no case when reading fails but there is no
      // error information available. All such cases must be removed.
      if (!expParseFile)
        m_Config.raiseDiagEntry(std::move(expParseFile.error()));
      if (!expParseFile.has_value() || !expParseFile.value()) {
        return false;
      }
      In->setInputFile(I);
    } else {
      // Issue an error if the object is not recognized.
      m_Config.raise(diag::err_unrecognized_input_file)
          << In->decoratedPath() << m_Config.targets().triple().str();
      m_pModule->setFailure(true);
      return false;
    }
    if (m_TraceLTO)
      m_Config.raise(diag::note_insert_object) << In->getResolvedPath();

    if (!m_Config.getDiagEngine()->diagnose()) {
      if (m_pModule->getPrinter()->isVerbose())
        m_Config.raise(diag::function_has_error) << __PRETTY_FUNCTION__;
      return false;
    }

    LTOELFFiles.push_back(std::move(In));
  }

  addLTOOutputToTar();

  std::vector<InputFile *> ltoobjectfiles;
  for (auto &elffile : LTOELFFiles)
    ltoobjectfiles.push_back(elffile->getInputFile());

  m_pModule->insertLTOObjects(bitcodeObj, ltoobjectfiles);

  if (!m_pModule->getScript().linkerScriptHasSectionsCommand())
    return true;

  llvm::StringMap<InputFile *> m_SectionWithOldInputMap;
  // Record the section and the map that contains the old input for a section.
  for (auto &elffile : ltoobjectfiles) {
    ELFObjectFile *ObjFile = llvm::dyn_cast<ELFObjectFile>(elffile);
    for (auto &sect : ObjFile->getSections()) {
      ELFSection *section = llvm::dyn_cast<eld::ELFSection>(sect);
      if (section->hasOldInputFile())
        m_SectionWithOldInputMap[section->name()] = section->getOldInputFile();
    }
  }
  // If a section appears with the same name, then associate the same input
  // for the section that contains the old symbol too.
  for (auto &elffile : ltoobjectfiles) {
    ELFObjectFile *ObjFile = llvm::dyn_cast<ELFObjectFile>(elffile);
    for (auto &sect : ObjFile->getSections()) {
      ELFSection *section = llvm::dyn_cast<eld::ELFSection>(sect);
      // If the section already has an old input associated with it, move on.
      if (section->getOldInputFile())
        continue;
      // Very important to note, that sections that are not tracked and that
      // sections that have the same name will be assigned the same input.
      auto oldInput = m_SectionWithOldInputMap.find(section->name());
      if (oldInput == m_SectionWithOldInputMap.end())
        continue;
      section->setOldInputFile(oldInput->second);
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
  if ((!m_Config.isCodeStatic() &&
       m_Config.codeGenType() != LinkerConfig::Exec) ||
      (LinkerConfig::DynObj == m_Config.codeGenType()) ||
      m_Config.options().isPIE()) {
    if (m_TraceLTO)
      m_Config.raise(diag::lto_code_model) << "Dynamic";
    return std::make_pair(llvm::Reloc::PIC_, "pic");
  }
  // -frwpi, -fropi
  if (m_Config.options().hasRWPI() && m_Config.options().hasROPI()) {
    if (m_TraceLTO)
      m_Config.raise(diag::lto_code_model) << "ROPI, RWPI";
    return std::make_pair(llvm::Reloc::ROPI_RWPI, "ropi-rwpi");
  }
  // -frwpi
  if (m_Config.options().hasRWPI()) {
    if (m_TraceLTO)
      m_Config.raise(diag::lto_code_model) << "RWPI";
    return std::make_pair(llvm::Reloc::RWPI, "rwpi");
  }
  // --fropi
  if (m_Config.options().hasROPI()) {
    if (m_TraceLTO)
      m_Config.raise(diag::lto_code_model) << "ROPI";
    return std::make_pair(llvm::Reloc::ROPI, "ropi");
  }
  // If the relocation model is static (the default),
  // we'll let LLVM decide which RelocModel to use based on the "PIC Level"
  // module flag which is set if -fPIC was specified during bitcode
  // compilation. This allows partial linking of -fPIC code with LTO.
  if (m_TraceLTO)
    m_Config.raise(diag::lto_code_model) << "Auto Detect (Default: Static)";
  return std::make_pair(std::nullopt, "static");
}

bool ObjectLinker::runSectionIteratorPlugin() {
  ObjectBuilder builder(m_Config, *m_pModule);
  std::vector<InputFile *> inputs;
  std::vector<ELFSection *> entrySections;
  getInputs(inputs);
  builder.InitializePluginsAndProcess(inputs,
                                      plugin::Plugin::Type::SectionIterator);

  builder.reAssignOutputSections(/*LW=*/nullptr);
  return true;
}

void ObjectLinker::addInputFileToTar(InputFile *ipt, MappingFile::Kind K) {
  auto outputTar = m_pModule->getOutputTarWriter();
  if (!outputTar || !ipt)
    return;
  if (ipt->isInternal())
    return;
  if (ipt->getInput()->isArchiveMember())
    return;
  Input *i = ipt->getInput();
  bool useDecorated =
      !i->isNamespec() && ipt->getKind() == InputFile::Kind::ELFDynObjFileKind;
  ipt->setMappedPath(useDecorated ? i->decoratedPath() : i->getName());
  ipt->setMappingFileKind(K);
  outputTar->addInputFile(ipt, /*isLTO*/ false);
}

bool ObjectLinker::readAndProcessInput(Input *input, bool isPostLTO) {
  if (input->isInternal())
    return true;
  if (input->isAlreadyReleased())
    return true;
  if (!input->getSize())
    m_Config.raise(diag::input_file_has_zero_size) << input->decoratedPath();
  LayoutPrinter *printer = m_pModule->getLayoutPrinter();
  std::string path = input->getResolvedPath().native();
  InputFile *CurInput = input->getInputFile();
  if (!CurInput) {
    InputFile *I = InputFile::Create(input, m_Config.getDiagEngine());
    if (!I) {
      m_Config.raise(diag::err_unrecognized_input_file)
          << input->getResolvedPath() << m_Config.targets().triple().str();
      m_pModule->setFailure(true);
      return false;
    }
    CurInput = I;
    input->setInputFile(I);
  }
  if (CurInput->shouldSkipFile()) {
    if (printer) {
      printer->recordInputActions(LayoutPrinter::Skipped, input);
    }
    return true;
  }

  if (input->getAttribute().isPatchBase() &&
      CurInput->getKind() != InputFile::ELFExecutableFileKind) {
    m_Config.raise(diag::err_patch_base_not_executable)
        << input->getResolvedPath();
    m_pModule->setFailure(true);
    return false;
  }

  if (CurInput->isBinaryFile()) {
    eld::RegisterTimer T("Read ELF Executable Files", "Read all Input files",
                         m_Config.options().printTimingStats());
    if (printer)
      printer->recordInputKind(InputFile::Kind::BinaryFileKind);
    eld::Expected<void> expParseFile =
        getBinaryFileParser()->parseFile(*CurInput);
    if (!expParseFile) {
      m_Config.raiseDiagEntry(std::move(expParseFile.error()));
      return false;
    }
    m_pModule->getObjectList().push_back(CurInput);
    addInputFileToTar(CurInput, eld::MappingFile::Kind::ObjectFile);
  } else if (CurInput->getKind() == InputFile::ELFExecutableFileKind) {
    eld::RegisterTimer T("Read ELF Executable Files", "Read all Input files",
                         m_Config.options().printTimingStats());
    if (printer)
      printer->recordInputKind(CurInput->getKind());
    bool ELFOverriddenWithBC = false;
    eld::Expected<bool> expParseFile =
        getELFExecObjParser()->parseFile(*CurInput, ELFOverriddenWithBC);
    if (!expParseFile)
      m_Config.raiseDiagEntry(std::move(expParseFile.error()));
    if (!expParseFile.has_value() || !expParseFile.value()) {
      m_pModule->setFailure(true);
      return false;
    }
    if (!isPostLTO && overrideELFObjectWithBitCode(CurInput)) {
      return readAndProcessInput(input, isPostLTO);
    }
    m_pModule->getObjectList().push_back(CurInput);
    addInputFileToTar(CurInput, eld::MappingFile::Kind::ObjectFile);
  } else if (CurInput->getKind() == InputFile::ELFObjFileKind) {
    eld::RegisterTimer T("Read ELF Object Files", "Read all Input files",
                         m_Config.options().printTimingStats());
    if (printer)
      printer->recordInputKind(CurInput->getKind());
    bool ELFOverridenWithBC = false;
    eld::Expected<bool> expParseFile =
        getNewRelocObjParser()->parseFile(*CurInput, ELFOverridenWithBC);
    // Currently, we have to consider two cases:
    // expParseFile returns an error: In this case, we report the error and
    // return false.
    // expParseFile returns false: Reading failed but we do not
    // have error info available. In this case, we just return false.
    // There should ideally be no case when reading fails but there is no
    // error information available. All such cases must be removed.
    if (!expParseFile)
      m_Config.raiseDiagEntry(std::move(expParseFile.error()));
    if (!expParseFile.has_value() || !expParseFile.value()) {
      m_pModule->setFailure(true);
      return false;
    }
    if (ELFOverridenWithBC) {
      return readAndProcessInput(input, isPostLTO);
    }
    CurInput->setToSkip();
    m_pModule->getObjectList().push_back(CurInput);
    addInputFileToTar(CurInput, eld::MappingFile::Kind::ObjectFile);
  } else if (CurInput->getKind() == InputFile::BitcodeFileKind) {
    eld::RegisterTimer T("Read Bitcode Object Files", "Read all Input files",
                         m_Config.options().printTimingStats());
    if (printer)
      printer->recordInputKind(CurInput->getKind());
    if (isPostLTO)
      return true;
    CurInput->setToSkip();
    // LTO is needed.
    m_pModule->setLTONeeded();
    if (!getBitcodeReader()->readInput(*CurInput, LTOPlugin))
      return false;
    m_pModule->getObjectList().push_back(CurInput);
    addInputFileToTar(CurInput, MappingFile::Bitcode);
  } else if (CurInput->getKind() == InputFile::ELFSymDefFileKind) {
    eld::RegisterTimer T("Read SymDef Object Files", "Read all Input files",
                         m_Config.options().printTimingStats());
    if (printer)
      printer->recordInputKind(CurInput->getKind());
    if (m_Config.codeGenType() != LinkerConfig::Exec) {
      m_Config.raise(diag::symdef_incompatible_option);
      return false;
    }
    addInputFileToTar(CurInput, eld::MappingFile::Kind::SymDef);
    CurInput->setToSkip();
    // Just to make it simple, I just added template code.
    if (!getSymDefReader()->readHeader(*CurInput, isPostLTO))
      return false;
    if (!getSymDefReader()->readSections(*CurInput, isPostLTO))
      return false;
    if (!getSymDefReader()->readSymbols(*CurInput, isPostLTO)) {
      m_Config.raise(diag::file_has_error) << input->decoratedPath();
      return false;
    }
    if (printer)
      printer->recordInputActions(LayoutPrinter::Load, input);
  } else if (CurInput->getKind() == InputFile::ELFDynObjFileKind) {
    eld::RegisterTimer T("Read ELF Shared Object Files", "Read all Input files",
                         m_Config.options().printTimingStats());
    if (printer)
      printer->recordInputKind(CurInput->getKind());
    addInputFileToTar(CurInput, eld::MappingFile::SharedLibrary);
    CurInput->setToSkip();
    if (m_Config.isLinkPartial()) {
      m_Config.raise(diag::err_shared_objects_in_partial_link)
          << input->decoratedPath();
      return false;
    }
    if (input->getAttribute().isStatic()) {
      m_Config.raise(diag::err_mixed_shared_static_objects)
          << input->decoratedPath();
      return false;
    }
    auto expRead = getNewDynObjReader()->parseFile(*CurInput);
    // Currently, we have to consider two cases:
    // expRead returns an error: In this case, we report the error and return.
    // expRead returns false: Reading failed but we do not have error info
    // available. In this case, we just return false.
    if (!expRead.has_value() || !expRead.value()) {
      if (!expRead.has_value())
        m_Config.raiseDiagEntry(std::move(expRead.error()));
      return false;
    }
    m_pModule->getDynLibraryList().push_back(CurInput);
  } else if (CurInput->getKind() == InputFile::GNUArchiveFileKind) {
    eld::RegisterTimer T("Read Archive Files", "Read all Input files",
                         m_Config.options().printTimingStats());
    if (printer)
      printer->recordInputKind(CurInput->getKind());
    std::string NameSpecPath;
    if (input->getInputType() == Input::Namespec)
      NameSpecPath = "-l" + input->getFileName();
    ArchiveFile *CurArchive = llvm::dyn_cast<eld::ArchiveFile>(CurInput);
    if (m_Config.options().isInExcludeLIBS(path, NameSpecPath))
      CurArchive->setNoExport();
    MemoryArea *memArea = CurInput->getInput()->getMemArea();
    if (const ArchiveFile *AF = getArchiveFileFromMemoryAreaToAFMap(memArea)) {
      m_Config.raise(diag::verbose_reusing_archive_file_info)
          << input->decoratedPath() << "\n";
      CurArchive->setArchiveFileInfo(AF->getArchiveFileInfo());
    }
    uint32_t numObjects = 0;
    eld::Expected<uint32_t> expNumObjects =
        getArchiveParser()->parseFile(*CurInput);
    if (!expNumObjects) {
      m_Config.raise(diag::error_read_archive) << input->decoratedPath();
      m_Config.raiseDiagEntry(std::move(expNumObjects.error()));
      return false;
    }
    numObjects = expNumObjects.value();
    if ((numObjects == 0) && isPostLTO) {
      llvm::StringRef fileType = " (ELF)";
      if (CurArchive->isMixedArchive())
        fileType = " (Mixed)";
      else if (CurArchive->isBitcodeArchive())
        fileType = " (Bitcode)";
      else if (CurArchive->isELFArchive())
        fileType = " (ELF)";
      if (printer) {
        printer->recordInputActions(LayoutPrinter::SkippedRescan, input,
                                    fileType.str());
      }
    }
    m_pModule->getArchiveLibraryList().push_back(CurInput);
    addInputFileToTar(CurInput, MappingFile::Kind::Archive);
    addToMemoryAreaToAFMap(*CurArchive);
  }
  // try to parse input as a linker script
  else if (CurInput->getKind() == InputFile::GNULinkerScriptKind) {
    eld::RegisterTimer T("Read Linker Script", "Read all Input files",
                         m_Config.options().printTimingStats());
    if (printer)
      printer->recordInputKind(CurInput->getKind());
    addInputFileToTar(CurInput, eld::MappingFile::LinkerScript);
    CurInput->setToSkip();
    if (!readLinkerScript(CurInput)) {
      m_pModule->setFailure(true);
      return false;
    }
  }
  if (!m_Config.getDiagEngine()->diagnose()) {
    if (m_pModule->getPrinter()->isVerbose())
      m_Config.raise(diag::function_has_error) << __PRETTY_FUNCTION__;
    return false;
  }
  return true;
}

bool ObjectLinker::overrideELFObjectWithBitCode(InputFile *CurInputFile) {
  // If -flto option is not passed to the linker, and there is not a list
  // to include, then just move on.
  if (!m_Config.options().hasLTO() && !LTOPatternList.size())
    return false;

  eld::RegisterTimer T("Read Mixed ELF/Bitcode Object Files",
                       "Read all Input files",
                       m_Config.options().printTimingStats());

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
    uint64_t hash = llvm::hash_combine(Path);
    // Check exclude list
    for (auto &I : LTOPatternList) {
      if (I->hasHash() && I->hashValue() == hash) {
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
  if (m_Config.options().hasLTO()) {
    if (MatchedPattern)
      return false;
  } else {
    // The file needs to be included, if
    // -flto option is not used and there is a match.
    if (!MatchedPattern)
      return false;
  }

  if (m_pModule->getPrinter()->isVerbose())
    m_Config.raise(diag::use_embedded_bitcode) << Path;

  // LTO is needed. Since Bitcode was chosen.
  m_pModule->setLTONeeded();

  llvm::StringRef LLVMBCContents =
      CurInputFile->getSlice(LLVMBCSection->offset(), LLVMBCSection->size());

  InputFile *OverrideBCFile = InputFile::CreateEmbedded(
      CurInput, LLVMBCContents, m_Config.getDiagEngine());

  ASSERT(OverrideBCFile->getKind() == InputFile::BitcodeFileKind,
         CurInput->decoratedPath());

  CurInput->overrideInputFile(OverrideBCFile);

  return true;
}

bool ObjectLinker::parseIncludeOrExcludeLTOfiles() {
  std::set<std::string> listFiles;
  // if -flto is used use --exclude-lto-filelist
  // if -flto is not used, use --include-lto-filelist
  if (m_Config.options().hasLTO())
    listFiles = m_Config.options().getExcludeLTOFiles();
  else
    listFiles = m_Config.options().getIncludeLTOFiles();

  for (const std::string &Name : listFiles) {
    std::unique_ptr<MemoryArea> List(new MemoryArea(Name));
    if (!List->Init(m_Config.getDiagEngine()))
      return false;
    llvm::StringRef buffer = List->getContents();
    while (!buffer.empty()) {
      std::pair<StringRef, StringRef> lineAndRest = buffer.split('\n');
      StringRef line = lineAndRest.first.trim();
      // Comment lines starts with #
      if (!line.empty() || !line.starts_with("#"))
        LTOPatternList.emplace_back(make<WildcardPattern>(line.str()));
      buffer = lineAndRest.second;
    }
  }
  return true;
}

bool ObjectLinker::provideGlobalSymbolAndContents(std::string Name, size_t Sz,
                                                  uint32_t Alignment) {
  LDSymbol *sym = m_pModule->getNamePool().findSymbol(Name);

  if (!sym || !sym->resolveInfo() || !sym->resolveInfo()->isUndef())
    return true;

  char *Buf = m_pModule->getUninitBuffer(Sz);
  ELFSection *inputSect = m_pModule->createInternalSection(
      Module::InternalInputType::GlobalDataSymbols, LDFileFormat::Regular,
      ".rodata.internal." + Name, llvm::ELF::SHT_PROGBITS, llvm::ELF::SHF_ALLOC,
      Alignment, 0);
  Fragment *F = make<RegionFragment>(llvm::StringRef(Buf, Sz), inputSect,
                                     Fragment::Type::Region, Alignment);
  inputSect->addFragmentAndUpdateSize(F);
  ELFObjectFile *EObj =
      llvm::dyn_cast<eld::ELFObjectFile>(inputSect->getInputFile());
  if (!EObj)
    return false;
  LayoutPrinter *P = m_pModule->getLayoutPrinter();
  if (P)
    P->recordFragment(EObj, inputSect, F);
  LDSymbol *Sym = m_pBuilder->AddSymbol(
      *EObj, Name, (ResolveInfo::Type)sym->resolveInfo()->type(),
      ResolveInfo::Define, ResolveInfo::Global, Sz, 0, inputSect,
      sym->resolveInfo()->visibility(), true, EObj->getSections().size(),
      EObj->getSymbols().size());
  EObj->addSymbol(Sym);
  return true;
}

bool ObjectLinker::setCommonSectionsFallbackToBSS() {
  ObjectFile *commonInput =
      llvm::dyn_cast<ObjectFile>(m_pModule->getCommonInternalInput());
  auto iter = std::find_if(commonInput->getSections().begin(),
                           commonInput->getSections().end(), [](Section *S) {
                             return llvm::isa<CommonELFSection>(S) &&
                                    !S->getOutputSection();
                           });
  // Do not proceed if there are no unassigned common sections.
  if (iter == commonInput->getSections().end())
    return true;
  SectionMap &sectionMap = m_pModule->getScript().sectionMap();
  OutputSectionEntry *outSecEntry = sectionMap.findOutputSectionEntry(".bss");
  ELFSection *outSection = nullptr;
  RuleContainer *rule = nullptr;

  if (outSecEntry)
    outSection = outSecEntry->getSection();
  else {
    outSection = m_pModule->createOutputSection(".bss", LDFileFormat::Regular,
                                                /*pType=*/0, /*pFlag=*/0,
                                                /*pAlign=*/0);
    outSecEntry = outSection->getOutputSection();
  }

  rule = outSecEntry->getLastRule();
  if (!rule)
    rule = outSecEntry->createDefaultRule(*m_pModule);

  for (auto S : commonInput->getSections()) {
    if (!llvm::isa<CommonELFSection>(S) || S->getOutputSection())
      continue;
    S->setOutputSection(outSecEntry);
    S->setMatchedLinkerScriptRule(rule);
  }
  return true;
}

bool ObjectLinker::setCopyRelocSectionsFallbackToBSS() {
  ObjectFile *copyRelocInput = llvm::dyn_cast<ObjectFile>(
      m_pModule->getInternalInput(Module::InternalInputType::CopyRelocSymbols));

  SectionMap &sectionMap = m_pModule->getScript().sectionMap();
  OutputSectionEntry *outSecEntry = sectionMap.findOutputSectionEntry(".bss");
  ELFSection *outSection = nullptr;
  RuleContainer *rule = nullptr;

  if (outSecEntry)
    outSection = outSecEntry->getSection();
  else {
    outSection = m_pModule->createOutputSection(".bss", LDFileFormat::Regular,
                                                /*pType=*/0, /*pFlag=*/0,
                                                /*pAlign=*/0);
    outSecEntry = outSection->getOutputSection();
  }

  rule = outSecEntry->getLastRule();
  if (!rule)
    rule = outSecEntry->createDefaultRule(*m_pModule);

  for (auto S : copyRelocInput->getSections()) {
    if (S->getOutputSection())
      continue;
    S->setOutputSection(outSecEntry);
    S->setMatchedLinkerScriptRule(rule);
  }
  return true;
}

void ObjectLinker::accountSymForSymStats(SymbolStats &symbolStats,
                                         const ResolveInfo &RI) {
  symbolStats.local += RI.isLocal();
  // NOTE: We are not counting absolute symbols here.
  symbolStats.global += RI.isGlobal();
  symbolStats.weak += RI.isWeak();
  symbolStats.hidden += RI.isHidden();
  // NOTE: We have to explicitly add isFile() here because
  // we set absolute property to false for STT_File symbols.
  symbolStats.absolute += RI.isAbsolute() || RI.isFile();
  symbolStats.protectedSyms += RI.isProtected();
  symbolStats.file += RI.isFile();
}

void ObjectLinker::accountSymForTotalSymStats(const ResolveInfo &RI) {
  accountSymForSymStats(m_TotalSymStats, RI);
}

void ObjectLinker::accountSymForDiscardedSymStats(const ResolveInfo &RI) {
  accountSymForSymStats(m_DiscardedSymStats, RI);
}

const ObjectLinker::SymbolStats &ObjectLinker::getTotalSymbolStats() const {
  return m_TotalSymStats;
}

const ObjectLinker::SymbolStats &ObjectLinker::getDiscardedSymbolStats() const {
  return m_DiscardedSymStats;
}

void ObjectLinker::reportPendingPluginRuleInsertions() const {
  const LinkerScript &script = m_pModule->getLinkerScript();
  const auto &pendingRuleInsertions = script.getPendingRuleInsertions();
  for (const auto &item : pendingRuleInsertions) {
    const plugin::LinkerWrapper *LW = item.first;
    for (const RuleContainer *R : item.second) {
      std::string ruleString;
      llvm::raw_string_ostream ss(ruleString);
      R->desc()->dumpSpec(ss);
      m_Config.raise(diag::warn_pending_rule_insertion)
          << ss.str() << LW->getPlugin()->getPluginName()
          << R->desc()->getOutputDesc().name();
    }
  }
}

void ObjectLinker::finalizeOutputSectionFlags(OutputSectionEntry *OSE) const {
  ELFSection *S = OSE->getSection();
  uint32_t outFlags = S->getFlags();
  auto outSectType = OSE->prolog().type();

  if (outSectType == OutputSectDesc::Type::DSECT ||
      outSectType == OutputSectDesc::Type::COPY ||
      outSectType == OutputSectDesc::Type::INFO ||
      outSectType == OutputSectDesc::Type::OVERLAY) {
    outFlags = S->getFlags();
    S->setFlags(outFlags & ~llvm::ELF::SHF_ALLOC);
  }
}

void ObjectLinker::collectEntrySections() const {
  const std::vector<InputFile *> &inputFiles = m_pModule->getObjectList();
  SectionMap &SM = m_pModule->getLinkerScript().sectionMap();
  std::vector<std::vector<ELFSection *>> inputToEntrySections(
      inputFiles.size());

  llvm::parallelFor(0, inputFiles.size(), [&](std::size_t i) {
    if (isPostLTOPhase() && inputFiles[i]->isBitcode())
      return;
    const ObjectFile *objFile = llvm::cast<ObjectFile>(inputFiles[i]);

    for (Section *S : objFile->getSections()) {
      ELFSection *ELFSect = llvm::dyn_cast<ELFSection>(S);
      if (!ELFSect)
        continue;
      RuleContainer *R = ELFSect->getMatchedLinkerScriptRule();

      if ((R && R->isEntry()) ||
          ELFSect->name().find("@") != llvm::StringRef::npos) {
        inputToEntrySections[i].push_back(ELFSect);
        continue;
      }

      for (auto entry : m_Config.targets().getEntrySections()) {
        if (SM.matched(*entry, ELFSect->name(), ELFSect->getSectionHash())) {
          inputToEntrySections[i].push_back(ELFSect);
          break;
        }
      }
    }
  });

  for (const auto &entrySections : inputToEntrySections) {
    for (ELFSection *ELFSect : entrySections) {
      m_Config.raise(diag::keeping_section)
          << ELFSect->name()
          << ELFSect->originalInput()->getInput()->decoratedPath();
      SM.addEntrySection(ELFSect);
    }
  }
}
