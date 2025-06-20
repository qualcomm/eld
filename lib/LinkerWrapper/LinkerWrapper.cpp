//===- LinkerWrapper.cpp---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/PluginAPI/LinkerWrapper.h"
#include "eld/Config/Version.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Diagnostics/DiagnosticPrinter.h"
#include "eld/Fragment/Fragment.h"
#include "eld/Input/BitcodeFile.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/Object/ObjectLinker.h"
#include "eld/Object/OutputSectionEntry.h"
#include "eld/Object/SectionMap.h"
#include "eld/Plugin/PluginManager.h"
#include "eld/PluginAPI/DWARF.h"
#include "eld/PluginAPI/DiagnosticEntry.h"
#include "eld/PluginAPI/Diagnostics.h"
#include "eld/PluginAPI/Expected.h"
#include "eld/PluginAPI/LinkerScript.h"
#include "eld/PluginAPI/PluginADT.h"
#include "eld/PluginAPI/SmallJSON.h"
#include "eld/PluginAPI/TarWriter.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Readers/Relocation.h"
#include "eld/Script/Plugin.h"
#include "eld/Support/DynamicLibrary.h"
#include "eld/Support/MsgHandling.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "eld/Target/ELFSegmentFactory.h"
#include "llvm/DebugInfo/DWARF/DWARFContext.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/GlobPattern.h"
#include "llvm/Support/Memory.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>
#include <string>

#define RETURN_INVALID_LINK_STATE_ERR(validStates)                             \
  return std::make_unique<DiagnosticEntry>(                                    \
      Diag::error_invalid_link_state,                                          \
      std::vector<std::string>{std::string(getCurrentLinkStateAsStr()),        \
                               std::string(LLVM_PRETTY_FUNCTION),              \
                               std::string(validStates)});

using namespace eld;
using namespace eld::plugin;

LinkerWrapper::LinkerWrapper(eld::Plugin *P, eld::Module &M)
    : m_Module(M), m_Plugin(P), m_DiagEngine(M.getConfig().getDiagEngine()) {}

LinkerWrapper::~LinkerWrapper() {}

void LinkerWrapper::RequestGarbageCollection() {
  m_Module.getIRBuilder()->requestGarbageCollection();
}

eld::Expected<std::vector<Use>> LinkerWrapper::getUses(Chunk &C) {
  eld::Fragment *F = C.getFragment();
  std::vector<Use> Uses;
  for (auto &relocation : F->getOwningSection()->getRelocations())
    Uses.push_back(Use(relocation));
  return Uses;
}

eld::Expected<void>
LinkerWrapper::setOutputSection(Section &S, const std::string &OutputSection,
                                const std::string &Annotation) {
  m_Module.getScript().addSectionOverride(this, &m_Module, S.getSection(),
                                          OutputSection, Annotation);
  return {};
}

/// Mark the symbol as preserved for garbage collection.
eld::Expected<void> LinkerWrapper::setPreserveSymbol(Symbol Symbol) {
  eld::ResolveInfo *R = Symbol.getSymbol();
  assert(R->isBitCode());
  if (!R || !R->outSymbol())
    return std::make_unique<DiagnosticEntry>(
        DiagnosticEntry(Diag::error_invalid_symbol, {}));

  R->shouldPreserve(true);
  R->outSymbol()->setShouldIgnore(false);

  return {};
}

eld::Expected<std::vector<Use>> LinkerWrapper::getUses(Section &S) {
  std::vector<Use> Uses;
  eld::ELFSection *E = llvm::dyn_cast<eld::ELFSection>(S.getSection());
  if (!E)
    return Uses;
  for (auto &relocation : E->getRelocations())
    Uses.push_back(Use(relocation));
  return Uses;
}

eld::Expected<plugin::Symbol> LinkerWrapper::addSymbol(
    InputFile InputFile, const std::string &Name,
    plugin::Symbol::Binding Binding, plugin::Section InputSection,
    plugin::Symbol::Kind Kind, plugin::Symbol::Visibility Visibility,
    unsigned Type, uint64_t Size) {

  auto GetSymbolKind = [](plugin::Symbol::Kind SymbolKind) {
    switch (SymbolKind) {
    case plugin::Symbol::Undefined:
      return ResolveInfo::Undefined;
    case plugin::Symbol::Define:
      return ResolveInfo::Define;
    case plugin::Symbol::Common:
      return ResolveInfo::Common;
    }
  };

  auto GetSymbolBinding = [](plugin::Symbol::Binding SymbolBinding) {
    switch (SymbolBinding) {
    case plugin::Symbol::Global:
      return ResolveInfo::Global;
    case plugin::Symbol::Weak:
      return ResolveInfo::Weak;
    case plugin::Symbol::Local:
      return ResolveInfo::Local;
    }
  };

  auto GetSymbolVisibility = [](plugin::Symbol::Visibility SymbolVisibility)
      -> ResolveInfo::Visibility {
    switch (SymbolVisibility) {
    case plugin::Symbol::Default:
      return ResolveInfo::Default;
    case plugin::Symbol::Internal:
      return ResolveInfo::Internal;
    case plugin::Symbol::Hidden:
      return ResolveInfo::Hidden;
    case plugin::Symbol::Protected:
      return ResolveInfo::Protected;
    }
  };

  eld::BitcodeFile *BitcodeFile =
      llvm::dyn_cast<eld::BitcodeFile>(InputFile.getInputFile());
  if (!BitcodeFile)
    return std::make_unique<DiagnosticEntry>(
        Diag::error_invalid_input_file,
        std::vector<std::string>{InputFile.getFileName()});

  // We cast from an unsigned value to ResolveInfo::Type, assuming the
  // values represent ELF types.
  // TODO: Index is unused.
  LDSymbol *S = m_Module.addSymbolFromBitCode(
      *BitcodeFile, Name, ResolveInfo::Type(Type), GetSymbolKind(Kind),
      GetSymbolBinding(Binding), Size, GetSymbolVisibility(Visibility), 0);

  if (InputSection && S->resolveInfo())
    BitcodeFile->setInputSectionForSymbol(*S->resolveInfo(),
                                          *InputSection.getSection());

  return plugin::Symbol(S->resolveInfo());
}

