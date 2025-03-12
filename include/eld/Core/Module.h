//===- Module.h------------------------------------------------------------===//
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
//
// Module contains the intermediate representation (LDIR) of MCLinker.
//
//===----------------------------------------------------------------------===//
#ifndef ELD_CORE_MODULE_H
#define ELD_CORE_MODULE_H

#include "eld/Config/GeneralOptions.h"
#include "eld/Core/Linker.h"
#include "eld/Input/InputFile.h"
#include "eld/LayoutMap/LayoutPrinter.h"
#include "eld/Plugin/PluginManager.h"
#include "eld/PluginAPI/LinkerWrapper.h"
#include "eld/Script/StrToken.h"
#include "eld/Script/VersionScript.h"
#include "eld/Support/OutputTarWriter.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "eld/SymbolResolver/NamePool.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/Hashing.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSet.h"
#include <array>
#include <climits>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace llvm {
class StringSaver;
}

namespace eld {

class BitcodeFile;
class CommonELFSection;
class Input;
class Section;
class LinkerScript;
class ELFSection;
class EhFrameHdrSection;
class LDSymbol;
class Linker;
class Plugin;
class PluginData;
class TextLayoutPrinter;
class YamlLayoutPrinter;
class Relocation;
class ExternCmd;
class ScriptSymbol;

/** \class Module
 *  \brief Module provides the intermediate representation for linking.
 */
class Module {
public:
  typedef enum {
    Attributes,
    BitcodeSections,
    Common,
    CopyRelocSymbols,
    DynamicExports,
    DynamicList,
    DynamicSections,
    EhFrameFiller,
    EhFrameHdr,
    Exception,
    ExternList,
    Guard,
    LinkerVersion,
    OutputSectData,
    Plugin,
    RegionTable,
    Script,
    Sections,
    SectionRelocMap,
    SmallData,
    Timing,
    TLSStub,
    Trampoline,
    GlobalDataSymbols,
    GNUBuildID,
    MAX
  } InternalInputType;

  typedef std::vector<InputFile *> ObjectList;
  typedef ObjectList::iterator obj_iterator;
  typedef ObjectList::const_iterator const_obj_iterator;

  typedef std::vector<InputFile *> LibraryList;
  typedef LibraryList::iterator lib_iterator;
  typedef LibraryList::const_iterator const_lib_iterator;

  typedef std::vector<ELFSection *> SectionTable;
  typedef SectionTable::iterator iterator;
  typedef SectionTable::const_iterator const_iterator;

  typedef std::vector<StrToken *> ListSyms;
  typedef ListSyms::iterator ListSyms_iterator;
  typedef ListSyms::const_iterator const_ListSyms_iterator;

  typedef std::array<InputFile *, MAX> InternalInputArray;

  typedef std::vector<const VersionScriptNode *> VersionScriptNodes;

  typedef llvm::StringMap<ObjectReader::GroupSignatureInfo *> GroupSignatureMap;

  typedef std::unordered_map<std::string, size_t> NoCrossRefSet;

  typedef std::vector<std::pair<FragmentRef *, MemoryArea *>>
      ReplaceFragsVectorT;

  typedef std::unordered_map<std::string, std::vector<PluginData *>>
      PluginDataMapT;

  typedef std::unordered_map<const Section *, std::vector<ResolveInfo *>>
      ReferencedSymbols;

  typedef std::vector<ScriptSymbol *> ScriptSymbolList;

  typedef std::pair<uint64_t, uint64_t> DynamicListStartEndIndexPair;

public:
  explicit Module(LinkerScript &pScript, LinkerConfig &pConfig,
                  LayoutPrinter *layoutPrinter);

  Module(const std::string &pName, LinkerScript &pScript, LinkerConfig &pConfig,
         LayoutPrinter *layoutPrinter);

  ~Module();

  LinkerScript &getScript() const { return m_Script; }

  LinkerConfig &getConfig() const { return m_Config; }

  const LinkerScript &getLinkerScript() const { return m_Script; }

  LinkerScript &getLinkerScript() { return m_Script; }

  // -----  link-in objects ----- //
  std::vector<InputFile *> &getObjectList() { return m_ObjectList; }

  const std::vector<InputFile *> &getObjectList() const { return m_ObjectList; }

  obj_iterator obj_begin() { return m_ObjectList.begin(); }
  obj_iterator obj_end() { return m_ObjectList.end(); }

