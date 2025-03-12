//===- LinkerScript.cpp----------------------------------------------------===//
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
#include "eld/Core/LinkerScript.h"
#include "eld/Config/GeneralOptions.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Plugin/PluginOp.h"
#include "eld/PluginAPI/DiagnosticEntry.h"
#include "eld/PluginAPI/LinkerWrapper.h"
#include "eld/PluginAPI/PluginBase.h"
#include "eld/Script/ExternCmd.h"
#include "eld/Script/MemoryCmd.h"
#include "eld/Script/SymbolContainer.h"
#include "eld/Support/Memory.h"
#include "eld/Target/Relocator.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/xxhash.h"
#include <string>

using namespace eld;

//===----------------------------------------------------------------------===//
// LinkerScript
//===----------------------------------------------------------------------===//
LinkerScript::LinkerScript(DiagnosticEngine *diagEngine)
    : m_hasPhdrsSpecified(false), m_hasPTPHDR(false), m_HasSizeOfHeader(false),
      m_HasFileHeader(false), m_HasProgramHeader(false), m_hasError(false),
      m_HasSectionsCmd(false), m_HasExternCmd(false), m_NumWildCardPatterns(0),
      m_HashingEnabled(false), RuleCount(0), m_DiagEngine(diagEngine) {}

LinkerScript::~LinkerScript() {}

void LinkerScript::unloadPlugins(Module *Module) {
  // Unload the plugins.
  for (auto &H : m_LibraryToPluginMap) {
    if (!H.second)
      continue;
    // Run the cleanup function
    H.second->Cleanup();
    Plugin::Unload(H.first, H.second->getLibraryHandle(), Module);
    if (Module->getPrinter()->isVerbose())
      m_DiagEngine->raise(diag::unloaded_plugin) << H.first;
  }
}

void LinkerScript::printPluginTimers(llvm::raw_ostream &OS) {
  if (!Map.size())
    return;
  for (auto &I : Map) {
    I.second.first->print(OS);
    I.second.first->clear();
  }
}

// FIXME: This member function does not need a reference argument
// to the same class object.
void LinkerScript::createSectionMap(LinkerScript &L, const LinkerConfig &config,
                                    LayoutPrinter *layoutPrinter) {
  m_SectionMap = make<SectionMap>(L, config, layoutPrinter);
}

void LinkerScript::insertPhdrSpec(const PhdrSpec &phdrsSpec) {
  m_PhdrList.push_back(make<Phdrs>(phdrsSpec));
}

Plugin *LinkerScript::addPlugin(plugin::PluginBase::Type T, std::string Name,
                                std::string PluginRegisterType,
                                std::string PluginOpts, bool Stats,
                                Module &Module) {
  Plugin *P =
      make<Plugin>(T, Name, PluginRegisterType, PluginOpts, Stats, Module);
  addPlugin(P, Module);
  return P;
}

void LinkerScript::addPlugin(Plugin *P, Module &Module) {
  m_Plugins.push_back(P);
  if (Module.getPrinter()->isVerbose())
    m_DiagEngine->raise(diag::added_plugin) << P->getName();
}

LinkerScript::PluginVectorT
LinkerScript::getPluginForType(plugin::PluginBase::Type T) const {
  std::vector<Plugin *> PluginsForType;
  for (auto &P : m_Plugins) {
    if (P->getType() == T)
      PluginsForType.push_back(P);
  }
  return PluginsForType;
}

void LinkerScript::addPluginToTar(std::string filename,
                                  std::string &resolvedPath,
                                  OutputTarWriter *outputTar) {
  outputTar->createAndAddPlugin(filename, resolvedPath);
}

bool LinkerScript::loadNonUniversalPlugins(Module &M) {
  for (auto &P : m_Plugins) {
    ASSERT(P, "P must not be a nullptr");
    if (P->getType() == plugin::PluginBase::LinkerPlugin)
      continue;
    if (!loadPlugin(*P, M))
      return false;
  }

  return true;
}

bool LinkerScript::loadUniversalPlugins(Module &M) {
  for (auto &P : m_Plugins) {
    ASSERT(P, "P must not be a nullptr");
    if (P->getType() != plugin::PluginBase::LinkerPlugin)
      continue;
    if (!loadPlugin(*P, M))
      return false;
  }

  return true;
}

/// Return a combined hash representing the linker script (and its included
/// files)
std::string LinkerScript::getHash() {
  assert(m_HashingEnabled &&
         "Linker script hash requested with hashing disabled!");
  return llvm::toHex(Hasher.result());
}