eld::Expected<plugin::OutputSection>
LinkerWrapper::getOutputSection(Section &S) const {
  eld::ELFSection *ES = llvm::dyn_cast<eld::ELFSection>(S.getSection());
  if (!ES)
    return std::make_unique<DiagnosticEntry>(
        DiagnosticEntry(Diag::error_no_output_section, {S.getName()}));
  return plugin::OutputSection(ES->getOutputSection());
}

eld::Expected<std::unique_ptr<const uint8_t[]>>
LinkerWrapper::getOutputSectionContents(OutputSection &O) const {
  if (getState() <= LinkerWrapper::CreatingSections)
    return std::make_unique<DiagnosticEntry>(
        Diag::error_invalid_link_state,
        std::vector<std::string>{std::string(getCurrentLinkStateAsStr()),
                                 __FUNCTION__,
                                 "CreatingSegments, AfterLayout"});
  if (O.getOutputSection()->getSection()->isNoBits())
    return std::make_unique<DiagnosticEntry>(
        Diag::error_nobits_unsupported, std::vector<std::string>{O.getName()});
  auto Data = std::make_unique<uint8_t[]>(O.getSize());
  eld::MemoryRegion Region(Data.get(), O.getSize());
  for (LinkerScriptRule &R : O.getLinkerScriptRules()) {
    for (Chunk &C : R.getChunks()) {
      if (C.isNoBits())
        continue;
      C.getFragment()->emit(Region, m_Module);
    }
  }
  // The section could have a fill expression
  m_Module.getBackend()->maybeFillRegion(O.getOutputSection(), Region);
  return std::move(Data);
}

eld::Expected<plugin::OutputSection>
LinkerWrapper::getOutputSection(const std::string &OS) {
  eld::SectionMap &SM = m_Module.getScript().sectionMap();
  eld::ELFSection *S = SM.find(OS);
  if (!S)
    return std::make_unique<DiagnosticEntry>(
        DiagnosticEntry(Diag::error_output_section_not_found, {OS}));
  return plugin::OutputSection(S->getOutputSection());
}

eld::Expected<void> LinkerWrapper::finishAssignOutputSections() {
  if (!m_Module.getLinkerScript().hasPendingSectionOverride(this))
    reportDiag(plugin::Diagnostic::warn_no_section_overrides_found());
  m_Module.getLinker()->getObjLinker()->finishAssignOutputSections(this);
  return {};
}

eld::Expected<void> LinkerWrapper::reassignVirtualAddresses() {
  if (getState() != State::CreatingSegments)
    RETURN_INVALID_LINK_STATE_ERR("CreatingSegments");
  m_Module.getBackend()->createScriptProgramHdrs();
  return {};
}

eld::Expected<std::vector<Segment>> LinkerWrapper::getSegmentTable() const {
  if (getState() < LinkerWrapper::AfterLayout)
    return std::make_unique<DiagnosticEntry>(
        Diag::error_invalid_link_state,
        std::vector<std::string>{std::string(getCurrentLinkStateAsStr()),
                                 LLVM_PRETTY_FUNCTION,
                                 "'CreatingSegments, AfterLayout'"});
  std::vector<Segment> Segments;
  for (auto *S : m_Module.getBackend()->elfSegmentTable())
    Segments.push_back(Segment(S));
  return Segments;
}

eld::Expected<plugin::DynamicLibrary>
LinkerWrapper::loadLibrary(const std::string &LibraryName) {
  auto L = m_Plugin->loadLibrary(LibraryName);
  if (!L)
    return std::move(L.error());
  DynamicLibrary Lib{L->first, L->second};
  return Lib;
}

eld::Expected<void *>
LinkerWrapper::getFunction(void *LibraryHandle,
                           const std::string &FunctionName) const {
  void *Func = eld::DynamicLibrary::GetFunction(LibraryHandle, FunctionName);
  if (!Func)
    return std::make_unique<DiagnosticEntry>(
        Diag::unable_to_find_func,
        std::vector<std::string>{"", FunctionName,
                                 eld::DynamicLibrary::GetLastError()});
  return Func;
}

eld::Expected<void> LinkerWrapper::applyRelocations(uint8_t *Buf) {
  if (getState() < LinkerWrapper::AfterLayout)
    RETURN_INVALID_LINK_STATE_ERR("CreatingSegments, AfterLayout");
  m_Module.getLinker()->getObjectLinker()->syncRelocations(Buf);
  return {};
}

eld::Expected<void> LinkerWrapper::doRelocation() {
  if (getState() != LinkerWrapper::CreatingSegments)
    RETURN_INVALID_LINK_STATE_ERR("CreatingSegments");
  /// FIXME: we can report better errors if ObjectLinker::relocation() returned
  /// eld::Expected
  if (!m_Module.getLinker()->getObjectLinker()->relocation(
          m_Module.getConfig().options().emitRelocs()))
    return std::make_unique<DiagnosticEntry>(Diag::error_relocations_plugin);
  return {};
}

eld::Expected<void> LinkerWrapper::addChunkToOutput(Chunk C) {
  if (getState() < State::CreatingSections)
    RETURN_INVALID_LINK_STATE_ERR(
        "CreatingSections, CreatingSegments, AfterLayout");

  auto ExpMapping = getOutputSectionAndRule(C.getSection());
  if (!ExpMapping)
    return std::move(ExpMapping.error());

  ObjectBuilder Builder(m_Module.getConfig(), m_Module);

  eld::ELFSection *S = llvm::cast<ELFSection>(C.getSection().getSection());
  auto [OutputSection, Rule] = *ExpMapping;

  S->setOutputSection(OutputSection.getOutputSection());
  S->setMatchedLinkerScriptRule(Rule.getRuleContainer());
  Builder.moveSection(S, Rule.getRuleContainer()->getSection());

  m_Module.getLinker()->getObjLinker()->createOutputSection(
      Builder, OutputSection.getOutputSection(), true);

  if (auto *layoutInfo = m_Module.getLayoutInfo()) {
    auto *Op = make<AddChunkPluginOp>(this, Rule.getRuleContainer(),
                                      C.getFragment(), "");
    layoutInfo->recordAddChunk(this, Op);
  }
  return {};
}

