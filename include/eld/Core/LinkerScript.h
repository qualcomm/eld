//===- LinkerScript.h------------------------------------------------------===//
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
#ifndef ELD_CORE_LINKERSCRIPT_H
#define ELD_CORE_LINKERSCRIPT_H
#include "eld/Input/SearchDirs.h"
#include "eld/Object/ScriptMemoryRegion.h"
#include "eld/Object/SectionMap.h"
#include "eld/Plugin/PluginOp.h"
#include "eld/PluginAPI/DiagnosticEntry.h"
#include "eld/PluginAPI/Expected.h"
#include "eld/PluginAPI/LinkerPluginConfig.h"
#include "eld/PluginAPI/LinkerWrapper.h"
#include "eld/Script/Assignment.h"
#include "eld/Script/MemoryDesc.h"
#include "eld/Script/PhdrDesc.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/Support/SHA1.h"
#include "llvm/Support/Timer.h"
#include <atomic>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ELD {
class LinkerWrapper;
class LinkerPluginConfig;
} // namespace ELD

namespace eld {

class ChangeOutputSectionOp;
class DiagnosticEngine;
class ELFSection;
class GeneralOptions;
class LayoutPrinter;
class LDSymbol;
class OutputTarWriter;
class RuleContainer;
class ScriptCommand;
class SectionMap;
class MemoryCmd;
class SymbolContainer;

class Phdrs {
public:
  Phdrs(const PhdrSpec &PPhdrDesc) {
    ScriptSpec.Name = PPhdrDesc.Name;
    ScriptSpec.ThisType = PPhdrDesc.ThisType;
    ScriptSpec.ScriptHasFileHdr = PPhdrDesc.ScriptHasFileHdr;
    ScriptSpec.ScriptHasPhdr = PPhdrDesc.ScriptHasPhdr;
    ScriptSpec.FixedAddress = PPhdrDesc.FixedAddress;
    ScriptSpec.SectionFlags = PPhdrDesc.SectionFlags;
  }

  const PhdrSpec &spec() const { return ScriptSpec; }

  const PhdrSpec *getSpec() const { return &ScriptSpec; }

private:
  PhdrSpec ScriptSpec;
};

/** \class LinkerScript
 *
 */
class LinkerScript {
public:
  typedef std::vector<std::pair<LDSymbol *, Assignment *>> Assignments;
  typedef std::vector<ChangeOutputSectionPluginOp *> OverrideSectionMatchT;
  typedef std::vector<Plugin *> PluginVectorT;
  typedef PluginVectorT::iterator PluginVectorTIter;
  typedef llvm::StringMap<llvm::Timer *> Name2TimerMap;

  typedef std::vector<Phdrs *> PhdrSpecList;
  typedef std::vector<MemorySpec *> MemorySpecList;

public:
  LinkerScript(DiagnosticEngine *);

  ~LinkerScript();

  const PhdrSpecList &phdrList() const { return PhdrList; }
  PhdrSpecList &phdrList() { return PhdrList; }

  void createSectionMap(LinkerScript &L, const LinkerConfig &Config,
                        LayoutPrinter *LayoutPrinter);

  SectionMap &sectionMap() const { return *OutputSectionMap; }

  const Assignments &assignments() const { return LinkerScriptAssignments; }
  Assignments &assignments() { return LinkerScriptAssignments; }

  /// sysroot
  sys::fs::Path &sysroot() const;

  void setSysroot(const sys::fs::Path &Path);

  void insertPhdrSpec(const PhdrSpec &PhdrsSpec);

  void setPhdrsSpecified() { HasPhdrsSpecified = true; }

  bool phdrsSpecified() const { return HasPhdrsSpecified; }

  void sethasPTPHDR() { HasPtphdr = true; }

  bool hasPTPHDR() const { return HasPtphdr; }

  bool hasSizeOfHeader() const { return HasSizeOfHeader; }

  void setSizeOfHeader() { HasSizeOfHeader = true; }

  bool hasFileHeader() const { return HasFileHeader; }

  void setFileHeader() { HasFileHeader = true; }

  bool hasProgramHeader() const { return HasProgramHeader; }

  void setProgramHeader() { HasProgramHeader = true; }

  void setError() { HasError = true; }

  bool hasError() const { return HasError; }

  bool linkerScriptHasSectionsCommand() const { return HasSectionsCmd; }

  bool hasExternCommand() const { return HasExternCmd; }

  bool linkerScriptHasRules() const { return RuleCount > 0; };

  void setHasSectionsCmd() { HasSectionsCmd = true; }

  void setHasExternCmd() { HasExternCmd = true; }