/// Compute a hash for the given linker script and add it to the combined hash
/// if it's a file. Otherwise, add the given text to the hash.
void LinkerScript::addToHash(llvm::StringRef FilenameOrText) {
  using namespace llvm;

  if (!m_HashingEnabled)
    return;

  Hasher.update(FilenameOrText);

  // If this is a file, we also want to read its contents and compute a hash.
  llvm::sys::fs::file_status Status;
  if (llvm::sys::fs::status(FilenameOrText, Status))
    return;

  ErrorOr<std::unique_ptr<MemoryBuffer>> MBOrErr =
      MemoryBuffer::getFile(FilenameOrText);
  if (!MBOrErr)
    return; // Ignore errors here. It'll fail later.

  uint64_t I = xxHash64((*MBOrErr)->getBuffer());

  uint8_t Data[8];
  Data[0] = I;
  Data[1] = I >> 8;
  Data[2] = I >> 16;
  Data[3] = I >> 24;
  Data[4] = I >> 32;
  Data[5] = I >> 40;
  Data[6] = I >> 48;
  Data[7] = I >> 56;
  Hasher.update(llvm::ArrayRef<uint8_t>{Data, 8});
}

void LinkerScript::registerWildCardPattern(WildcardPattern *P) {
  P->setID(m_NumWildCardPatterns++);
}

void LinkerScript::addSectionOverride(plugin::LinkerWrapper *W, eld::Module *M,
                                      eld::Section *S, std::string O,
                                      std::string Annotation) {
  ChangeOutputSectionPluginOp *Op = eld::make<ChangeOutputSectionPluginOp>(
      W, llvm::dyn_cast<ELFSection>(S), O, Annotation);
  m_OverrideSectionMatch[W].push_back(Op);
  if (M->getPrinter()->isVerbose())
    m_DiagEngine->raise(diag::added_section_override)
        << W->getPlugin()->getPluginName() << O << S->name() << Annotation;
  LayoutPrinter *printer = M->getLayoutPrinter();
  if (!printer)
    return;
  printer->recordSectionOverride(W, Op);
}

void LinkerScript::removeSymbolOp(plugin::LinkerWrapper *W, eld::Module *M,
                                  const ResolveInfo *S) {
  RemoveSymbolPluginOp *Op = make<RemoveSymbolPluginOp>(W, "", S);
  LayoutPrinter *Printer = M->getLayoutPrinter();
  if (!Printer)
    return;
  Printer->recordRemoveSymbol(W, Op);
}

void LinkerScript::clearAllSectionOverrides() {
  m_OverrideSectionMatch.clear();
}

void LinkerScript::clearSectionOverrides(const plugin::LinkerWrapper *LW) {
  if (LW)
    m_OverrideSectionMatch.erase(LW);
  else
    clearAllSectionOverrides();
}

std::vector<ChangeOutputSectionPluginOp *>
LinkerScript::getAllSectionOverrides() {
  std::vector<ChangeOutputSectionPluginOp *> Ops;
  for (auto &K : m_OverrideSectionMatch)
    Ops.insert(Ops.end(), K.second.begin(), K.second.end());
  return Ops;
}

LinkerScript::OverrideSectionMatchT
LinkerScript::getSectionOverrides(const plugin::LinkerWrapper *LW) {
  if (!LW)
    return getAllSectionOverrides();
  if (m_OverrideSectionMatch.count(LW))
    return m_OverrideSectionMatch[LW];
  return OverrideSectionMatchT{};
}

eld::Expected<void> LinkerScript::addChunkOp(plugin::LinkerWrapper *W,
                                             eld::Module *M, RuleContainer *R,
                                             eld::Fragment *F,
                                             std::string Annotation) {
  AddChunkPluginOp *Op = eld::make<AddChunkPluginOp>(W, R, F, Annotation);
  LinkerConfig &config = M->getConfig();
  const auto &unbalancedAdds =
      W->getPlugin()->getUnbalancedFragmentMoves().UnbalancedAdds;
  auto fragUnbalancedAdds = unbalancedAdds.find(F);
  if (fragUnbalancedAdds != unbalancedAdds.end()) {
    return std::make_unique<plugin::DiagnosticEntry>(
        diag::err_multiple_chunk_add,
        std::vector<std::string>{
            F->getOwningSection()->getDecoratedName(config.options()),
            R->getAsString(), fragUnbalancedAdds->second->getAsString()});
  }

  F->getOwningSection()->setOutputSection(R->getSection()->getOutputSection());
  F->getOwningSection()->setMatchedLinkerScriptRule(R);

  R->getSection()->addFragment(F);
  R->setDirty();

  W->getPlugin()->recordFragmentAdd(R, F);

  LayoutPrinter *printer = M->getLayoutPrinter();
  if (!printer)
    return {};
  printer->recordAddChunk(W, Op);
  if (M->getPrinter()->isVerbose())
    m_DiagEngine->raise(diag::added_chunk_op)
        << R->getSection()->getOutputSection()->name() << Annotation;
  return {};
}