  void insertLTOObjects(obj_iterator iter, std::vector<InputFile *> &inp) {
    m_ObjectList.insert(iter, inp.begin(), inp.end());
  }

  // -----  link-in libraries  ----- //
  LibraryList &getDynLibraryList() { return m_DynLibraryList; }
  LibraryList &getArchiveLibraryList() { return m_ArchiveLibraryList; }

  /// @}
  /// @name Section Accessors
  /// @{

  // -----  sections  ----- //
  const SectionTable &getSectionTable() const { return m_SectionTable; }
  SectionTable &getSectionTable() { return m_SectionTable; }

  void clearOutputSections() {
    m_SectionTable.clear();
    m_OutputSectionTableMap.clear();
  }

  void addOutputSectionToTable(ELFSection *S) {
    m_OutputSectionTableMap[S->name()] = S;
  }

  void addOutputSection(ELFSection *S) {
    m_SectionTable.push_back(S);
    m_OutputSectionTableMap[S->name()] = S;
  }

  iterator begin() { return m_SectionTable.begin(); }
  const_iterator begin() const { return m_SectionTable.begin(); }
  iterator end() { return m_SectionTable.end(); }
  const_iterator end() const { return m_SectionTable.end(); }
  ELFSection *front() { return m_SectionTable.front(); }
  const ELFSection *front() const { return m_SectionTable.front(); }
  ELFSection *back() { return m_SectionTable.back(); }
  const ELFSection *back() const { return m_SectionTable.back(); }
  size_t size() const { return m_SectionTable.size(); }
  bool empty() const { return m_SectionTable.empty(); }

  ELFSection *getSection(const std::string &pName) const;

  // --- Sections with @ --- //
  SectionTable &getAtTable() { return m_AtTable; }

  /// @}
  /// @name Symbol Accessors
  /// @{

  const NamePool &getNamePool() const { return m_NamePool; }
  NamePool &getNamePool() { return m_NamePool; }

  // ------ Dynamic List symbols ----//
  ScriptSymbolList &dynListSyms() { return m_DynamicListSymbols; }

  VersionScriptNodes &getVersionScriptNodes() { return m_VersionScriptNodes; }

  void addVersionScriptNode(const VersionScriptNode *N) {
    m_VersionScriptNodes.push_back(N);
  }

  void clear();

  void addToCopyFarCallSet(llvm::StringRef sym) {
    m_DuplicateFarCalls.insert(sym);
  }

  bool findInCopyFarCallSet(llvm::StringRef sym) const {
    if (m_DuplicateFarCalls.find(sym) != m_DuplicateFarCalls.end())
      return true;
    return false;
  }

  void removeFromCopyFarCallSet(llvm::StringRef sym) {
    m_DuplicateFarCalls.erase(m_DuplicateFarCalls.find(sym));
  }

  void addToNoReuseOfTrampolines(llvm::StringRef sym) {
    m_NoReuseTrampolines.insert(sym);
  }

  bool findCanReuseTrampolinesForSymbol(llvm::StringRef sym) const {
    if (m_NoReuseTrampolines.find(sym) != m_NoReuseTrampolines.end())
      return true;
    return false;
  }

  // Find the common symbol recorded previously.
  InputFile *findCommon(std::string name) const {
    auto It = m_CommonMap.find(name);
    if (It == m_CommonMap.end())
      return nullptr;
    return It->getValue();
  }

  // Record commons as we dont have a section for them.
  void recordCommon(std::string name, InputFile *I) { m_CommonMap[name] = I; }

  void setDotSymbol(LDSymbol *dotSymbol) { m_DotSymbol = dotSymbol; }

  LDSymbol *getDotSymbol() const { return m_DotSymbol; }

  eld::IRBuilder *getIRBuilder() const;

  void setFailure(bool fails = false);

  bool linkFail() const { return m_Failure; }

  Linker *getLinker() const { return m_pLinker; }

  void setLinker(Linker *l) { m_pLinker = l; }

  bool createInternalInputs();

  InputFile *createInternalInputFile(Input *I, bool createELFObjectFile);

  InputFile *getInternalInput(InternalInputType type) const {
    return m_InternalFiles[type];
  }

  InternalInputArray &getInternalFiles() { return m_InternalFiles; }