  // ---------------- Plugin Support ------------------------------
  Plugin *addPlugin(plugin::PluginBase::Type T, std::string Name,
                    std::string PluginRegisterType, std::string PluginOpts,
                    bool S, Module &Module);

  void addPlugin(Plugin *, Module &Module);

  const PluginVectorT &getPlugins() const { return MPlugins; }

  PluginVectorT getPluginForType(plugin::PluginBase::Type T) const;

  void addPluginToTar(std::string Filename, std::string &ResolvedPath,
                      OutputTarWriter *OutputTar);

  bool loadUniversalPlugins(eld::Module &M);
  bool loadNonUniversalPlugins(eld::Module &M);

  void unloadPlugins(eld::Module *Module);

  void setNumWildCardPatterns(size_t NumPatterns) {
    NumWildCardPatterns = NumPatterns;
  }

  size_t getNumWildCardPatterns() const { return NumWildCardPatterns; }

  // --------------- Plugin Config support -----------------------

  void addPluginOutputSection(llvm::StringRef S, eld::Plugin *P) {
    PluginForOutputSection.insert(std::make_pair(S, P));
  }

  const std::unordered_map<std::string, Plugin *> &
  getPluginOutputSection() const {
    return PluginForOutputSection;
  }

  bool hasPlugins() const { return PluginForOutputSection.size(); }

  // --------------- ThinLTO Caching support -----------------------

  void setHashingEnabled() { HashingEnabled = true; }
  std::string getHash();

  void addToHash(llvm::StringRef FilenameOrText);

  // ------------- WildCardPattern support -----------------------
  void registerWildCardPattern(WildcardPattern *P);

  // ------------- OverrideSectionMatch support -----------------------
  OverrideSectionMatchT getAllSectionOverrides();

  /// Returns section overrides associated with the LinkerWrapper LW.
  /// If LW is nullptr, then all the pending section overrides are returned.
  OverrideSectionMatchT getSectionOverrides(const plugin::LinkerWrapper *);

  void clearAllSectionOverrides();

  /// Clears section overrides associated with the LinkerWrapper LW.
  /// If LW is nullptr, then clears all the pending section overrides.
  void clearSectionOverrides(const plugin::LinkerWrapper *);

  void addSectionOverride(plugin::LinkerWrapper *W, eld::Module *M,
                          eld::Section *S, std::string O,
                          std::string Annotation = "");

  /// Returns true if there are pending section overrides associated with the
  /// LinkerWrapper LW. If LW is nullptr, then returns true if there are any
  /// pending sections overrides associated with any LinkerWrapper.
  bool hasPendingSectionOverride(const plugin::LinkerWrapper *LW) const;

  // -------------- Plugin Runlist support -------------------
  std::vector<Plugin *> &getPluginRunList() { return MRunList; }

  // -------------- Annotate Rule Count ------------------------
  uint32_t getRuleCount() const { return RuleCount; }

  uint32_t getIncrementedRuleCount() { return ++RuleCount; }

  // ----------------Chunk Ops ----------------------------
  eld::Expected<void> addChunkOp(plugin::LinkerWrapper *W, eld::Module *M,
                                 RuleContainer *R, eld::Fragment *F,
                                 std::string Annotation = "");

  eld::Expected<void> removeChunkOp(plugin::LinkerWrapper *W, eld::Module *M,
                                    RuleContainer *R, eld::Fragment *F,
                                    std::string Annotation = "");

  eld::Expected<void> updateChunksOp(plugin::LinkerWrapper *W, eld::Module *M,
                                     RuleContainer *R,
                                     std::vector<eld::Fragment *> &Frags,
                                     std::string Annotation = "");

  void removeSymbolOp(plugin::LinkerWrapper *W, eld::Module *M,
                      const ResolveInfo *S);

  // ---------------- Plugin profiling Support */
  llvm::Timer *getTimer(llvm::StringRef Name, llvm::StringRef Description,
                        llvm::StringRef GroupName,
                        llvm::StringRef GroupDescription);

  void printPluginTimers(llvm::raw_ostream &);

  // ------------------ Plugin Map --------------------------------
  void recordPlugin(plugin::LinkerWrapper *Wrapper, Plugin *P) {
    MPluginMap[Wrapper] = P;
  }

  plugin::LinkerPluginConfig *getLinkerPluginConfig(plugin::LinkerWrapper *LW);

  Plugin *getPlugin(plugin::LinkerWrapper *LW);

  bool registerReloc(plugin::LinkerWrapper *LW, uint32_t RelocType,
                     std::string Name);

  // ------------------- Script Commands -------------------------
  void addScriptCommand(ScriptCommand *PCommand);

  const std::vector<ScriptCommand *> &getScriptCommands() const {
    return UserLinkerScriptCommands;
  }