eld::Expected<void> LinkerScript::removeChunkOp(plugin::LinkerWrapper *W,
                                                eld::Module *M,
                                                RuleContainer *R,
                                                eld::Fragment *F,
                                                std::string Annotation) {
  RemoveChunkPluginOp *Op = eld::make<RemoveChunkPluginOp>(W, R, F, Annotation);

  bool removed = R->getSection()->removeFragment(F);
  R->setDirty();

  LinkerConfig &config = M->getConfig();

  if (removed)
    W->getPlugin()->recordFragmentRemove(R, F);
  else
    return std::make_unique<plugin::DiagnosticEntry>(
        diag::error_chunk_not_found,
        std::vector<std::string>{
            F->getOwningSection()->getDecoratedName(config.options()),
            R->getAsString(),
            F->getOutputELFSection()->getDecoratedName(config.options())});

  LayoutPrinter *printer = M->getLayoutPrinter();
  if (!printer)
    return {};
  printer->recordRemoveChunk(W, Op);
  if (M->getPrinter()->isVerbose())
    m_DiagEngine->raise(diag::removed_chunk_op)
        << R->getSection()->name() << Annotation;
  return {};
}

eld::Expected<void> LinkerScript::updateChunksOp(
    plugin::LinkerWrapper *W, eld::Module *M, RuleContainer *R,
    std::vector<eld::Fragment *> &Frags, std::string Annotation) {
  LayoutPrinter *printer = M->getLayoutPrinter();
  if (printer) {
    UpdateChunksPluginOp *Op = eld::make<UpdateChunksPluginOp>(
        W, R, UpdateChunksPluginOp::Type::Start, Annotation);
    printer->recordUpdateChunks(W, Op);
  }

  llvm::SmallVectorImpl<Fragment *> &FragmentsInRule =
      R->getSection()->getFragmentList();
  if (printer) {
    for (auto &Frag : FragmentsInRule) {
      RemoveChunkPluginOp *Op =
          eld::make<RemoveChunkPluginOp>(W, R, Frag, Annotation);
      printer->recordRemoveChunk(W, Op);
    }
  }

  for (auto F : FragmentsInRule) {
    W->getPlugin()->recordFragmentRemove(R, F);
  }

  R->clearSections();
  R->clearFragments();

  for (auto &F : Frags) {
    auto expAddChunk = addChunkOp(W, M, R, F);
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expAddChunk);
  }

  if (printer) {
    UpdateChunksPluginOp *Op = eld::make<UpdateChunksPluginOp>(
        W, R, UpdateChunksPluginOp::Type::End, Annotation);
    printer->recordUpdateChunks(W, Op);
  }
  return {};
}

llvm::Timer *LinkerScript::getTimer(llvm::StringRef Name,
                                    llvm::StringRef Description,
                                    llvm::StringRef GroupName,
                                    llvm::StringRef GroupDescription) {
  std::pair<llvm::TimerGroup *, Name2TimerMap> &GroupEntry = Map[GroupName];
  if (!GroupEntry.first)
    GroupEntry.first = eld::make<llvm::TimerGroup>(GroupName, GroupDescription);

  llvm::Timer *T = GroupEntry.second[Name];
  if (T == nullptr) {
    T = eld::make<llvm::Timer>();
    GroupEntry.second[Name] = T;
  }
  if (!T->isInitialized())
    T->init(Name, Description, *GroupEntry.first);
  return T;
}

plugin::LinkerPluginConfig *
LinkerScript::getLinkerPluginConfig(plugin::LinkerWrapper *LW) {
  auto Plugin = m_PluginMap.find(LW);
  if (Plugin == m_PluginMap.end())
    return nullptr;
  return Plugin->second->getLinkerPluginConfig();
}

Plugin *LinkerScript::getPlugin(plugin::LinkerWrapper *LW) {
  auto Plugin = m_PluginMap.find(LW);
  if (Plugin == m_PluginMap.end())
    return nullptr;
  return Plugin->second;
}

bool LinkerScript::registerReloc(plugin::LinkerWrapper *LW, uint32_t RelocType,
                                 std::string Name) {
  auto Plugin = m_PluginMap.find(LW);
  if (Plugin == m_PluginMap.end())
    return false;
  Plugin->second->registerRelocType(RelocType, Name);
  return true;
}