eld::Expected<void> LinkerWrapper::resetOffset(OutputSection O) {
  if (getState() < LinkerWrapper::AfterLayout)
    RETURN_INVALID_LINK_STATE_ERR("CreatingSegments, AfterLayout");
  if (auto *layoutInfo = m_Module.getLayoutInfo()) {
    auto OldOffset = O.getOffset();
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(OldOffset);
    auto *Op =
        eld::make<ResetOffsetPluginOp>(this, O.getOutputSection(), *OldOffset);
    layoutInfo->recordResetOffset(this, Op);
  }
  O.getOutputSection()->getSection()->setNoOffset();
  return {};
}

eld::Expected<std::pair<OutputSection, LinkerScriptRule>>
LinkerWrapper::getOutputSectionAndRule(Section S) {
  if (getState() < State::CreatingSections)
    RETURN_INVALID_LINK_STATE_ERR(
        "CreatingSections, CreatingSegments, AfterLayout");

  InputFile F = S.getInputFile();

  auto *Script = getLinkerScript().getLinkerScript();
  auto *Input = F.getInputFile()->getInput();

  auto [Section, Rule] = Script->sectionMap().findIn(
      Script->sectionMap().begin(), Input->getResolvedPath().native(),
      *llvm::cast<ELFSection>(S.getSection()), F.isArchive(), Input->getName(),
      S.getSection()->sectionNameHash(), Input->getResolvedPathHash(),
      Input->getArchiveMemberNameHash(),
      m_Module.getConfig().options().getScriptOption() ==
          GeneralOptions::MatchGNU);

  ELFSection *inputELFSect = llvm::dyn_cast<ELFSection>(S.getSection());

  if (!Section && inputELFSect) {
    ELFSection *outSect = m_Module.createOutputSection(
        inputELFSect->name().str(), inputELFSect->getKind(),
        inputELFSect->getType(), inputELFSect->getFlags(),
        inputELFSect->getAddrAlign());
    Section = outSect->getOutputSection();
    Rule = Section->createDefaultRule(m_Module);
  }

  if (!Section)
    return std::make_unique<DiagnosticEntry>(
        DiagnosticEntry(Diag::error_invalid_input_section, {S.getName()}));

  return std::make_pair(OutputSection(Section), LinkerScriptRule(Rule));
}

eld::Expected<void> LinkerWrapper::linkSections(OutputSection A,
                                                OutputSection B) const {
  if (getState() != LinkerWrapper::CreatingSections &&
      getState() != LinkerWrapper::CreatingSegments)
    RETURN_INVALID_LINK_STATE_ERR("CreatingSections, CreatingSegments");
  m_Module.getBackend()->pluginLinkSections(A.getOutputSection(),
                                            B.getOutputSection());
  return {};
}

eld::Expected<plugin::Symbol>
LinkerWrapper::getSymbol(const std::string &Sym) const {
  eld::ResolveInfo *Info = m_Module.getNamePool().findInfo(Sym);
  if (!Info)
    return std::make_unique<DiagnosticEntry>(
        DiagnosticEntry(Diag::error_symbol_not_found, {Sym}));
  return plugin::Symbol(Info);
}

bool LinkerWrapper::matchPattern(const std::string &Pattern,
                                 const std::string &Name) const {
  auto E = llvm::GlobPattern::create(Pattern);
  if (!E) {
    (void)(E.takeError());
    return false;
  }
  return E->match(Name);
}

LinkerWrapper::State LinkerWrapper::getState() const {
  return m_Module.getState();
}

bool LinkerWrapper::isPostLTOPhase() const {
  return m_Module.getLinker()->getObjectLinker()->isPostLTOPhase();
}

void LinkerWrapper::setLinkerFatalError() { m_Module.setFailure(true); }

eld::Expected<plugin::Section> LinkerWrapper::createBitcodeSection(
    const std::string &Name, plugin::BitcodeFile BitcodeFile, bool IsInternal) {
  return plugin::Section(m_Module.createBitcodeSection(
      Name, BitcodeFile.getBitcodeFile(), IsInternal));
}

void LinkerWrapper::setSectionName(plugin::Section S, const std::string &Name) {
  if (S)
    S.getSection()->setName(Name);
}

void LinkerWrapper::setRuleMatchingInput(plugin::Section S,
                                         plugin::InputFile I) {
  if (eld::ELFSection *ELFSection =
          llvm::dyn_cast<eld::ELFSection>(S.getSection()))
    ELFSection->setOldInputFile(I.getInputFile());
}

eld::Expected<void>
LinkerWrapper::addReferencedSymbol(plugin::Section ReferencingSection,
                                   plugin::Symbol ReferencedSymbol) {
  if (!ReferencingSection || !ReferencedSymbol)
    return std::make_unique<DiagnosticEntry>(Diag::error_invalid_argument);

  m_Module.addReferencedSymbol(*ReferencingSection.getSection(),
                               *ReferencedSymbol.getSymbol());
  return {};
}

void LinkerWrapper::runGarbageCollection(const std::string &Phase) {
  if (!m_Module.getIRBuilder()->shouldRunGarbageCollection()) {
    m_DiagEngine->raise(Diag::error_call_gc_without_request);
    return;
  }

  m_Module.getLinker()->getObjLinker()->runGarbageCollection(Phase, true);
}

eld::Expected<void> LinkerWrapper::addSymbolToChunk(Chunk &C,
                                                    const std::string &Symbol,
                                                    uint64_t Val) {
  if (!C.getFragment())
    return std::make_unique<DiagnosticEntry>(
        DiagnosticEntry(Diag::error_failed_to_add_sym_to_chunk, {Symbol}));
  m_Module.addSymbolCreatedByPluginToFragment(C.getFragment(), Symbol, Val,
                                              getPlugin());
  return {};
}

eld::Expected<Chunk> LinkerWrapper::createPaddingChunk(uint32_t Alignment,
                                                       size_t PaddingSize) {
  eld::Fragment *F = m_Module.createPluginFillFragment(getPlugin()->getName(),
                                                       Alignment, PaddingSize);
  return Chunk(F);
}

eld::Expected<Chunk> LinkerWrapper::createCodeChunk(const std::string &Name,
                                                    uint32_t Alignment,
                                                    const char *Buf,
                                                    size_t Sz) {
  eld::Fragment *F = m_Module.createPluginCodeFragment(
      getPlugin()->getName(), Name, Alignment, Buf, Sz);
  return Chunk(F);
}

