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
#include "eld/LayoutMap/LayoutInfo.h"
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
} // namespace llvm

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
    SymbolVersioning,
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

  /// Stages of the link process.
  /// What actions a plugin can perform depends on the
  /// link state. For example:
  /// - Plugins can change the output section of an input
  ///   section only in BeforeLayout link state.
  /// - Plugins can move chunks from one output section to
  ///   another only in CreatingSections link state.
  /// - Plugins can only compute the output image layout checkum using
  ///   `LinkerWrapper::getImageLayoutChecksum` only in AfterLayout linker
  ///   state.
  /// - Plugins can reassign virtual addresses using
  ///   `LinkerWrapper::reassignVirtualAddresses()` only in CreatingSegments
  ///   link state.
  ///
  /// Plugin authors should ensure that the action being performed by the
  /// plugin is meaningful in the link state in which it is executed.
  /// Executing invalid actions for a link state can result in undefined
  /// behavior.
  enum LinkState : uint8_t {
    Unknown,
    Initializing,
    BeforeLayout,
    CreatingSections,
    CreatingSegments,
    AfterLayout,
  };

public:
  explicit Module(LinkerScript &CurScript, LinkerConfig &Config,
                  LayoutInfo *LayoutInfo);

  Module(const std::string &Name, LinkerScript &CurScript, LinkerConfig &Config,
         LayoutInfo *LayoutInfo);

  ~Module();

  LinkerScript &getScript() const { return UserLinkerScript; }

  LinkerConfig &getConfig() const { return ThisConfig; }

  const LinkerScript &getLinkerScript() const { return UserLinkerScript; }

  LinkerScript &getLinkerScript() { return UserLinkerScript; }

  // -----  link-in objects ----- //
  std::vector<InputFile *> &getObjectList() { return InputObjectList; }

  const std::vector<InputFile *> &getObjectList() const {
    return InputObjectList;
  }

  obj_iterator objBegin() { return InputObjectList.begin(); }
  obj_iterator objEnd() { return InputObjectList.end(); }

  void insertLTOObjects(obj_iterator Iter, std::vector<InputFile *> &Inp) {
    InputObjectList.insert(Iter, Inp.begin(), Inp.end());
  }

  // -----  link-in libraries  ----- //
  LibraryList &getDynLibraryList() { return DynLibraryList; }
  LibraryList &getArchiveLibraryList() { return ArchiveLibraryList; }

  /// @}
  /// @name Section Accessors
  /// @{

  // -----  sections  ----- //
  const SectionTable &getSectionTable() const { return OutputSectionTable; }
  SectionTable &getSectionTable() { return OutputSectionTable; }

  void clearOutputSections() {
    OutputSectionTable.clear();
    OutputSectionTableMap.clear();
  }

  void addOutputSectionToTable(ELFSection *S) {
    OutputSectionTableMap[S->name()] = S;
  }

  void addOutputSection(ELFSection *S) {
    OutputSectionTable.push_back(S);
    OutputSectionTableMap[S->name()] = S;
  }

  iterator begin() { return OutputSectionTable.begin(); }
  const_iterator begin() const { return OutputSectionTable.begin(); }
  iterator end() { return OutputSectionTable.end(); }
  const_iterator end() const { return OutputSectionTable.end(); }
  ELFSection *front() { return OutputSectionTable.front(); }
  const ELFSection *front() const { return OutputSectionTable.front(); }
  ELFSection *back() { return OutputSectionTable.back(); }
  const ELFSection *back() const { return OutputSectionTable.back(); }
  size_t size() const { return OutputSectionTable.size(); }
  bool empty() const { return OutputSectionTable.empty(); }

  ELFSection *getSection(const std::string &Name) const;

  // --- Sections with @ --- //
  SectionTable &getAtTable() { return AtTable; }

  /// @}
  /// @name Symbol Accessors
  /// @{

  const NamePool &getNamePool() const { return SymbolNamePool; }
  NamePool &getNamePool() { return SymbolNamePool; }

  // ------ Dynamic List symbols ----//
  ScriptSymbolList &dynListSyms() { return DynamicListSymbols; }

  VersionScriptNodes &getVersionScriptNodes() {
    return LinkerVersionScriptNodes;
  }

  void addVersionScriptNode(const VersionScriptNode *N) {
    LinkerVersionScriptNodes.push_back(N);
  }

  void clear();

  void addToCopyFarCallSet(llvm::StringRef Sym) {
    DuplicateFarCalls.insert(Sym);
  }

  bool findInCopyFarCallSet(llvm::StringRef Sym) const {
    if (DuplicateFarCalls.find(Sym) != DuplicateFarCalls.end())
      return true;
    return false;
  }

  void removeFromCopyFarCallSet(llvm::StringRef Sym) {
    DuplicateFarCalls.erase(DuplicateFarCalls.find(Sym));
  }

  void addToNoReuseOfTrampolines(llvm::StringRef Sym) {
    NoReuseTrampolines.insert(Sym);
  }

  bool findCanReuseTrampolinesForSymbol(llvm::StringRef Sym) const {
    if (NoReuseTrampolines.find(Sym) != NoReuseTrampolines.end())
      return true;
    return false;
  }

  // Find the common symbol recorded previously.
  InputFile *findCommon(std::string Name) const {
    auto It = CommonMap.find(Name);
    if (It == CommonMap.end())
      return nullptr;
    return It->getValue();
  }

  // Record commons as we dont have a section for them.
  void recordCommon(std::string Name, InputFile *I) { CommonMap[Name] = I; }

  void setDotSymbol(LDSymbol *S) { DotSymbol = S; }

  LDSymbol *getDotSymbol() const { return DotSymbol; }

  eld::IRBuilder *getIRBuilder() const;

  void setFailure(bool Fails = false);

  bool linkFail() const { return Failure; }

  Linker *getLinker() const { return Linker; }

  void setLinker(Linker *L) { Linker = L; }

  bool createInternalInputs();

  InputFile *createInternalInputFile(Input *I, bool CreateElfObjectFile);

  InputFile *getInternalInput(InternalInputType Type) const {
    return InternalFiles[Type];
  }

  InternalInputArray &getInternalFiles() { return InternalFiles; }

  InternalInputArray::iterator beginInternalFiles() {
    return std::begin(InternalFiles);
  }

  InternalInputArray::iterator endInternalFiles() {
    return std::end(InternalFiles);
  }

  ELFSection *createOutputSection(const std::string &Name,
                                  LDFileFormat::Kind PKind, uint32_t Type,
                                  uint32_t PFlag, uint32_t PAlign);

  ELFSection *createInternalSection(InputFile &I, LDFileFormat::Kind K,
                                    std::string Name, uint32_t Type,
                                    uint32_t PFlag, uint32_t PAlign,
                                    uint32_t EntSize = 0);

  ELFSection *createInternalSection(InternalInputType Type,
                                    LDFileFormat::Kind K, std::string Name,
                                    uint32_t SectionType, uint32_t Flag,
                                    uint32_t Align, uint32_t EntSize = 0) {
    return createInternalSection(*InternalFiles[Type], K, Name, SectionType,
                                 Flag, Align, EntSize);
  }

  EhFrameHdrSection *createEhFrameHdrSection(InternalInputType IType,
                                             std::string Name, uint32_t Type,
                                             uint32_t PFlag, uint32_t PAlign);

  LayoutInfo *getLayoutInfo() { return ThisLayoutInfo; }

  // Section symbols and all other symbols that live in the output.
  void recordSectionSymbol(ELFSection *S, ResolveInfo *R) {
    SectionSymbol[S] = R;
  }

  ResolveInfo *getSectionSymbol(ELFSection *S) {
    auto Iter = SectionSymbol.find(S);
    if (Iter == SectionSymbol.end())
      return nullptr;
    return Iter->second;
  }

  void addSymbol(ResolveInfo *R);

  LDSymbol *addSymbolFromBitCode(ObjectFile &CurInput, const std::string &Name,
                                 ResolveInfo::Type Type, ResolveInfo::Desc Desc,
                                 ResolveInfo::Binding Binding,
                                 ResolveInfo::SizeType Size,
                                 ResolveInfo::Visibility Visibility,
                                 unsigned int PIdx);

  const std::vector<ResolveInfo *> &getSymbols() const { return Symbols; }

  std::vector<ResolveInfo *> &getSymbols() { return Symbols; }

  // Common symbols.
  void addCommonSymbol(ResolveInfo *R) { CommonSymbols.push_back(R); }

  std::vector<ResolveInfo *> &getCommonSymbols() { return CommonSymbols; }

  bool sortCommonSymbols();

  bool sortSymbols();

  GroupSignatureMap &signatureMap() { return SectionGroupSignatureMap; }

  // ------------------Plugin Support-----------------------------------
  bool readPluginConfig();

  bool updateOutputSectionsWithPlugins();

  // -------------------Linker script symbol and GC support ----------
  void addAssignment(llvm::StringRef SymName, const Assignment *A) {
    AssignmentsLive[SymName] = A;
  }

  const Assignment *getAssignmentForSymbol(llvm::StringRef Sym) {
    auto AssignExpr = AssignmentsLive.find(Sym);
    if (AssignExpr == AssignmentsLive.end())
      return nullptr;
    return AssignExpr->getValue();
  }

  // Save the binding info for symbols taking part in --wrap
  void saveWrapSymBinding(llvm::StringRef Name, uint32_t Binding) {
    WrapBindings[Name] = Binding;
  }

  uint32_t getWrapSymBinding(llvm::StringRef Name) const {
    assert(WrapBindings.count(Name) != 0);
    auto Entry = WrapBindings.find(Name);
    if (Entry != WrapBindings.end())
      return Entry->second;
    return ResolveInfo::NoneBinding;
  }

  void saveWrapReference(llvm::StringRef Name) {
    WrappedReferences.insert(Name);
  }

  bool hasWrapReference(llvm::StringRef Name) const {
    return WrappedReferences.count(Name);
  }

  NoCrossRefSet &getNonRefSections() { return NonRefSections; }

  /* Add support for symbols that need to be selected from archive, if the
   * symbol remains to be undefined */
  void addNeededSymbol(llvm::StringRef S) { NeededSymbols.insert(S); }

  bool hasSymbolInNeededSet(llvm::StringRef S) {
    return (NeededSymbols.find(S) != NeededSymbols.end());
  }

  /// ------------- LTO-related functions ------------------

  /// A flag that is used to check if LTO is really needed.
  bool needLTOToBeInvoked() const { return UsesLto; }

  void setLTONeeded() { UsesLto = true; }

  bool isPostLTOPhase() const;

  /// Set linker state.
  /// On updating the state, this function also checks linker invariants for
  /// the state. It returns true if all the invariants are true; Otherwise it
  /// returns false.
  bool setLinkState(LinkState S);

  LinkState getState() const { return State; }

  llvm::StringRef getStateStr() const;

  void addSymbolCreatedByPluginToFragment(Fragment *F, std::string Name,
                                          uint64_t Val,
                                          const eld::Plugin *Plugin);

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
                                               size_t SectType,
                                               size_t SectFlags,
                                               uint32_t Alignment,
                                               const char *Buf, size_t Size);

  // Get backend
  GNULDBackend &getBackend() const;

  void replaceFragment(FragmentRef *F, const uint8_t *Data, size_t Sz);

  ReplaceFragsVectorT &getReplaceFrags() { return ReplaceFrags; }

  void addPluginFrag(Fragment *);

  /// ------------- Record Plugin Data functionality ------------------
  void recordPluginData(std::string PluginName, uint32_t Key, void *Data,
                        std::string Annotation);

  std::vector<eld::PluginData *> getPluginData(std::string PluginName);

  /// OutputTarWriter get/set
  eld::OutputTarWriter *getOutputTarWriter() { return OutputTar; }

  void createOutputTarWriter();

  DiagnosticPrinter *getPrinter() { return Printer; }

  /// ------------------ Linker Caching Feature --------------------
  void addIntoRuleContainerMap(uint64_t RuleHash, RuleContainer *R) {
    RuleContainerMap[RuleHash] = R;
  }
  RuleContainer *getRuleContainer(uint64_t RuleHash) const {
    auto It = RuleContainerMap.find(RuleHash);
    if (It != RuleContainerMap.end())
      return It->second;
    return nullptr;
  }

  OutputSectionEntry *getOutputSectionEntry(uint64_t OutSectionHash) const {
    auto It = OutputSectionIndexMap.find(OutSectionHash);
    if (It != OutputSectionIndexMap.end())
      return It->second;
    return nullptr;
  }
  void setOutputSectionEntry(uint64_t OutSectionId, OutputSectionEntry *Out) {
    OutputSectionIndexMap[OutSectionId] = Out;
  }

  // -----------------------------Saver support ------------------------------
  llvm::StringRef saveString(std::string S);

  llvm::StringRef saveString(llvm::StringRef S);

  // ----------------------------LayoutPrinters ------------------------------
  TextLayoutPrinter *getTextMapPrinter() const { return TextMapPrinter; }

  YamlLayoutPrinter *getYAMLMapPrinter() const { return YamlMapPrinter; }

  bool createLayoutPrintersForMapStyle(llvm::StringRef);

  bool checkAndRaiseLayoutPrinterDiagEntry(eld::Expected<void> E) const;

  // --------------------------Plugin Memory Buffer Support -----------------
  char *getUninitBuffer(size_t Sz);

  // ---------------------------resetSymbol support -------------------------
  bool resetSymbol(ResolveInfo *, Fragment *F);

  // ---------------------------ImageLayoutChecksum support------------------
  uint64_t getImageLayoutChecksum() const;

  void addVisitedAssignment(std::string S) { VisitedAssignments.insert(S); }

  bool isVisitedAssignment(std::string S) {
    return VisitedAssignments.count(S) > 0;
  }
  // -------------------------Writable Chunks -------------------------------
  bool makeChunkWritable(eld::Fragment *F);

  // ------------------- Relocation data set by plugins ---------------------
  void setRelocationData(const eld::Relocation *, uint64_t);
  bool getRelocationData(const eld::Relocation *, uint64_t &);
  bool getRelocationDataForSync(const eld::Relocation *, uint64_t &);

  void addReferencedSymbol(Section &, ResolveInfo &);

  const ReferencedSymbols &getBitcodeReferencedSymbols() const {
    return BitcodeReferencedSymbols;
  }

  // ---------------------------Central Thread Pool ------------------------
  llvm::ThreadPoolInterface *getThreadPool();

  // ---------------Internal Input Files ------------------------------
  /// Returns the common internal input file.
  InputFile *getCommonInternalInput() const {
    return InternalFiles[InternalInputType::Common];
  }

  /// Create a common section. Common section is an internal input section. Each
  /// common section contains one common symbol.
  CommonELFSection *createCommonELFSection(const std::string &SectionName,
                                           uint32_t Align,
                                           InputFile *OriginatingInputFile);

  MergeableString *getMergedNonAllocString(const MergeableString *S) const {
    ASSERT(!S->isAlloc(), "string is alloc!");
    auto Str = UniqueNonAllocStrings.find(S->String);
    if (Str == UniqueNonAllocStrings.end())
      return nullptr;
    MergeableString *MergedString = Str->second;
    if (MergedString == S)
      return nullptr;
    return MergedString;
  }

  llvm::SmallVectorImpl<MergeableString *> &getNonAllocStrings() {
    return AllNonAllocStrings;
  }

  void addNonAllocString(MergeableString *S) {
    ASSERT(!S->isAlloc(), "string is alloc!");
    AllNonAllocStrings.push_back(S);
    UniqueNonAllocStrings.insert({S->String, S});
  }

  void addScriptSymbolForDynamicListFile(InputFile *DynamicListFile,
                                         ScriptSymbol *Sym) {
    DynamicListFileToScriptSymbolsMap[DynamicListFile].push_back(Sym);
  }

  const llvm::DenseMap<InputFile *, ScriptSymbolList> &
  getDynamicListFileToScriptSymbolsMap() const {
    return DynamicListFileToScriptSymbolsMap;
  }

  void addToOutputSectionDescNameSet(llvm::StringRef Name) {
    OutputSectDescNameSet.insert(Name);
  }

  bool findInOutputSectionDescNameSet(llvm::StringRef Name) {
    return OutputSectDescNameSet.find(Name) != OutputSectDescNameSet.end();
  }

  void addVersionScript(const VersionScript *VerScr) {
    VersionScripts.push_back(VerScr);
  }

  const llvm::SmallVectorImpl<const VersionScript *> &
  getVersionScripts() const {
    return VersionScripts;
  }

  bool isLinkStateBeforeLayout() const {
    return getState() == Module::LinkState::BeforeLayout;
  }

  bool isLinkStateCreatingSections() const {
    return getState() == Module::LinkState::CreatingSections;
  }

  bool isLinkStateCreatingSegments() const {
    return getState() == Module::LinkState::CreatingSegments;
  }

  bool isLinkStateAfterLayout() const {
    return getState() == Module::LinkState::AfterLayout;
  }

  void setFragmentPaddingValue(Fragment *F, uint64_t V);

  std::optional<uint64_t> getFragmentPaddingValue(const Fragment *F) const;

  PluginManager &getPluginManager() { return PM; }

  Section *createBitcodeSection(const std::string &Section, BitcodeFile &File,
                                bool Internal = false);

  bool isBackendInitialized() const;