bool LinkerScript::hasPendingSectionOverride(
    const plugin::LinkerWrapper *LW) const {
  if (LW) {
    auto iter = m_OverrideSectionMatch.find(LW);
    return iter != m_OverrideSectionMatch.end() && !iter->second.empty();
  } else
    return !m_OverrideSectionMatch.empty();
}

void LinkerScript::addScriptCommand(ScriptCommand *pCommand) {
  m_ScriptCommands.push_back(pCommand);
  if (!hasExternCommand() && pCommand->isExtern())
    setHasExternCmd();
}

eld::Expected<eld::ScriptMemoryRegion *>
LinkerScript::getMemoryRegion(llvm::StringRef DescName,
                              const std::string Context) const {
  auto R = m_MemoryRegionMap.find(DescName);
  if (R == m_MemoryRegionMap.end())
    return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
        diag::error_region_not_found, {std::string(DescName), Context}));
  return R->second;
}

eld::Expected<eld::ScriptMemoryRegion *>
LinkerScript::getMemoryRegion(llvm::StringRef DescName) const {
  auto R = m_MemoryRegionMap.find(DescName);
  if (R == m_MemoryRegionMap.end())
    return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
        diag::error_exp_mem_region_not_found, {std::string(DescName)}));
  return R->second;
}

eld::Expected<eld::ScriptMemoryRegion *>
LinkerScript::getMemoryRegionForRegionAlias(llvm::StringRef DescName,
                                            const std::string Context) const {
  auto R = m_MemoryRegionMap.find(DescName);
  if (R == m_MemoryRegionMap.end())
    return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
        diag::error_region_not_found, {std::string(DescName), Context}));
  return R->second;
}

eld::Expected<bool> LinkerScript::insertRegionAlias(llvm::StringRef Alias,
                                                    const std::string Context) {
  if (m_RegionAlias.insert(Alias).second)
    return true;
  return std::make_unique<plugin::DiagnosticEntry>(
      plugin::DiagnosticEntry(diag::error_duplicate_memory_region_alias,
                              {std::string(Alias), Context}));
}

llvm::StringRef LinkerScript::saveString(std::string S) {
  return Saver.save(S);
}

bool LinkerScript::loadPlugin(Plugin &P, Module &M) {
  LayoutPrinter *printer = M.getLayoutPrinter();
  LinkerConfig &config = M.getConfig();

  ASSERT(!P.getID(), "Plugin " + P.getPluginName() + " is already loaded!");

  if (m_PluginInfo.find(P.getPluginType()) != m_PluginInfo.end()) {
    m_DiagEngine->raise(diag::error_plugin_name_not_unique)
        << P.getPluginType();
    return false;
  }

  m_PluginInfo.insert(std::make_pair(P.getPluginType(), &P));

  std::string resolvedPath = P.resolvePath(config);
  if (resolvedPath.empty())
    return false;
  if (M.getOutputTarWriter() && llvm::sys::fs::exists(resolvedPath))
    addPluginToTar(P.getName(), resolvedPath, M.getOutputTarWriter());
  auto I = m_LibraryToPluginMap.find(resolvedPath);
  void *handle = nullptr;
  if (I == m_LibraryToPluginMap.end()) {
    handle = Plugin::LoadPlugin(resolvedPath, &M);
    m_LibraryToPluginMap.insert(std::make_pair(resolvedPath, &P));
  } else {
    handle = I->second->getLibraryHandle();
  }
  if (!handle)
    return false;
  P.setLibraryHandle(handle);
  P.SetFunctions();
  // Call RegisterAll function of the library if not already called.
  if (I == m_LibraryToPluginMap.end())
    if (!P.RegisterAll())
      return false;
  if (!P.RegisterPlugin(handle))
    return false;
  // Create the Bitvector.
  P.createRelocationVector(M.getBackend()->getRelocator()->getNumRelocs());

  plugin::LinkerWrapper *LW = eld::make<plugin::LinkerWrapper>(&P, M);
  P.getLinkerPlugin()->setLinkerWrapper(LW);
  P.setID(m_PluginMap.size() + 1);
  recordPlugin(LW, &P);
  if (printer)
    printer->recordPlugin();

  // FIXME: Why are we calling Init hook if a plugin has LinkerPluginConfig?
  if (P.getLinkerPluginConfig())
    P.Init(M.getOutputTarWriter());
  P.initializeLinkerPluginConfig();
  if (M.getPrinter()->isVerbose())
    m_DiagEngine->raise(diag::loaded_plugin) << P.getName();
  return true;
}