eld::Expected<Chunk> LinkerWrapper::createDataChunk(const std::string &Name,
                                                    uint32_t Alignment,
                                                    const char *Buf,
                                                    size_t Sz) {
  eld::Fragment *F = m_Module.createPluginDataFragment(
      getPlugin()->getName(), Name, Alignment, Buf, Sz);
  return Chunk(F);
}

eld::Expected<Chunk> LinkerWrapper::createDataChunkWithCustomName(
    const std::string &Name, uint32_t Alignment, const char *Buf, size_t Sz) {
  eld::Fragment *F = m_Module.createPluginDataFragmentWithCustomName(
      getPlugin()->getName(), Name, Alignment, Buf, Sz);
  return Chunk(F);
}

eld::Expected<Chunk> LinkerWrapper::createBSSChunk(const std::string &Name,
                                                   uint32_t Alignment,
                                                   size_t Sz) {
  eld::Fragment *F = m_Module.createPluginBSSFragment(getPlugin()->getName(),
                                                      Name, Alignment, Sz);
  return Chunk(F);
}

eld::Expected<Chunk> LinkerWrapper::createChunkWithCustomName(
    const std::string &Name, size_t sectType, size_t sectFlags,
    uint32_t Alignment, const char *Buf, size_t Sz) {
  eld::Fragment *F = m_Module.createPluginFragmentWithCustomName(
      Name, sectType, sectFlags, Alignment, Buf, Sz);
  return Chunk(F);
}

eld::Expected<void> LinkerWrapper::replaceSymbolContent(plugin::Symbol S,
                                                        const uint8_t *Buf,
                                                        size_t Sz) {
  eld::LDSymbol *Sym = S.getSymbol()->outSymbol();
  if (!Sym)
    return std::make_unique<DiagnosticEntry>(
        DiagnosticEntry(Diag::error_invalid_symbol, {}));

  if (!Sym->hasFragRefSection())
    return std::make_unique<DiagnosticEntry>(
        DiagnosticEntry(Diag::error_symbol_has_no_chunk, {S.getName()}));

  eld::FragmentRef *FragRef = Sym->fragRef();

  if (FragRef->getOutputELFSection()->isBSS())
    return std::make_unique<DiagnosticEntry>(
        DiagnosticEntry(Diag::error_chunk_is_bss, {S.getName()}));

  eld::RegionFragment *R = llvm::dyn_cast<eld::RegionFragment>(FragRef->frag());

  if (!R)
    return std::make_unique<DiagnosticEntry>(
        DiagnosticEntry(Diag::error_chunk_is_bss, {S.getName()}));

  if (Sz > Sym->size())
    return std::make_unique<DiagnosticEntry>(
        DiagnosticEntry(Diag::error_symbol_is_small,
                        {S.getName(), std::to_string(Sz - Sym->size())}));

  m_Module.replaceFragment(FragRef, Buf, Sz);

  return {};
}

eld::Expected<void> LinkerWrapper::addChunk(const plugin::LinkerScriptRule &R,
                                            const Chunk &C,
                                            const std::string &Annotation) {
  /// Rules contain chunks only in CreatingSections state!
  if (getState() != State::CreatingSections)
    RETURN_INVALID_LINK_STATE_ERR("CreatingSections");

  auto expAddChunk = m_Module.getScript().addChunkOp(
      this, &m_Module, R.getRuleContainer(), C.getFragment(), Annotation);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expAddChunk);
  return {};
}

eld::Expected<void>
LinkerWrapper::removeChunk(const plugin::LinkerScriptRule &R, const Chunk &C,
                           const std::string &Annotation) {
  /// Rules contain chunks only in CreatingSections state!
  if (getState() != State::CreatingSections)
    RETURN_INVALID_LINK_STATE_ERR("CreatingSections");
  auto expRemoveChunk = m_Module.getScript().removeChunkOp(
      this, &m_Module, R.getRuleContainer(), C.getFragment(), Annotation);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expRemoveChunk);
  return {};
}

eld::Expected<void>
LinkerWrapper::updateChunks(const plugin::LinkerScriptRule &R,
                            const std::vector<Chunk> &Chunks,
                            const std::string &Annotation) {
  /// Rules contain chunks only in CreatingSections state!
  if (getState() != State::CreatingSections)
    RETURN_INVALID_LINK_STATE_ERR("CreatingSections");
  std::vector<eld::Fragment *> Fragments;
  for (auto &C : Chunks)
    Fragments.push_back(C.getFragment());
  auto expUpdateChunks = m_Module.getScript().updateChunksOp(
      this, &m_Module, R.getRuleContainer(), Fragments, Annotation);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expUpdateChunks);
  return {};
}

std::string LinkerWrapper::getRepositoryVersion() const {
  return "ELD version:" + eld::getELDRepositoryVersion() +
         " LLVM version:" + eld::getLLVMRepositoryVersion();
}

void LinkerWrapper::recordPluginData(uint32_t Key, void *Data,
                                     const std::string &Annotation) {
  m_Module.recordPluginData(getPlugin()->getName(), Key, Data, Annotation);
}

std::vector<plugin::PluginData>
LinkerWrapper::getPluginData(const std::string &PluginName) {
  std::vector<plugin::PluginData> Data;
  std::vector<eld::PluginData *> PD = m_Module.getPluginData(PluginName);
  if (!PD.size())
    return Data;
  for (auto &PData : PD)
    Data.push_back(plugin::PluginData(PData));
  return Data;
}

llvm::Timer *LinkerWrapper::CreateTimer(const std::string &Name,
                                        const std::string &Description,
                                        bool IsEnabled) {
  if (!IsEnabled)
    return nullptr;
  return m_Module.getScript().getTimer(
      Name, Description, getPlugin()->getPluginName() + " USER PROFILE",
      getPlugin()->getDescription() + " USER PROFILE");
}

eld::Expected<void> LinkerWrapper::registerReloc(uint32_t RelocType,
                                                 const std::string &Name) {
  bool b = m_Module.getScript().registerReloc(this, RelocType, Name);
  if (b)
    return {};
  return std::make_unique<DiagnosticEntry>(DiagnosticEntry(
      Diag::error_failed_to_register_reloc, {std::to_string(RelocType)}));
}