  InternalInputArray::iterator beginInternalFiles() {
    return std::begin(m_InternalFiles);
  }

  InternalInputArray::iterator endInternalFiles() {
    return std::end(m_InternalFiles);
  }

  ELFSection *createOutputSection(const std::string &pName,
                                  LDFileFormat::Kind pKind, uint32_t pType,
                                  uint32_t pFlag, uint32_t pAlign);

  ELFSection *createInternalSection(InputFile &I, LDFileFormat::Kind K,
                                    std::string pName, uint32_t pType,
                                    uint32_t pFlag, uint32_t pAlign,
                                    uint32_t entSize = 0);

  ELFSection *createInternalSection(InternalInputType type,
                                    LDFileFormat::Kind K, std::string Name,
                                    uint32_t Type, uint32_t Flag,
                                    uint32_t Align, uint32_t EntSize = 0) {
    return createInternalSection(*m_InternalFiles[type], K, Name, Type, Flag,
                                 Align, EntSize);
  }

  EhFrameHdrSection *createEhFrameHdrSection(InternalInputType type,
                                             std::string pName, uint32_t pType,
                                             uint32_t pFlag, uint32_t pAlign);

  LayoutPrinter *getLayoutPrinter() { return m_LayoutPrinter; }

  // Section symbols and all other symbols that live in the output.
  void recordSectionSymbol(ELFSection *S, ResolveInfo *R) {
    m_SectionSymbol[S] = R;
  }

  ResolveInfo *getSectionSymbol(ELFSection *S) {
    auto Iter = m_SectionSymbol.find(S);
    if (Iter == m_SectionSymbol.end())
      return nullptr;
    return Iter->second;
  }

  void addSymbol(ResolveInfo *R);

  LDSymbol *addSymbolFromBitCode(ObjectFile &pInput, const std::string &pName,
                                 ResolveInfo::Type pType,
                                 ResolveInfo::Desc pDesc,
                                 ResolveInfo::Binding pBinding,
                                 ResolveInfo::SizeType pSize,
                                 ResolveInfo::Visibility pVisibility,
                                 unsigned int pIdx);

  const std::vector<ResolveInfo *> &getSymbols() const { return m_Symbols; }

  std::vector<ResolveInfo *> &getSymbols() { return m_Symbols; }

  // Common symbols.
  void addCommonSymbol(ResolveInfo *R) { m_CommonSymbols.push_back(R); }

  std::vector<ResolveInfo *> &getCommonSymbols() { return m_CommonSymbols; }

  bool sortCommonSymbols();

  bool sortSymbols();

  GroupSignatureMap &signatureMap() { return f_GroupSignatureMap; }

  // ------------------Plugin Support-----------------------------------
  bool readPluginConfig();

  bool updateOutputSectionsWithPlugins();

  // -------------------Linker script symbol and GC support ----------
  void addAssignment(llvm::StringRef symName, const Assignment *A) {
    m_AssignmentsLive[symName] = A;
  }

  const Assignment *getAssignmentForSymbol(llvm::StringRef Sym) {
    auto AssignExpr = m_AssignmentsLive.find(Sym);
    if (AssignExpr == m_AssignmentsLive.end())
      return nullptr;
    return AssignExpr->getValue();
  }

  // Save the binding info for symbols taking part in --wrap
  void saveWrapSymBinding(llvm::StringRef Name, uint32_t Binding) {
    m_WrapBindings[Name] = Binding;
  }

  uint32_t getWrapSymBinding(llvm::StringRef Name) const {
    assert(m_WrapBindings.count(Name) != 0);
    auto Entry = m_WrapBindings.find(Name);
    if (Entry != m_WrapBindings.end())
      return Entry->second;
    return ResolveInfo::NoneBinding;
  }

  void saveWrapReference(llvm::StringRef Name) {
    WrappedReferences.insert(Name);
  }

  bool hasWrapReference(llvm::StringRef Name) const {
    return WrappedReferences.count(Name);
  }

  void addOutputArchOption(llvm::StringRef Option, Expression *expr) {
    m_ArchOptions[Option] = expr;
  }

  void addOutputArchOptionMap(llvm::StringRef K, llvm::StringRef V) {
    OutputArchOptionMap.insert({K, V});
  }

  llvm::StringMap<Expression *> &getOutputArchOptions() {
    return m_ArchOptions;
  }

