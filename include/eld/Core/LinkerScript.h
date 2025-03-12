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
  Phdrs(const PhdrSpec &pPhdrDesc) {
    m_Spec.m_Name = pPhdrDesc.m_Name;
    m_Spec.m_Type = pPhdrDesc.m_Type;
    m_Spec.m_HasFileHdr = pPhdrDesc.m_HasFileHdr;
    m_Spec.m_HasPhdr = pPhdrDesc.m_HasPhdr;
    m_Spec.m_AtAddress = pPhdrDesc.m_AtAddress;
    m_Spec.m_Flags = pPhdrDesc.m_Flags;
  }

  const PhdrSpec &spec() const { return m_Spec; }

  const PhdrSpec *getSpec() const { return &m_Spec; }

private:
  PhdrSpec m_Spec;
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

  const PhdrSpecList &phdrList() const { return m_PhdrList; }
  PhdrSpecList &phdrList() { return m_PhdrList; }

  void createSectionMap(LinkerScript &L, const LinkerConfig &config,
                        LayoutPrinter *layoutPrinter);

  SectionMap &sectionMap() const { return *m_SectionMap; }

  const Assignments &assignments() const { return m_Assignments; }
  Assignments &assignments() { return m_Assignments; }

  /// sysroot
  sys::fs::Path &sysroot() const;

  void setSysroot(const sys::fs::Path &pPath);

  void insertPhdrSpec(const PhdrSpec &phdrsSpec);

  void setPhdrsSpecified() { m_hasPhdrsSpecified = true; }

  bool phdrsSpecified() const { return m_hasPhdrsSpecified; }

  void sethasPTPHDR() { m_hasPTPHDR = true; }

  bool hasPTPHDR() const { return m_hasPTPHDR; }

  bool hasSizeOfHeader() const { return m_HasSizeOfHeader; }

  void setSizeOfHeader() { m_HasSizeOfHeader = true; }

  bool hasFileHeader() const { return m_HasFileHeader; }

  void setFileHeader() { m_HasFileHeader = true; }

  bool hasProgramHeader() const { return m_HasProgramHeader; }

  void setProgramHeader() { m_HasProgramHeader = true; }

  void setError() { m_hasError = true; }

  bool hasError() const { return m_hasError; }

  bool linkerScriptHasSectionsCommand() const { return m_HasSectionsCmd; }

  bool hasExternCommand() const { return m_HasExternCmd; }

  bool linkerScriptHasRules() const { return RuleCount > 0; };

  void setHasSectionsCmd() { m_HasSectionsCmd = true; }

  void setHasExternCmd() { m_HasExternCmd = true; }

  // ---------------- Plugin Support ------------------------------
  Plugin *addPlugin(plugin::PluginBase::Type T, std::string Name,
                    std::string PluginRegisterType, std::string PluginOpts,
                    bool S, Module &Module);

  void addPlugin(Plugin *, Module &Module);

  const PluginVectorT &getPlugins() const { return m_Plugins; }

  PluginVectorT getPluginForType(plugin::PluginBase::Type T) const;

  void addPluginToTar(std::string filename, std::string &resolvedPath,
                      OutputTarWriter *outputTar);

  bool loadUniversalPlugins(eld::Module &M);
  bool loadNonUniversalPlugins(eld::Module &M);

  void unloadPlugins(eld::Module *Module);

  void setNumWildCardPatterns(size_t NumPatterns) {
    m_NumWildCardPatterns = NumPatterns;
  }

  size_t getNumWildCardPatterns() const { return m_NumWildCardPatterns; }

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

  void setHashingEnabled() { m_HashingEnabled = true; }
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
  std::vector<Plugin *> &getPluginRunList() { return m_RunList; }

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
    m_PluginMap[Wrapper] = P;
  }

  plugin::LinkerPluginConfig *getLinkerPluginConfig(plugin::LinkerWrapper *LW);

  Plugin *getPlugin(plugin::LinkerWrapper *LW);

  bool registerReloc(plugin::LinkerWrapper *LW, uint32_t RelocType,
                     std::string Name);

  // ------------------- Script Commands -------------------------
  void addScriptCommand(ScriptCommand *pCommand);

  const std::vector<ScriptCommand *> &getScriptCommands() const {
    return m_ScriptCommands;
  }

  // ------------------ MEMORY Support --------------------------
  MemoryCmd *getMemoryCommand() const { return m_MemoryCmd; }

  bool hasMemoryCommand() const { return m_MemoryCmd != nullptr; }

  void setMemoryCommand(MemoryCmd *cmd) { m_MemoryCmd = cmd; }

  bool insertMemoryDescriptor(llvm::StringRef DescName) {
    return !m_MemoryDescriptors.insert(DescName).second;
  }

  void addMemoryRegion(std::string DescName, eld::ScriptMemoryRegion *M) {
    m_MemoryRegionMap[DescName] = M;
    m_MemoryRegions.push_back(M);
  }

  eld::Expected<eld::ScriptMemoryRegion *>
  getMemoryRegion(llvm::StringRef DescName, const std::string Context) const;

  eld::Expected<eld::ScriptMemoryRegion *>
  getMemoryRegion(llvm::StringRef DescName) const;

  const llvm::SmallVector<ScriptMemoryRegion *, 4> &getMemoryRegions() const {
    return m_MemoryRegions;
  }
  void addPendingRuleInsertion(const plugin::LinkerWrapper *LW,
                               const RuleContainer *R) {
    m_PendingRuleInsertions[LW].insert(R);
  }

  void removePendingRuleInsertion(const plugin::LinkerWrapper *LW,
                                  const RuleContainer *R) {
    m_PendingRuleInsertions[LW].erase(R);
  }

  const std::unordered_map<const plugin::LinkerWrapper *,
                           std::unordered_set<const RuleContainer *>> &
  getPendingRuleInsertions() const {
    return m_PendingRuleInsertions;
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
  SectionMap *m_SectionMap = nullptr;
  Assignments m_Assignments;
  std::string m_Entry;
  PhdrSpecList m_PhdrList;
  bool m_hasPhdrsSpecified;
  bool m_hasPTPHDR;
  bool m_HasSizeOfHeader;
  bool m_HasFileHeader;
  bool m_HasProgramHeader;
  bool m_hasError;
  bool m_HasSectionsCmd;
  bool m_HasExternCmd;
  PluginVectorT m_Plugins;
  std::atomic<uint64_t> m_NumWildCardPatterns;
  std::unordered_map<std::string, Plugin *> PluginForOutputSection;
  /// Mapping of plugin library to the first plugin that is loaded from
  /// the library.
  std::unordered_map<std::string, Plugin *> m_LibraryToPluginMap;
  llvm::DenseMap<const plugin::LinkerWrapper *,
                 std::vector<ChangeOutputSectionPluginOp *>>
      m_OverrideSectionMatch;
  std::vector<Plugin *> m_RunList;
  llvm::DenseMap<plugin::LinkerWrapper *, Plugin *> m_PluginMap;
  llvm::StringMap<std::pair<llvm::TimerGroup *, Name2TimerMap>> Map;
  llvm::StringSet<> m_MemoryDescriptors;
  llvm::StringSet<> m_RegionAlias;
  llvm::StringMap<eld::ScriptMemoryRegion *> m_MemoryRegionMap;
  llvm::SmallVector<ScriptMemoryRegion *, 4> m_MemoryRegions;
  bool m_HashingEnabled;
  llvm::SHA1 Hasher;
  uint32_t RuleCount;
  std::vector<ScriptCommand *> m_ScriptCommands;
  std::vector<SymbolContainer *> m_SymbolContainers;
  DiagnosticEngine *m_DiagEngine = nullptr;
  // Support MEMORY commmand
  MemoryCmd *m_MemoryCmd = nullptr;
  std::unordered_map<const plugin::LinkerWrapper *,
                     std::unordered_set<const RuleContainer *>>
      m_PendingRuleInsertions;
  std::unordered_map<std::string, Plugin *> m_PluginInfo;
};

} // namespace eld

#endif