RelocationHandler LinkerWrapper::getRelocationHandler() const {
  return RelocationHandler(m_Module.getBackend()->getRelocator());
}

size_t LinkerWrapper::getPluginThreadCount() const {
  const eld::LinkerConfig &Config = m_Module.getConfig();
  return Config.options().numThreads();
}

bool LinkerWrapper::isMultiThreaded() const {
  const eld::LinkerConfig &Config = m_Module.getConfig();
  if (Config.options().threadsEnabled()) {
    uint32_t NumThreads = Config.options().numThreads();
    if (NumThreads > 1)
      return true;
  }
  return false;
}

plugin::LinkerScript LinkerWrapper::getLinkerScript() {
  plugin::LinkerScript LinkerScript(nullptr);
  return plugin::LinkerScript(&m_Module.getLinkerScript());
}

std::string LinkerWrapper::getFileContents(std::string FileName) {
  if (m_Module.getConfig().options().hasMappingFile())
    FileName = m_Module.getConfig().getFileFromHash(FileName);
  if (!llvm::sys::fs::exists(FileName))
    return "";
  if (m_Module.getOutputTarWriter())
    m_Module.getOutputTarWriter()->createAndAddConfigFile(FileName, FileName);
  eld::MemoryArea buf(FileName);
  bool success = buf.Init(m_DiagEngine);
  std::string ErrorMsg = "";
  if (!success) {
    m_DiagEngine->raise(Diag::fatal_cannot_read_input) << FileName;
    return "";
  }
  return buf.getContents().str();
}

eld::Expected<std::string>
LinkerWrapper::findConfigFile(const std::string &FileName) const {
  eld::SearchDirs Directories = m_Module.getConfig().directories();
  const eld::sys::fs::Path *P = Directories.findFile(
      "plugin configuration INI file", FileName, getPlugin()->getPluginName());
  if (P)
    return P->getFullPath();
  return std::make_unique<DiagnosticEntry>(
      DiagnosticEntry(Diag::error_finding_plugin_config, {FileName}));
}

/// Read the contents of a file in .ini format
/// \return eld::Expected<INIFile> containing an INI file object if
/// initialization and reading of the INI file was successful; Otherwise return
/// a diagnostic object describing the error.
eld::Expected<INIFile> LinkerWrapper::readINIFile(std::string FileName) {
  if (m_Module.getOutputTarWriter() && llvm::sys::fs::exists(FileName))
    m_Module.getOutputTarWriter()->createAndAddConfigFile(FileName, FileName);
  if (m_Module.getConfig().options().hasMappingFile())
    FileName = m_Module.getConfig().getHashFromFile(FileName);
  return INIFile::Create(FileName);
}

eld::Expected<void>
LinkerWrapper::writeINIFile(INIFile &INI, const std::string &OutputPath) const {
  auto writer = std::make_unique<eld::INIWriter>();
  for (std::string sectionName : INI.getSections()) {
    for (std::pair<std::string, std::string> kv : INI.getSection(sectionName)) {
      (*writer)[sectionName][kv.first] = kv.second;
    }
  }
  std::error_code ec = writer->writeFile(OutputPath);
  INI.setLastError(ec ? INIFile::WriteError : INIFile::Success);
  if (ec)
    return std::make_unique<DiagnosticEntry>(
        DiagnosticEntry(Diag::error_write_file, {OutputPath, ec.message()}));
  return {};
}

/// \returns eld::Expected<TarWriter>
eld::Expected<TarWriter>
LinkerWrapper::getTarWriter(const std::string &Name) const {
  llvm::Expected<std::unique_ptr<llvm::TarWriter>> Tar =
      llvm::TarWriter::create(Name, llvm::sys::path::parent_path(Name));
  LLVMEXP_RETURN_DIAGENTRY_IF_ERROR(Tar);
  return TarWriter(std::move(*Tar));
}

/// \returns if Timing is enabled for the plugin.
bool LinkerWrapper::isTimingEnabled() const {
  if (!m_Plugin)
    return false;
  return m_Plugin->isTimingEnabled();
}

/// \returns the list of input files visited by the Linker.
std::vector<plugin::InputFile> LinkerWrapper::getInputFiles() const {
  std::vector<plugin::InputFile> InputFiles;
  for (eld::InputFile *I : m_Module.getObjectList()) {
    if (I->isInternal())
      continue;
    InputFiles.push_back(plugin::InputFile(I));
  }
  return InputFiles;
}

std::string eld::plugin::LinkerWrapper::getLinkerVersion() const {
  return eld::getELDVersion().str();
}

plugin::LinkerWrapper::LinkMode LinkerWrapper::getLinkMode() const {
  const eld::LinkerConfig &Config = m_Module.getConfig();

  if (Config.isBuildingExecutable()) {
    if (Config.options().isPIE())
      return plugin::LinkerWrapper::LinkMode::PIE;
    if (Config.isCodeDynamic() || Config.options().forceDynamic())
      return plugin::LinkerWrapper::LinkMode::DynamicExecutable;
    return plugin::LinkerWrapper::LinkMode::StaticExecutable;
  }
  if (Config.isLinkPartial())
    return plugin::LinkerWrapper::LinkMode::PartialLink;
  if (Config.codeGenType() == eld::LinkerConfig::DynObj)
    return plugin::LinkerWrapper::LinkMode::SharedLibrary;
  return plugin::LinkerWrapper::LinkMode::UnknownLinkMode;
}

// --- DWARF support:
eld::Expected<DWARFInfo>
LinkerWrapper::getDWARFInfoForInputFile(plugin::InputFile F,
                                        bool is32bit) const {
  eld::InputFile *IF = F.getInputFile();
  eld::ELFObjectFile *EObj = llvm::dyn_cast<eld::ELFObjectFile>(IF);
  if (!EObj)
    return std::make_unique<DiagnosticEntry>(
        DiagnosticEntry(Diag::error_invalid_input_file, {F.getFileName()}));
  if (!EObj->hasDWARFContext())
    EObj->createDWARFContext(is32bit);
  llvm::DWARFContext *DC = EObj->getDWARFContext();
  if (!DC)
    return std::make_unique<DiagnosticEntry>(DiagnosticEntry(
        Diag::error_dwarf_context_not_available, {F.getFileName()}));
  return DWARFInfo(DC);
}