  llvm::StringMap<llvm::StringRef> &getOutputArchOptionMap() {
    return OutputArchOptionMap;
  }

  NoCrossRefSet &getNonRefSections() { return m_NonRefSections; }

  /* Add support for symbols that need to be selected from archive, if the
   * symbol remains to be undefined */
  void addNeededSymbol(llvm::StringRef S) { NeededSymbols.insert(S); }

  bool hasSymbolInNeededSet(llvm::StringRef S) {
    return (NeededSymbols.find(S) != NeededSymbols.end());
  }

  /// ------------- LTO-related functions ------------------

  /// A flag that is used to check if LTO is really needed.
  bool needLTOToBeInvoked() const { return usesLTO; }

  void setLTONeeded() { usesLTO = true; }

  bool isPostLTOPhase() const;

  /// Set linker state.
  /// On updating the state, this function also checks linker invariants for
  /// the state. It returns true if all the invariants are true; Otherwise it
  /// returns false.
  bool setState(plugin::LinkerWrapper::State S);

  plugin::LinkerWrapper::State getState() const { return m_State; }

  llvm::StringRef getStateStr() const;

  void addSymbolCreatedByPluginToFragment(Fragment *F, std::string Name,
                                          uint64_t Val,
                                          const eld::Plugin *plugin);

  // Create a Plugin Fragment.
  Fragment *createPluginFillFragment(std::string PluginName, uint32_t Alignment,
                                     uint32_t PaddingSize);

  // Create a Code Fragment
  Fragment *createPluginCodeFragment(std::string PluginName, std::string Name,
                                     uint32_t Alignment, const char *Buf,
                                     size_t Sz);

  // Create a Data fragment
  Fragment *createPluginDataFragment(std::string PluginName, std::string Name,
                                     uint32_t Alignment, const char *Buf,
                                     size_t Sz);

  // Create a Data fragment wtih custom section name
  Fragment *
  createPluginDataFragmentWithCustomName(const std::string &PluginName,
                                         std::string Name, uint32_t Alignment,
                                         const char *Buf, size_t Size);

  // Create a .bss fragment
  Fragment *createPluginBSSFragment(std::string PluginName, std::string Name,
                                    uint32_t Alignment, size_t Sz);

  // Create a Note fragment wtih custom section name
  Fragment *createPluginFragmentWithCustomName(std::string Name,
                                               size_t sectType,
                                               size_t sectFlags,
                                               uint32_t Alignment,
                                               const char *Buf, size_t Size);

  // Get backend
  GNULDBackend *getBackend() const;

  void replaceFragment(FragmentRef *F, const uint8_t *Data, size_t Sz);

  ReplaceFragsVectorT &getReplaceFrags() { return ReplaceFrags; }

  void addPluginFrag(Fragment *);

  /// ------------- Record Plugin Data functionality ------------------
  void recordPluginData(std::string PluginName, uint32_t Key, void *Data,
                        std::string Annotation);

  std::vector<eld::PluginData *> getPluginData(std::string PluginName);

  /// OutputTarWriter get/set
  eld::OutputTarWriter *getOutputTarWriter() { return m_OutputTar; }

  void createOutputTarWriter();

  DiagnosticPrinter *getPrinter() { return m_Printer; }

  /// ------------------ Linker Caching Feature --------------------
  void addIntoRuleContainerMap(uint64_t RuleHash, RuleContainer *R) {
    m_RuleContainerMap[RuleHash] = R;
  }
  RuleContainer *getRuleContainer(uint64_t RuleHash) const {
    auto it = m_RuleContainerMap.find(RuleHash);
    if (it != m_RuleContainerMap.end())
      return it->second;
    return nullptr;
  }

  OutputSectionEntry *getOutputSectionEntry(uint64_t OutSectionHash) const {
    auto it = m_OutputSectionIndexMap.find(OutSectionHash);
    if (it != m_OutputSectionIndexMap.end())
      return it->second;
    return nullptr;
  }
  void setOutputSectionEntry(uint64_t OutSectionId, OutputSectionEntry *Out) {
    m_OutputSectionIndexMap[OutSectionId] = Out;
  }

  // -----------------------------Saver support ------------------------------
  llvm::StringRef saveString(std::string S);

  llvm::StringRef saveString(llvm::StringRef S);

  // ----------------------------LayoutPrinters ------------------------------
  TextLayoutPrinter *getTextMapPrinter() const { return m_TextMapPrinter; }