private:
  /// Verifies invariants of 'CreatingSections' linker state.
  /// Invariants here means the conditions and rules that 'CreatingSections'
  /// state expects to be true.
  /// 'CreatingSections' invariants consists of:
  /// - There should be pending section overrides.
  bool verifyInvariantsForCreatingSectionsState() const;

  // Read one plugin config file
  bool readOnePluginConfig(llvm::StringRef Cfg, bool IsDefaultConfig);



private:
  LinkerScript &UserLinkerScript;
  ObjectList InputObjectList;
  InternalInputArray InternalFiles;
  LibraryList ArchiveLibraryList;
  LibraryList DynLibraryList;
  SectionTable OutputSectionTable;
  llvm::StringMap<ELFSection *> OutputSectionTableMap;
  std::unordered_map<Fragment *, std::vector<LDSymbol *>>
      PluginFragmentToSymbols;
  SectionTable AtTable;
  LinkerConfig &ThisConfig;
  ScriptSymbolList DynamicListSymbols;
  llvm::StringSet<> DuplicateFarCalls;
  llvm::StringSet<> NoReuseTrampolines;
  std::vector<ResolveInfo *> Symbols;
  std::vector<ResolveInfo *> CommonSymbols;
  llvm::DenseMap<ELFSection *, ResolveInfo *> SectionSymbol;
  llvm::StringMap<const Assignment *> AssignmentsLive;
  VersionScriptNodes LinkerVersionScriptNodes;
  llvm::StringMap<InputFile *> CommonMap;
  GroupSignatureMap SectionGroupSignatureMap;
  llvm::StringMap<uint32_t> WrapBindings;
  llvm::StringSet<> WrappedReferences;
  llvm::StringSet<> NeededSymbols;
  NoCrossRefSet NonRefSections;
  LDSymbol *DotSymbol = nullptr;
  Linker *Linker = nullptr;
  LayoutInfo *ThisLayoutInfo = nullptr;
  bool Failure = false;
  bool UsesLto = false;
  LinkState State = LinkState::Unknown;
  ReplaceFragsVectorT ReplaceFrags;
  PluginDataMapT PluginDataMap;
  eld::OutputTarWriter *OutputTar = nullptr;
  eld::DiagnosticPrinter *Printer;
  // -----------Linker Caching Feature -----------------------
  std::unordered_map<uint64_t, RuleContainer *> RuleContainerMap;
  std::unordered_map<uint64_t, OutputSectionEntry *> OutputSectionIndexMap;
  // ------------------Plugin Fragment -----------------------------------
  std::vector<Fragment *> PluginFragments;
  // -------------- StringSaver Support -----------------------------
  llvm::BumpPtrAllocator BAlloc;
  llvm::StringSaver Saver;
  // -----------------Multiple Map file generation support --------------
  TextLayoutPrinter *TextMapPrinter = nullptr;
  YamlLayoutPrinter *YamlMapPrinter = nullptr;
  // ----------------- Use/Def support for linker script --------------
  std::unordered_set<std::string> VisitedAssignments;
  // ----------------- Relocation Data set by plugins ------------------
  std::unordered_map<const eld::Relocation *, uint64_t> RelocationData;
  // ----------------- Section references set by plugins --------------
  ReferencedSymbols BitcodeReferencedSymbols;
  // ----------------- Mutex guard -----------------------------------
  std::mutex Mutex;
  // ----------------- Central thread pool for Linker ---------------
  llvm::ThreadPoolInterface *LinkerThreadPool = nullptr;

  llvm::StringMap<MergeableString *> UniqueNonAllocStrings;
  llvm::SmallVector<MergeableString *> AllNonAllocStrings;
  llvm::DenseMap<InputFile *, ScriptSymbolList>
      DynamicListFileToScriptSymbolsMap;
  llvm::StringSet<> OutputSectDescNameSet;
  llvm::SmallVector<const VersionScript *> VersionScripts;
  llvm::DenseMap<Fragment *, uint64_t> FragmentPaddingValues;
  PluginManager PM;
  NamePool SymbolNamePool;
};

} // namespace eld

#endif