eld::Expected<void>
LinkerWrapper::deleteDWARFInfoForInputFile(plugin::InputFile &F) const {
  eld::InputFile *IF = F.getInputFile();
  eld::ELFObjectFile *EObj = llvm::dyn_cast<eld::ELFObjectFile>(IF);
  // FIXME: Maybe return an error here?
  if (!EObj || !EObj->hasDWARFContext())
    return {};
  EObj->deleteDWARFContext();
  return {};
}

//--- JSON:
bool LinkerWrapper::writeSmallJSONFile(const std::string &FileName,
                                       SmallJSONValue &V) const {
  auto &X = V.str();
  std::error_code EC;
  llvm::raw_fd_ostream OS(FileName, EC);
  if (EC) {
    m_DiagEngine->raise(Diag::unable_to_write_json_file)
        << FileName << EC.message();
    return false;
  }
  OS.reserveExtraSpace(X.size());
  OS << X;
  return true;
}

bool LinkerWrapper::is32Bits() const {
  return m_Module.getConfig().targets().is32Bits();
}

bool LinkerWrapper::is64Bits() const {
  return m_Module.getConfig().targets().is64Bits();
}

char *LinkerWrapper::getUninitBuffer(size_t S) const {
  // FIXME: Raise plugin_request_memory diagnostic!
  return m_Module.getUninitBuffer(S);
}

eld::Expected<void> LinkerWrapper::resetSymbol(plugin::Symbol S, Chunk C) {
  bool b = m_Module.resetSymbol(S.getSymbol(), C.getFragment());
  if (!b)
    return std::make_unique<DiagnosticEntry>(
        DiagnosticEntry(Diag::error_failed_to_reset_symbol, {S.getName()}));
  return {};
}

eld::Expected<Use> LinkerWrapper::createAndAddUse(Chunk C, off_t Offset,
                                                  uint32_t RelocationType,
                                                  plugin::Symbol S,
                                                  int64_t Addend) {
  Use ChunkUse(nullptr);
  eld::Relocation *relocation = m_Module.getIRBuilder()->createRelocation(
      m_Module.getBackend()->getRelocator(), *C.getFragment(), RelocationType,
      *S.getSymbol()->outSymbol(), Offset, Addend);
  C.getFragment()->getOwningSection()->addRelocation(relocation);
  return Use(relocation);
}

/// Return the value of the relocation target
eld::Expected<void> LinkerWrapper::getTargetDataForUse(Use &U,
                                                       uint64_t &Data) const {
  auto *R = U.getRelocation();
  if (!R)
    return std::make_unique<DiagnosticEntry>(
        DiagnosticEntry(Diag::error_invalid_use));
  if (m_Module.getRelocationData(R, Data))
    return {};
  Data = R->target();
  return {};
}

eld::Expected<void> LinkerWrapper::setTargetDataForUse(Use &U, uint64_t Data) {
  auto *R = U.getRelocation();
  if (!R)
    return std::make_unique<DiagnosticEntry>(
        DiagnosticEntry(Diag::error_invalid_use));
  m_Module.setRelocationData(R, Data);
  if (LayoutInfo *printer = m_Module.getLayoutInfo()) {
    auto *Op = eld::make<RelocationDataPluginOp>(this, R);
    printer->recordRelocationData(this, Op);
  }
  return {};
}

eld::Expected<uint32_t> LinkerWrapper::getImageLayoutChecksum() const {
  uint64_t Hash = 0;
  // FIXME: Return error here!
  if (getState() != LinkerWrapper::AfterLayout)
    return 0;
  Hash = m_Module.getImageLayoutChecksum();
  return Hash & 0xFFFFFFFF;
}

std::string LinkerWrapper::getOutputFileName() const {
  return m_Module.getConfig().options().outputFileName();
}

eld::Expected<plugin::LinkerScriptRule>
LinkerWrapper::createLinkerScriptRule(plugin::OutputSection S,
                                      const std::string &Annotation) {
  if (Annotation.empty())
    return plugin::LinkerScriptRule(nullptr);
  eld::InputFile *I = m_Module.getInternalInput(eld::Module::Plugin);
  plugin::LinkerScriptRule rule(S.getOutputSection()->createRule(
      m_Module, m_Plugin->getName() + Annotation, I));
  eld::LinkerScript &script = m_Module.getLinkerScript();
  script.addPendingRuleInsertion(this, rule.getRuleContainer());
  return rule;
}

eld::Expected<void>
LinkerWrapper::insertAfterRule(plugin::OutputSection O,
                               plugin::LinkerScriptRule Rule,
                               plugin::LinkerScriptRule RuleToAdd) {
  eld::LinkerScript &script = m_Module.getLinkerScript();
  bool inserted = O.getOutputSection()->insertAfterRule(
      Rule.getRuleContainer(), RuleToAdd.getRuleContainer());
  if (!inserted)
    return std::make_unique<DiagnosticEntry>(
        DiagnosticEntry(Diag::error_failed_to_insert_rule, {}));
  script.removePendingRuleInsertion(this, RuleToAdd.getRuleContainer());
  return {};
}

eld::Expected<void>
LinkerWrapper::insertBeforeRule(plugin::OutputSection O,
                                plugin::LinkerScriptRule Rule,
                                plugin::LinkerScriptRule RuleToAdd) {
  eld::LinkerScript &script = m_Module.getLinkerScript();
  bool inserted = O.getOutputSection()->insertBeforeRule(
      Rule.getRuleContainer(), RuleToAdd.getRuleContainer());
  if (!inserted)
    return std::make_unique<DiagnosticEntry>(
        DiagnosticEntry(Diag::error_failed_to_insert_rule, {}));
  script.removePendingRuleInsertion(this, RuleToAdd.getRuleContainer());
  return {};
}

void LinkerWrapper::removeSymbolTableEntry(plugin::Symbol S) {
  m_Module.getBackend()->markSymbolForRemoval(S.getSymbol());
  m_Module.getScript().removeSymbolOp(this, &m_Module, S.getSymbol());
}