  YamlLayoutPrinter *getYAMLMapPrinter() const { return m_YAMLMapPrinter; }

  bool createLayoutPrintersForMapStyle(llvm::StringRef);

  bool checkAndRaiseLayoutPrinterDiagEntry(eld::Expected<void> E) const;

  // --------------------------Plugin Memory Buffer Support -----------------
  char *getUninitBuffer(size_t Sz);

  // ---------------------------resetSymbol support -------------------------
  bool resetSymbol(ResolveInfo *, Fragment *F);

  // ---------------------------ImageLayoutChecksum support------------------
  uint64_t getImageLayoutChecksum() const;

  void addVisitedAssignment(std::string s) { m_VisitedAssignments.insert(s); }

  bool isVisitedAssignment(std::string s) {
    return m_VisitedAssignments.count(s) > 0;
  }
  // -------------------------Writable Chunks -------------------------------
  bool makeChunkWritable(eld::Fragment *F);

  // ------------------- Relocation data set by plugins ---------------------
  void setRelocationData(const eld::Relocation *, uint64_t);
  bool getRelocationData(const eld::Relocation *, uint64_t &);
  bool getRelocationDataForSync(const eld::Relocation *, uint64_t &);

  void addReferencedSymbol(Section &, ResolveInfo &);

  const ReferencedSymbols &getBitcodeReferencedSymbols() const {
    return m_BitcodeReferencedSymbols;
  }

  // ---------------------------Central Thread Pool ------------------------
  llvm::ThreadPoolInterface *getThreadPool();

  // ---------------Internal Input Files ------------------------------
  /// Returns the common internal input file.
  InputFile *getCommonInternalInput() const {
    return m_InternalFiles[InternalInputType::Common];
  }

  /// Create a common section. Common section is an internal input section. Each
  /// common section contains one common symbol.
  CommonELFSection *createCommonELFSection(const std::string &sectionName,
                                           uint32_t align,
                                           InputFile *originatingInputFile);

  MergeableString *getMergedNonAllocString(const MergeableString *S) const {
    ASSERT(!S->isAlloc(), "string is alloc!");
    auto Str = m_UniqueNonAllocStrings.find(S->String);
    if (Str == m_UniqueNonAllocStrings.end())
      return nullptr;
    MergeableString *MergedString = Str->second;
    if (MergedString == S)
      return nullptr;
    return MergedString;
  }

  llvm::SmallVectorImpl<MergeableString *> &getNonAllocStrings() {
    return m_AllNonAllocStrings;
  }

  void addNonAllocString(MergeableString *S) {
    ASSERT(!S->isAlloc(), "string is alloc!");
    m_AllNonAllocStrings.push_back(S);
    m_UniqueNonAllocStrings.insert({S->String, S});
  }

  void addScriptSymbolForDynamicListFile(InputFile *dynamicListFile,
                                         ScriptSymbol *sym) {
    m_DynamicListFileToScriptSymbolsMap[dynamicListFile].push_back(sym);
  }

  const llvm::DenseMap<InputFile *, ScriptSymbolList> &
  getDynamicListFileToScriptSymbolsMap() const {
    return m_DynamicListFileToScriptSymbolsMap;
  }

  void addToOutputSectionDescNameSet(llvm::StringRef name) {
    m_OutputSectDescNameSet.insert(name);
  }

  bool findInOutputSectionDescNameSet(llvm::StringRef name) {
    return m_OutputSectDescNameSet.find(name) != m_OutputSectDescNameSet.end();
  }

  void addVersionScript(const VersionScript *verScr) {
    m_VersionScripts.push_back(verScr);
  }

  const llvm::SmallVectorImpl<const VersionScript *> &
  getVersionScripts() const {
    return m_VersionScripts;
  }

  bool isBeforeLayoutState() const {
    return getState() == plugin::LinkerWrapper::State::BeforeLayout;
  }

  void setFragmentPaddingValue(Fragment *F, uint64_t V);

  std::optional<uint64_t> getFragmentPaddingValue(const Fragment *F) const;

  PluginManager &getPluginManager() { return PM; }

  Section *createBitcodeSection(const std::string &section, BitcodeFile &File,
                                bool Internal = false);

private:
  /// Verifies invariants of 'CreatingSections' linker state.
  /// Invariants here means the conditions and rules that 'CreatingSections'
  /// state expects to be true.
  /// 'CreatingSections' invariants consists of:
  /// - There should be pending section overrides.
  bool verifyInvariantsForCreatingSectionsState() const;

