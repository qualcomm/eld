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
LinkerScript::LinkerScript(DiagnosticEngine *DiagEngine)
    : HasPhdrsSpecified(false), HasPtphdr(false), HasSizeOfHeader(false),
      HasFileHeader(false), HasProgramHeader(false), HasError(false),
      HasSectionsCmd(false), HasExternCmd(false), NumWildCardPatterns(0),
      HashingEnabled(false), RuleCount(0), Diag(DiagEngine) {}

LinkerScript::~LinkerScript() {}

void LinkerScript::unloadPlugins(Module *Module) {
  // Unload the plugins.
  for (auto &H : MLibraryToPluginMap) {
    if (!H.second)
      continue;
    // Run the cleanup function
    H.second->cleanup();
    Plugin::unload(H.first, H.second->getLibraryHandle(), Module);
    if (Module->getPrinter()->isVerbose())
      Diag->raise(Diag::unloaded_plugin) << H.first;
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
void LinkerScript::createSectionMap(LinkerScript &L, const LinkerConfig &Config,
                                    LayoutInfo *LayoutInfo) {
  OutputSectionMap = make<SectionMap>(L, Config, LayoutInfo);
}

void LinkerScript::insertPhdrSpec(const PhdrSpec &PhdrsSpec) {
  PhdrList.push_back(make<Phdrs>(PhdrsSpec));
}

Plugin *LinkerScript::addPlugin(plugin::PluginBase::Type T, std::string Name,
                                std::string PluginRegisterType,
                                std::string PluginOpts, bool Stats,
                                bool IsDefaultPlugin, Module &Module) {
  Plugin *P = make<Plugin>(T, Name, PluginRegisterType, PluginOpts, Stats,
                           IsDefaultPlugin, Module);
  addPlugin(P, Module);
  return P;
}

void LinkerScript::addPlugin(Plugin *P, Module &Module) {
  MPlugins.push_back(P);
  if (Module.getPrinter()->isVerbose())
    Diag->raise(Diag::added_plugin) << P->getName();
}

LinkerScript::PluginVectorT
LinkerScript::getPluginForType(plugin::PluginBase::Type T) const {
  std::vector<Plugin *> PluginsForType;
  for (auto &P : MPlugins) {
    if (P->getType() == T)
      PluginsForType.push_back(P);
  }
  return PluginsForType;
}

void LinkerScript::addPluginToTar(std::string Filename,
                                  std::string &ResolvedPath,
                                  OutputTarWriter *OutputTar) {
  OutputTar->createAndAddPlugin(Filename, ResolvedPath);
}

bool LinkerScript::loadNonUniversalPlugins(Module &M) {
  for (auto &P : MPlugins) {
    ASSERT(P, "P must not be a nullptr");
    if (P->getType() == plugin::PluginBase::LinkerPlugin)
      continue;
    if (!loadPlugin(*P, M))
      return false;
  }

  return true;
}

bool LinkerScript::loadUniversalPlugins(Module &M) {
  for (auto &P : MPlugins) {
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
  assert(HashingEnabled &&
         "Linker script hash requested with hashing disabled!");
  return llvm::toHex(Hasher.result());
}

/// Compute a hash for the given linker script and add it to the combined hash
/// if it's a file. Otherwise, add the given text to the hash.
void LinkerScript::addToHash(llvm::StringRef FilenameOrText) {
  using namespace llvm;

  if (!HashingEnabled)
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
  P->setID(NumWildCardPatterns++);
}

void LinkerScript::addSectionOverride(plugin::LinkerWrapper *W, eld::Module *M,
                                      eld::Section *S, std::string O,
                                      std::string Annotation) {
  ChangeOutputSectionPluginOp *Op = eld::make<ChangeOutputSectionPluginOp>(
      W, llvm::dyn_cast<ELFSection>(S), O, Annotation);
  OverrideSectionMatch[W].push_back(Op);
  if (M->getPrinter()->isVerbose())
    Diag->raise(Diag::added_section_override)
        << W->getPlugin()->getPluginName() << O << S->name() << Annotation;
  LayoutInfo *layoutInfo = M->getLayoutInfo();
  if (!layoutInfo)
    return;
  layoutInfo->recordSectionOverride(W, Op);
}

void LinkerScript::removeSymbolOp(plugin::LinkerWrapper *W, eld::Module *M,
                                  const ResolveInfo *S) {
  RemoveSymbolPluginOp *Op = make<RemoveSymbolPluginOp>(W, "", S);
  LayoutInfo *layoutInfo = M->getLayoutInfo();
  if (!layoutInfo)
    return;
  layoutInfo->recordRemoveSymbol(W, Op);
}

void LinkerScript::clearAllSectionOverrides() { OverrideSectionMatch.clear(); }

void LinkerScript::clearSectionOverrides(const plugin::LinkerWrapper *LW) {
  if (LW)
    OverrideSectionMatch.erase(LW);
  else
    clearAllSectionOverrides();
}

std::vector<ChangeOutputSectionPluginOp *>
LinkerScript::getAllSectionOverrides() {
  std::vector<ChangeOutputSectionPluginOp *> Ops;
  for (auto &K : OverrideSectionMatch)
    Ops.insert(Ops.end(), K.second.begin(), K.second.end());
  return Ops;
}

LinkerScript::OverrideSectionMatchT
LinkerScript::getSectionOverrides(const plugin::LinkerWrapper *LW) {
  if (!LW)
    return getAllSectionOverrides();
  if (OverrideSectionMatch.count(LW))
    return OverrideSectionMatch[LW];
  return OverrideSectionMatchT{};
}

eld::Expected<void> LinkerScript::addChunkOp(plugin::LinkerWrapper *W,
                                             eld::Module *M, RuleContainer *R,
                                             eld::Fragment *F,
                                             std::string Annotation) {
  AddChunkPluginOp *Op = eld::make<AddChunkPluginOp>(W, R, F, Annotation);
  LinkerConfig &Config = M->getConfig();
  const auto &UnbalancedAdds =
      W->getPlugin()->getUnbalancedFragmentMoves().UnbalancedAdds;
  auto FragUnbalancedAdds = UnbalancedAdds.find(F);
  if (FragUnbalancedAdds != UnbalancedAdds.end()) {
    return std::make_unique<plugin::DiagnosticEntry>(
        Diag::err_multiple_chunk_add,
        std::vector<std::string>{
            F->getOwningSection()->getDecoratedName(Config.options()),
            R->getAsString(), FragUnbalancedAdds->second->getAsString()});
  }

  F->getOwningSection()->setOutputSection(R->getSection()->getOutputSection());
  F->getOwningSection()->setMatchedLinkerScriptRule(R);

  R->getSection()->addFragment(F);
  R->setDirty();

  W->getPlugin()->recordFragmentAdd(R, F);

  LayoutInfo *layoutInfo = M->getLayoutInfo();
  if (!layoutInfo)
    return {};
  layoutInfo->recordAddChunk(W, Op);
  if (M->getPrinter()->isVerbose())
    Diag->raise(Diag::added_chunk_op)
        << R->getSection()->getOutputSection()->name() << Annotation;
  return {};
}

eld::Expected<void> LinkerScript::removeChunkOp(plugin::LinkerWrapper *W,
                                                eld::Module *M,
                                                RuleContainer *R,
                                                eld::Fragment *F,
                                                std::string Annotation) {
  RemoveChunkPluginOp *Op = eld::make<RemoveChunkPluginOp>(W, R, F, Annotation);

  bool Removed = R->getSection()->removeFragment(F);
  R->setDirty();

  LinkerConfig &Config = M->getConfig();

  if (Removed)
    W->getPlugin()->recordFragmentRemove(R, F);
  else
    return std::make_unique<plugin::DiagnosticEntry>(
        Diag::error_chunk_not_found,
        std::vector<std::string>{
            F->getOwningSection()->getDecoratedName(Config.options()),
            R->getAsString(),
            F->getOutputELFSection()->getDecoratedName(Config.options())});

  LayoutInfo *layoutInfo = M->getLayoutInfo();
  if (!layoutInfo)
    return {};
  layoutInfo->recordRemoveChunk(W, Op);
  if (M->getPrinter()->isVerbose())
    Diag->raise(Diag::removed_chunk_op)
        << R->getSection()->name() << Annotation;
  return {};
}

eld::Expected<void> LinkerScript::updateChunksOp(
    plugin::LinkerWrapper *W, eld::Module *M, RuleContainer *R,
    std::vector<eld::Fragment *> &Frags, std::string Annotation) {
  LayoutInfo *layoutInfo = M->getLayoutInfo();
  if (layoutInfo) {
    UpdateChunksPluginOp *Op = eld::make<UpdateChunksPluginOp>(
        W, R, UpdateChunksPluginOp::Type::Start, Annotation);
    layoutInfo->recordUpdateChunks(W, Op);
  }

  llvm::SmallVectorImpl<Fragment *> &FragmentsInRule =
      R->getSection()->getFragmentList();
  if (layoutInfo) {
    for (auto &Frag : FragmentsInRule) {
      RemoveChunkPluginOp *Op =
          eld::make<RemoveChunkPluginOp>(W, R, Frag, Annotation);
      layoutInfo->recordRemoveChunk(W, Op);
    }
  }

  for (auto *F : FragmentsInRule) {
    W->getPlugin()->recordFragmentRemove(R, F);
  }

  R->clearSections();
  R->clearFragments();

  for (auto &F : Frags) {
    auto ExpAddChunk = addChunkOp(W, M, R, F);
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(ExpAddChunk);
  }

  if (layoutInfo) {
    UpdateChunksPluginOp *Op = eld::make<UpdateChunksPluginOp>(
        W, R, UpdateChunksPluginOp::Type::End, Annotation);
    layoutInfo->recordUpdateChunks(W, Op);
  }
  return {};
}

void LinkerScript::updateRuleOp(plugin::LinkerWrapper *W, eld::Module *M,
                                RuleContainer *R, ELFSection *S,
                                const std::string &Annotation) {
  UpdateRulePluginOp *Op = eld::make<UpdateRulePluginOp>(W, R, S, Annotation);
  S->setOutputSection(R->getSection()->getOutputSection());
  S->setMatchedLinkerScriptRule(R);
  R->incMatchCount();
  LayoutInfo *layoutInfo = M->getLayoutInfo();
  if (!layoutInfo)
    return;
  layoutInfo->recordUpdateRule(W, Op);
}

void LinkerScript::updateLinkStatsOp(plugin::LinkerWrapper *W, eld::Module *M,
                                     const std::string &StatName,
                                     const std::string &StatValue) {
  UpdateLinkStatsPluginOp *Op =
      eld::make<UpdateLinkStatsPluginOp>(W, StatName, StatValue);
  W->getPlugin()->addLinkStat(StatName, StatValue);
  LayoutInfo *layoutInfo = M->getLayoutInfo();
  if (!layoutInfo)
    return;
  layoutInfo->recordUpdateLinkStats(W, Op);
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
  auto Plugin = MPluginMap.find(LW);
  if (Plugin == MPluginMap.end())
    return nullptr;
  return Plugin->second->getLinkerPluginConfig();
}

Plugin *LinkerScript::getPlugin(plugin::LinkerWrapper *LW) {
  auto Plugin = MPluginMap.find(LW);
  if (Plugin == MPluginMap.end())
    return nullptr;
  return Plugin->second;
}

bool LinkerScript::registerReloc(plugin::LinkerWrapper *LW, uint32_t RelocType,
                                 std::string Name) {
  auto Plugin = MPluginMap.find(LW);
  if (Plugin == MPluginMap.end())
    return false;
  Plugin->second->registerRelocType(RelocType, Name);
  return true;
}

bool LinkerScript::hasPendingSectionOverride(
    const plugin::LinkerWrapper *LW) const {
  if (LW) {
    auto Iter = OverrideSectionMatch.find(LW);
    return Iter != OverrideSectionMatch.end() && !Iter->second.empty();
  }
  return !OverrideSectionMatch.empty();
}

void LinkerScript::addScriptCommand(ScriptCommand *PCommand) {
  UserLinkerScriptCommands.push_back(PCommand);
  if (!hasExternCommand() && PCommand->isExtern())
    setHasExternCmd();
}

eld::Expected<eld::ScriptMemoryRegion *>
LinkerScript::getMemoryRegion(llvm::StringRef DescName,
                              const std::string Context) const {
  auto R = MMemoryRegionMap.find(DescName);
  if (R == MMemoryRegionMap.end())
    return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
        Diag::error_region_not_found, {std::string(DescName), Context}));
  return R->second;
}

eld::Expected<eld::ScriptMemoryRegion *>
LinkerScript::getMemoryRegion(llvm::StringRef DescName) const {
  auto R = MMemoryRegionMap.find(DescName);
  if (R == MMemoryRegionMap.end())
    return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
        Diag::error_exp_mem_region_not_found, {std::string(DescName)}));
  return R->second;
}