eld::Expected<std::vector<plugin::Symbol>>
LinkerWrapper::getAllSymbols() const {
  if (m_Module.getSymbols().empty())
    return std::vector<plugin::Symbol>{};
  std::vector<plugin::Symbol> Symbols;
  for (ResolveInfo *S : m_Module.getSymbols())
    Symbols.push_back(plugin::Symbol(S));
  return Symbols;
}

DiagnosticEntry::DiagIDType
LinkerWrapper::getFatalDiagID(const std::string &formatStr) {
  return m_DiagEngine->getCustomDiagID(DiagnosticEngine::Fatal, formatStr);
}
DiagnosticEntry::DiagIDType
LinkerWrapper::getErrorDiagID(const std::string &formatStr) {
  return m_DiagEngine->getCustomDiagID(DiagnosticEngine::Error, formatStr);
}
DiagnosticEntry::DiagIDType
LinkerWrapper::getWarningDiagID(const std::string &formatStr) {
  return m_DiagEngine->getCustomDiagID(DiagnosticEngine::Warning, formatStr);
}

DiagnosticEntry::DiagIDType
LinkerWrapper::getNoteDiagID(const std::string &formatStr) const {
  return m_DiagEngine->getCustomDiagID(DiagnosticEngine::Note, formatStr);
}

DiagnosticEntry::DiagIDType
LinkerWrapper::getVerboseDiagID(const std::string &formatStr) {
  return m_DiagEngine->getCustomDiagID(DiagnosticEngine::Verbose, formatStr);
}

DiagnosticBuilder
LinkerWrapper::getDiagnosticBuilder(DiagnosticEntry::DiagIDType id) const {
  eld::MsgHandler *msg = m_DiagEngine->raisePluginDiag(id, getPlugin());
  return DiagnosticBuilder(msg);
}

bool LinkerWrapper::reportDiagEntry(std::unique_ptr<DiagnosticEntry> de) {
  m_DiagEngine->raisePluginDiag(std::move(de), getPlugin());
  return true;
}

bool LinkerWrapper::isChunkMovableFromOutputSection(const Chunk &C) const {
  // For linker states before layout, if Chunks are accessible, then they
  // can be moved
  if (getState() <= LinkerWrapper::BeforeLayout)
    return true;

  // For linker state After layout, Chunks cannot be moved
  if (getState() == LinkerWrapper::AfterLayout)
    return false;

  // Everything else can be moved
  return true;
}

std::string_view LinkerWrapper::getCurrentLinkStateAsStr() const {
  switch (getState()) {
#define ADD_CASE(linkerState)                                                  \
  case State::linkerState:                                                     \
    return #linkerState;
    ADD_CASE(Unknown);
    ADD_CASE(Initializing);
    ADD_CASE(BeforeLayout);
    ADD_CASE(CreatingSections);
    ADD_CASE(AfterLayout);
    ADD_CASE(CreatingSegments);
#undef ADD_CASE
  }
}

bool LinkerWrapper::isVerbose() const {
  return m_Module.getConfig().getPrinter()->isVerbose();
}

eld::Expected<std::vector<plugin::OutputSection>>
LinkerWrapper::getAllOutputSections() const {
  if (getState() < State::AfterLayout)
    RETURN_INVALID_LINK_STATE_ERR("CreatingSegments, AfterLayout");

  SectionMap sectMap = m_Module.getScript().sectionMap();
  std::vector<plugin::OutputSection> outputSects;
  for (OutputSectionEntry *OSE : sectMap) {
    outputSects.emplace_back(OSE);
  }
  return outputSects;
}

eld::Expected<std::vector<Segment>>
LinkerWrapper::getSegmentsForOutputSection(const OutputSection &O) const {
  if (getState() < LinkerWrapper::CreatingSections)
    return std::make_unique<DiagnosticEntry>(
        Diag::error_invalid_link_state,
        std::vector<std::string>{std::string(getCurrentLinkStateAsStr()),
                                 __FUNCTION__,
                                 "'CreatingSections, AfterLayout'"});
  std::vector<Segment> Segments;
  for (auto *S :
       m_Module.getBackend()->getSegmentsForSection(O.getOutputSection()))
    Segments.push_back(Segment(S));
  return Segments;
}

std::vector<LinkerWrapper::UnbalancedChunkMove>
LinkerWrapper::getUnbalancedChunkRemoves() const {
  const eld::Plugin::UnbalancedFragmentMoves &unbalancedFragMoves =
      m_Plugin->getUnbalancedFragmentMoves();
  const eld::Plugin::UnbalancedFragmentMoves::TrackingDataType
      &unbalancedFragRemoves = unbalancedFragMoves.UnbalancedRemoves;
  std::vector<UnbalancedChunkMove> unbalancedChunkRemoves;
  for (const auto &elem : unbalancedFragRemoves) {
    unbalancedChunkRemoves.push_back(
        {Chunk(elem.first), LinkerScriptRule(elem.second)});
  }
  return unbalancedChunkRemoves;
}

std::vector<LinkerWrapper::UnbalancedChunkMove>
LinkerWrapper::getUnbalancedChunkAdds() const {
  const eld::Plugin::UnbalancedFragmentMoves &unbalancedFragMoves =
      m_Plugin->getUnbalancedFragmentMoves();
  const eld::Plugin::UnbalancedFragmentMoves::TrackingDataType
      &unbalancedFragAdds = unbalancedFragMoves.UnbalancedAdds;
  std::vector<UnbalancedChunkMove> unbalancedChunkAdds;
  for (const auto &elem : unbalancedFragAdds) {
    unbalancedChunkAdds.push_back(
        {Chunk(elem.first), LinkerScriptRule(elem.second)});
  }
  return unbalancedChunkAdds;
}

std::optional<std::string>
LinkerWrapper::getEnv(const std::string &envVar) const {
  return llvm::sys::Process::GetEnv(envVar);
}