  // Read one plugin config file
  bool readOnePluginConfig(llvm::StringRef Cfg);

private:
  LinkerScript &m_Script;
  ObjectList m_ObjectList;
  InternalInputArray m_InternalFiles;
  LibraryList m_ArchiveLibraryList;
  LibraryList m_DynLibraryList;
  SectionTable m_SectionTable;
  llvm::StringMap<ELFSection *> m_OutputSectionTableMap;
  std::unordered_map<Fragment *, std::vector<LDSymbol *>>
      m_PluginFragmentToSymbols;
  SectionTable m_AtTable;
  LinkerConfig &m_Config;
  ScriptSymbolList m_DynamicListSymbols;
  llvm::StringSet<> m_DuplicateFarCalls;
  llvm::StringSet<> m_NoReuseTrampolines;
  std::vector<ResolveInfo *> m_Symbols;
  std::vector<ResolveInfo *> m_CommonSymbols;
  llvm::DenseMap<ELFSection *, ResolveInfo *> m_SectionSymbol;
  llvm::StringMap<const Assignment *> m_AssignmentsLive;
  VersionScriptNodes m_VersionScriptNodes;
  llvm::StringMap<InputFile *> m_CommonMap;
  GroupSignatureMap f_GroupSignatureMap;
  llvm::StringMap<uint32_t> m_WrapBindings;
  llvm::StringMap<Expression *> m_ArchOptions;
  llvm::StringMap<llvm::StringRef> OutputArchOptionMap;
  llvm::StringSet<> WrappedReferences;
  llvm::StringSet<> NeededSymbols;
  NoCrossRefSet m_NonRefSections;
  LDSymbol *m_DotSymbol = nullptr;
  Linker *m_pLinker = nullptr;
  GNULDBackend *m_pBackend = nullptr;
  LayoutPrinter *m_LayoutPrinter = nullptr;
  bool m_Failure = false;
  bool usesLTO = false;
  plugin::LinkerWrapper::State m_State = plugin::LinkerWrapper::Unknown;
  ReplaceFragsVectorT ReplaceFrags;
  PluginDataMapT PluginDataMap;
  eld::OutputTarWriter *m_OutputTar = nullptr;
  eld::DiagnosticPrinter *m_Printer;
  // -----------Linker Caching Feature -----------------------
  std::unordered_map<uint64_t, RuleContainer *> m_RuleContainerMap;
  std::unordered_map<uint64_t, OutputSectionEntry *> m_OutputSectionIndexMap;
  // ------------------Plugin Fragment -----------------------------------
  std::vector<Fragment *> m_PluginFragments;
  // -------------- StringSaver Support -----------------------------
  llvm::BumpPtrAllocator BAlloc;
  llvm::StringSaver Saver;
  // -----------------Multiple Map file generation support --------------
  TextLayoutPrinter *m_TextMapPrinter = nullptr;
  YamlLayoutPrinter *m_YAMLMapPrinter = nullptr;
  // ----------------- Use/Def support for linker script --------------
  std::unordered_set<std::string> m_VisitedAssignments;
  // ----------------- Relocation Data set by plugins ------------------
  std::unordered_map<const eld::Relocation *, uint64_t> m_RelocationData;
  // ----------------- Section references set by plugins --------------
  ReferencedSymbols m_BitcodeReferencedSymbols;
  // ----------------- Mutex guard -----------------------------------
  std::mutex m_Mutex;
  // ----------------- Central thread pool for Linker ---------------
  llvm::ThreadPoolInterface *m_ThreadPool = nullptr;

  llvm::StringMap<MergeableString *> m_UniqueNonAllocStrings;
  llvm::SmallVector<MergeableString *> m_AllNonAllocStrings;
  llvm::DenseMap<InputFile *, ScriptSymbolList>
      m_DynamicListFileToScriptSymbolsMap;
  llvm::StringSet<> m_OutputSectDescNameSet;
  llvm::SmallVector<const VersionScript *> m_VersionScripts;
  llvm::DenseMap<Fragment *, uint64_t> FragmentPaddingValues;
  PluginManager PM;
  NamePool m_NamePool;
};

} // namespace eld

#endif