eld::Expected<eld::ScriptMemoryRegion *>
LinkerScript::getMemoryRegionForRegionAlias(llvm::StringRef DescName,
                                            const std::string Context) const {
  auto R = MMemoryRegionMap.find(DescName);
  if (R == MMemoryRegionMap.end())
    return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
        Diag::error_region_not_found, {std::string(DescName), Context}));
  return R->second;
}

eld::Expected<bool> LinkerScript::insertRegionAlias(llvm::StringRef Alias,
                                                    const std::string Context) {
  if (MemoryRegionNameAlias.insert(Alias).second)
    return true;
  return std::make_unique<plugin::DiagnosticEntry>(
      plugin::DiagnosticEntry(Diag::error_duplicate_memory_region_alias,
                              {std::string(Alias), Context}));
}

llvm::StringRef LinkerScript::saveString(std::string S) {
  return Saver.save(S);
}

bool LinkerScript::loadPlugin(Plugin &P, Module &M) {
  LayoutInfo *layoutInfo = M.getLayoutInfo();
  LinkerConfig &Config = M.getConfig();

  ASSERT(!P.getID(), "Plugin " + P.getPluginName() + " is already loaded!");

  if (MPluginInfo.find(P.getPluginType()) != MPluginInfo.end()) {
    Diag->raise(Diag::error_plugin_name_not_unique) << P.getPluginType();
    return false;
  }

  MPluginInfo.insert(std::make_pair(P.getPluginType(), &P));

  std::string ResolvedPath = P.resolvePath(Config);
  if (ResolvedPath.empty())
    return false;
  if (M.getOutputTarWriter() && llvm::sys::fs::exists(ResolvedPath))
    addPluginToTar(P.getName(), ResolvedPath, M.getOutputTarWriter());
  auto I = MLibraryToPluginMap.find(ResolvedPath);
  void *Handle = nullptr;
  if (I == MLibraryToPluginMap.end()) {
    Handle = Plugin::loadPlugin(ResolvedPath, &M);
    MLibraryToPluginMap.insert(std::make_pair(ResolvedPath, &P));
  } else {
    Handle = I->second->getLibraryHandle();
  }
  if (!Handle)
    return false;
  P.setLibraryHandle(Handle);
  P.setFunctions();
  // Call RegisterAll function of the library if not already called.
  if (I == MLibraryToPluginMap.end())
    if (!P.registerAll())
      return false;
  if (!P.registerPlugin(Handle))
    return false;
  // Create the Bitvector.
  if (M.isBackendInitialized())
    P.createRelocationVector(M.getBackend().getRelocator()->getNumRelocs());

  plugin::LinkerWrapper *LW = eld::make<plugin::LinkerWrapper>(&P, M);
  P.getLinkerPlugin()->setLinkerWrapper(LW);
  P.setID(MPluginMap.size() + 1);
  recordPlugin(LW, &P);
  if (layoutInfo)
    layoutInfo->recordPlugin();

  // FIXME: Why are we calling Init hook if a plugin has LinkerPluginConfig?
  if (P.getLinkerPluginConfig())
    P.init(M.getOutputTarWriter());
  if (M.isBackendInitialized())
    P.initializeLinkerPluginConfig();
  if (M.getPrinter()->isVerbose())
    Diag->raise(Diag::loaded_plugin) << P.getName();
  return true;
}

void LinkerScript::initializePluginConfig(Module &M) {
  if (!M.isBackendInitialized())
    return;
  for (auto &P : MPlugins) {
    P->createRelocationVector(M.getBackend().getRelocator()->getNumRelocs());
    if (M.getPrinter()->isVerbose())
      Diag->raise(Diag::verbose_loaded_plugin_config) << P->getName();
    P->initializeLinkerPluginConfig();
  }
}