eld::Expected<void> LinkerWrapper::registerCommandLineOption(
    const std::string &opt, bool hasValue,
    const CommandLineOptionHandlerType &optionHandler) {
  if (getState() != LinkerWrapper::Initializing)
    return std::make_unique<DiagnosticEntry>(
        Diag::error_invalid_link_state,
        std::vector<std::string>{std::string(getCurrentLinkStateAsStr()),
                                 __FUNCTION__, "Initializing"});
  if (!(opt.size() >= 2 && opt[0] == '-' && opt[1] == '-'))
    return std::make_unique<DiagnosticEntry>(Diag::error_plugin_opt_prefix,
                                             std::vector<std::string>{opt});
  if (opt.size() == 2)
    return std::make_unique<DiagnosticEntry>(Diag::error_plugin_opt_empty,
                                             std::vector<std::string>{opt});
  m_Plugin->registerCommandLineOption(opt, hasValue, optionHandler);
  return {};
}

eld::Expected<void> LinkerWrapper::enableVisitSymbol() {
  if (getState() != LinkerWrapper::Initializing)
    return std::make_unique<DiagnosticEntry>(
        Diag::error_invalid_link_state,
        std::vector<std::string>{std::string(getCurrentLinkStateAsStr()),
                                 __FUNCTION__, "Initializing"});
  PluginManager &PM = m_Module.getPluginManager();
  PM.addSymbolVisitor(m_Plugin);
  return {};
}

eld::Expected<void> LinkerWrapper::setRuleMatchingSectionNameMap(
    InputFile IF, std::unordered_map<uint64_t, std::string> sectionMap) {
  if (getState() != LinkerWrapper::Initializing)
    RETURN_INVALID_LINK_STATE_ERR("Initializing")
  eld::InputFile *inputFile = IF.getInputFile();
  if (!inputFile)
    return std::make_unique<DiagnosticEntry>(Diag::error_empty_input_file);
  eld::ObjectFile *objectFile = llvm::dyn_cast<eld::ObjectFile>(inputFile);
  if (!objectFile)
    return std::make_unique<DiagnosticEntry>(
        Diag::error_invalid_input_file_for_api,
        std::vector<std::string>{inputFile->getInput()->decoratedPath(),
                                 LLVM_PRETTY_FUNCTION});
  PluginManager &PM = m_Module.getPluginManager();
  if (objectFile->hasRuleMatchingSectionNameMap()) {
    const eld::Plugin *P = PM.getRMSectionNameMapProvider(inputFile);
    ASSERT(P, "P must be non-null");
    return std::make_unique<DiagnosticEntry>(
        Diag::error_RM_sect_name_map_already_set,
        std::vector<std::string>{inputFile->getInput()->decoratedPath(),
                                 P->getPluginName()});
  }
  objectFile->setRuleMatchingSectionNameMap(sectionMap);
  PM.addRMSectionNameMapProvider(inputFile, m_Plugin);
  return {};
}

plugin::LinkerConfig LinkerWrapper::getLinkerConfig() const {
  return LinkerConfig(getModule()->getConfig());
}

void LinkerWrapper::showRuleMatchingSectionNameInDiagnostics() {
  eld::LinkerConfig &config = m_Module.getConfig();
  PluginManager &PM = m_Module.getPluginManager();
  PM.enableShowRMSectNameInDiag(config, *m_Plugin);
}

eld::Expected<void> LinkerWrapper::setAuxiliarySymbolNameMap(
    InputFile IF, const AuxiliarySymbolNameMap &symbolNameMap) {
  eld::InputFile *inputFile = IF.getInputFile();
  if (!inputFile)
    return std::make_unique<DiagnosticEntry>(Diag::error_empty_input_file);
  eld::ObjectFile *objectFile = llvm::dyn_cast<eld::ObjectFile>(inputFile);
  if (!objectFile)
    return std::make_unique<DiagnosticEntry>(
        Diag::error_invalid_input_file_for_api,
        std::vector<std::string>{inputFile->getInput()->decoratedPath(),
                                 LLVM_PRETTY_FUNCTION});
  PluginManager &PM = m_Module.getPluginManager();
  if (objectFile->hasAuxiliarySymbolNameMap()) {
    const eld::Plugin *P = PM.getAuxiliarySymbolNameMapProvider(objectFile);
    ASSERT(P, "P must be non-null");
    return std::make_unique<DiagnosticEntry>(
        Diag::error_aux_sym_name_map_already_set,
        std::vector<std::string>{inputFile->getInput()->decoratedPath(),
                                 P->getPluginName()});
  }
  PM.setAuxiliarySymbolNameMap(objectFile, symbolNameMap, m_Plugin);
  return {};
}
const std::optional<std::unordered_map<uint64_t, std::string>> &
LinkerWrapper::getRuleMatchingSectionNameMap(const InputFile &IF) {
  static const std::optional<std::unordered_map<uint64_t, std::string>> empty =
      std::nullopt;
  eld::ObjectFile *objFile =
      llvm::dyn_cast_or_null<eld::ObjectFile>(IF.getInputFile());
  if (!objFile)
    return empty;
  return objFile->getRuleMatchingSectNameMap();
}

std::optional<std::string>
LinkerWrapper::getRuleMatchingSectionName(Section S) {
  if (!S)
    return std::nullopt;
  eld::InputFile *IF = S.getInputFile().getInputFile();
  eld::ObjectFile *objFile = llvm::dyn_cast_or_null<eld::ObjectFile>(IF);
  if (!objFile)
    return nullptr;
  return objFile->getRuleMatchingSectName(S.getIndex());
}

eld::Expected<uint64_t> LinkerWrapper::getOutputSymbolIndex(plugin::Symbol S) {
  GNULDBackend &backend = *m_Module.getBackend();
  return backend.getSymbolIdx(S.getSymbol()->outSymbol());
}

eld::Expected<bool> LinkerWrapper::doesRuleMatchWithSection(
    const LinkerScriptRule &R, const Section &S, bool doNotUseRSymbolName) {
  if (!R)
    return std::make_unique<DiagnosticEntry>(Diag::error_empty_rule,
                                             std::vector<std::string>{});
  if (!S)
    return std::make_unique<DiagnosticEntry>(Diag::error_empty_section,
                                             std::vector<std::string>{});
  const SectionMap &SM = m_Module.getLinkerScript().sectionMap();
  return SM.doesRuleMatchWithSection(*(R.getRuleContainer()), *(S.getSection()),
                                     doNotUseRSymbolName);
}