  // ------------------ MEMORY Support --------------------------
  MemoryCmd *getMemoryCommand() const { return MemoryCmd; }

  bool hasMemoryCommand() const { return MemoryCmd != nullptr; }

  void setMemoryCommand(MemoryCmd *Cmd) { MemoryCmd = Cmd; }

  bool insertMemoryDescriptor(llvm::StringRef DescName) {
    return !MemoryDescriptors.insert(DescName).second;
  }

  void addMemoryRegion(std::string DescName, eld::ScriptMemoryRegion *M) {
    MMemoryRegionMap[DescName] = M;
    MMemoryRegions.push_back(M);
  }

  eld::Expected<eld::ScriptMemoryRegion *>
  getMemoryRegion(llvm::StringRef DescName, const std::string Context) const;

  eld::Expected<eld::ScriptMemoryRegion *>
  getMemoryRegion(llvm::StringRef DescName) const;

  const llvm::SmallVector<ScriptMemoryRegion *, 4> &getMemoryRegions() const {
    return MMemoryRegions;
  }
  void addPendingRuleInsertion(const plugin::LinkerWrapper *LW,
                               const RuleContainer *R) {
    MPendingRuleInsertions[LW].insert(R);
  }

  void removePendingRuleInsertion(const plugin::LinkerWrapper *LW,
                                  const RuleContainer *R) {
    MPendingRuleInsertions[LW].erase(R);
  }

  const std::unordered_map<const plugin::LinkerWrapper *,
                           std::unordered_set<const RuleContainer *>> &
  getPendingRuleInsertions() const {
    return MPendingRuleInsertions;
  }

  eld::Expected<bool> insertRegionAlias(llvm::StringRef Alias,
                                        const std::string Context);

  eld::Expected<eld::ScriptMemoryRegion *>
  getMemoryRegionForRegionAlias(llvm::StringRef DescName,
                                const std::string Context) const;

  // -----------------------------Saver support ------------------------------
  llvm::StringRef saveString(std::string S);

  /// Loads the plugin P. Loading a plugin means loading the plugin
  /// library, obtaining the plugin object and performing necessary
  /// initialization steps to make the plugin usable.
  ///
  /// Among other things, this function:
  /// - Loads the library which contains the plugin and calls 'RegisterAll'
  ///   function of the library. This step is only performed for the
  ///   first plugin being loaded from the library.
  /// - Calls 'getPlugin' function of the plugin library to obtain the
  ///   user plugin.
  /// - Initializes LinkerWrapper and LinkerPluginConfig for the plugin.
  bool loadPlugin(Plugin &P, Module &M);

private:
  SectionMap *OutputSectionMap = nullptr;
  Assignments LinkerScriptAssignments;
  std::string EntrySymbol;
  PhdrSpecList PhdrList;
  bool HasPhdrsSpecified;
  bool HasPtphdr;
  bool HasSizeOfHeader;
  bool HasFileHeader;
  bool HasProgramHeader;
  bool HasError;
  bool HasSectionsCmd;
  bool HasExternCmd;
  PluginVectorT MPlugins;
  std::atomic<uint64_t> NumWildCardPatterns;
  std::unordered_map<std::string, Plugin *> PluginForOutputSection;
  /// Mapping of plugin library to the first plugin that is loaded from
  /// the library.
  std::unordered_map<std::string, Plugin *> MLibraryToPluginMap;
  llvm::DenseMap<const plugin::LinkerWrapper *,
                 std::vector<ChangeOutputSectionPluginOp *>>
      OverrideSectionMatch;
  std::vector<Plugin *> MRunList;
  llvm::DenseMap<plugin::LinkerWrapper *, Plugin *> MPluginMap;
  llvm::StringMap<std::pair<llvm::TimerGroup *, Name2TimerMap>> Map;
  llvm::StringSet<> MemoryDescriptors;
  llvm::StringSet<> MemoryRegionNameAlias;
  llvm::StringMap<eld::ScriptMemoryRegion *> MMemoryRegionMap;
  llvm::SmallVector<ScriptMemoryRegion *, 4> MMemoryRegions;
  bool HashingEnabled;
  llvm::SHA1 Hasher;
  uint32_t RuleCount;
  std::vector<ScriptCommand *> UserLinkerScriptCommands;
  std::vector<SymbolContainer *> ThisSymbolContainers;
  DiagnosticEngine *Diag = nullptr;
  // Support MEMORY commmand
  MemoryCmd *MemoryCmd = nullptr;
  std::unordered_map<const plugin::LinkerWrapper *,
                     std::unordered_set<const RuleContainer *>>
      MPendingRuleInsertions;
  std::unordered_map<std::string, Plugin *> MPluginInfo;
};

} // namespace eld

#endif
