//===- GeneralOptions.h----------------------------------------------------===//
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
#ifndef ELD_CONFIG_GENERALOPTIONS_H
#define ELD_CONFIG_GENERALOPTIONS_H
#include "eld/Diagnostics/DiagnosticPrinter.h"
#include "eld/Script/StrToken.h"
#include "eld/Script/WildcardPattern.h"
#include "eld/Support/FileSystem.h"
#include "eld/Support/Memory.h"
#include "eld/Support/MsgHandling.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/Regex.h"
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace eld {
class ELFSection;
class Input;
class ZOption;
/** \class GeneralOptions
 *  \brief GeneralOptions collects the options that not be one of the
 *     - input files
 *     - attribute of input files
 */
class GeneralOptions {
public:
  typedef llvm::StringMap<std::string> SymbolRenameMap;

  typedef llvm::StringMap<uint64_t> AddressMapType;

  enum StripSymbolMode {
    KeepAllSymbols,
    StripTemporaries,
    StripLocals,
    StripAllSymbols
  };

  enum class WarnMismatchMode { None, WarnMismatch, NoWarnMismatch };

  enum OrphanMode { Place, Warn, Error, Invalid };

  enum ErrorStyleType { gnu, llvm };

  enum ScriptOptionType { MatchGNU, MatchLLVM };

  enum HashStyle { SystemV = 0x1, GNU = 0x2, Both = 0x3 };

  enum TraceType { T_Files = 0x1, T_Trampolines = 0x2, T_Symbols = 0x4 };

  enum LTOOptionType {
    LTONone = 0x0,
    LTOVerbose = 0x1,
    LTOPreserve = 0x2,
    LTOCodeGen = 0x4,
    LTOAsmOpts = 0x10,
    LTOAsmFileOpt = 0x20,
    LTOOutputFileOpt = 0x40,
    LTODisableLinkOrder = 0x80,
    LTOCacheEnabled = 0x200
  };

  enum SortCommonSymbols { AscendingAlignment, DescendingAlignment };

  enum SortSection { Name, Alignment };

  typedef std::vector<std::string> RpathListType;
  typedef RpathListType::iterator rpath_iterator;
  typedef RpathListType::const_iterator const_rpath_iterator;

  typedef std::vector<std::string> ScriptListType;
  typedef ScriptListType::iterator script_iterator;
  typedef ScriptListType::const_iterator const_script_iterator;

  typedef std::vector<StrToken *> UndefSymListType;
  typedef UndefSymListType::iterator undef_sym_iterator;
  typedef UndefSymListType::const_iterator const_undef_sym_iterator;

  typedef std::set<std::string> ExcludeLIBSType;

  typedef ExcludeLIBSType DynListType;
  typedef DynListType::iterator dyn_list_iterator;
  typedef DynListType::const_iterator const_dyn_list_iterator;

  typedef ExcludeLIBSType ExtList;
  typedef ExtList::iterator ext_list_iterator;
  typedef ExtList::const_iterator const_ext_list_iterator;

  typedef std::unordered_map<const ResolveInfo *,
                             std::vector<std::pair<const InputFile *, bool>>>
      CrefTableType;

  typedef std::vector<std::string> PreserveList;
  typedef std::vector<std::string>::const_iterator StringVectorIterT;

public:
  GeneralOptions(DiagnosticEngine *);
  ~GeneralOptions();

  GeneralOptions() = delete;
  GeneralOptions(const GeneralOptions&) = delete;
  GeneralOptions(GeneralOptions&&) = delete;

  /// stats
  void setStats(llvm::StringRef Stats);

  /// trace
  eld::Expected<void> setTrace(const char *TraceType);

  bool setRequestedTimingRegions(const char *TimingRegion);

  void setTrace(bool EnableTrace);

  bool traceSymbol(std::string const &PSym) const;

  bool traceSymbol(const LDSymbol &Sym, const ResolveInfo &RI) const;

  bool traceSymbol(const ResolveInfo &RI) const;

  bool traceSection(std::string const &PSym) const;

  bool traceSection(const Section *S) const;

  bool traceReloc(std::string const &RelocName) const;

  bool traceLTO(void) const;

  bool codegenOpts(void) const;

  bool asmopts(void) const;

  uint32_t trace() const { return DiagEngine->getPrinter()->trace(); }

  void setBsymbolic(bool PBsymbolic = true) { Bsymbolic = PBsymbolic; }

  bool bsymbolic() const { return Bsymbolic; }

  void setBsymbolicFunctions(bool PBsymbolicFn = true) {
    BsymbolicFunctions = PBsymbolicFn;
  }

  bool bsymbolicFunctions() const { return BsymbolicFunctions; }

  void setPIE(bool PPie = true) { BPIE = PPie; }

  bool isPIE() const { return BPIE; }

  void setBgroup(bool PBgroup = true) { Bgroup = PBgroup; }

  bool bgroup() const { return Bgroup; }

  void setLinkerPath(const std::string &Path) { LinkerPath = Path; }

  const std::string &linkerPath() const { return LinkerPath; }

  void setDyld(const std::string &PDyld) {
    Dyld = PDyld;
    BHasDyld = true;
  }

  std::string soname() const { return SoName; }

  void setSOName(std::string Path) {
    size_t Pos = Path.find_last_of(sys::fs::separator);
    if (std::string::npos == Pos)
      SoName = Path;
    else
      SoName = Path.substr(Pos + 1);
  }

  const std::string &dyld() const { return Dyld; }

  void setDtInit(const std::string &PDtInit) { DtInit = PDtInit; }

  const std::string &dtinit() const { return DtInit; }

  void setDtFini(const std::string &PDtFini) { DtFini = PDtFini; }

  const std::string &dtfini() const { return DtFini; }

  bool hasDyld() const { return BHasDyld; }

  void setOutputFileName(const std::string &PName);

  std::string outputFileName() const {
    if (OutputFileName.has_value())
      return *OutputFileName;
    return "a.out";
  }

  bool hasOutputFileName() const { return OutputFileName.has_value(); }

  void setVerbose(int8_t PVerbose = 1);

  void setColor(bool PEnabled = true) { BColor = PEnabled; }

  bool color() const { return BColor; }

  void setNoUndefined(bool PEnable = true) {
    NoUndefined = (PEnable ? YES : NO);
  }

  void setNoInhibitExec(bool PEnable);

  bool noInhibitExec() const { return NoInhibitExec; }

  bool noGnuStack() const { return NoGnuStack; }

  void setNoTrampolines() { BNoTrampolines = true; }

  bool noTrampolines() const { return BNoTrampolines; }

  void setMulDefs(bool PEnable = true) { MulDefs = (PEnable ? YES : NO); }

  void setWarnOnce(bool PWarn = true) { BWarnOnce = PWarn; }

  bool warnOnce() const { return BWarnOnce; }

  void setEhFrameHdr(bool PEnable = true) {
    BCreateEhFrameHdr = PEnable;
    BCreateEhFrameHdrSet = true;
  }

  ///  -----  the -z options  -----  ///
  bool addZOption(const eld::ZOption &POption);

  bool hasCombReloc() const { return BCombReloc; }

  bool hasNoUndefined() const { return (Unknown != NoUndefined); }

  bool isNoUndefined() const { return (YES == NoUndefined); }

  bool hasStackSet() const { return (Unknown != ExecStack); }

  bool hasExecStack() const { return (YES == ExecStack); }

  bool hasInitFirst() const { return BInitFirst; }

  bool hasMulDefs() const { return (Unknown != MulDefs); }

  bool isMulDefs() const { return (YES == MulDefs); }

  bool hasNoCopyReloc() const { return BNoCopyReloc; }

  bool hasRelro() const { return BRelro; }

  bool hasNow() const { return BNow; }

  void disableNow() { BNow = false; }

  bool hasGlobal() const { return BGlobal; }

  uint64_t commPageSize() const { return *CommPageSize; }

  uint64_t maxPageSize() const { return *MaxPageSize; }

  bool hasMaxPageSize() const {
    if (MaxPageSize)
      return true;
    return false;
  }

  bool hasCommPageSize() const {
    if (CommPageSize)
      return true;
    return false;
  }

  bool hasNoDelete() const { return BNoDelete; }

  bool hasForceBTI() const { return BForceBTI; }

  bool hasForcePACPLT() const { return BForcePACPLT; }

  bool hasEhFrameHdr() const { return BCreateEhFrameHdr; }
  bool isEhFrameHdrSet() const { return BCreateEhFrameHdrSet; }

  // -n, --nmagic
  void setNMagic(bool PMagic = true) { BNMagic = PMagic; }

  bool nmagic() const { return BNMagic; }

  // -N, --omagic
  void setOMagic(bool PMagic = true) { BOMagic = PMagic; }

  bool isOMagic() const { return BOMagic; }

  // -S, --strip-debug
  void setStripDebug(bool PStripDebug = true) { BStripDebug = PStripDebug; }

  bool stripDebug() const { return BStripDebug; }

  // -E, --export-dynamic
  void setExportDynamic(bool PExportDynamic = true) {
    BExportDynamic = PExportDynamic;
  }

  bool exportDynamic() const { return BExportDynamic; }

  // --warn-shared-textrel
  void setWarnSharedTextrel(bool PWarnSharedTextrel = true) {
    BWarnSharedTextrel = PWarnSharedTextrel;
  }

  bool warnSharedTextrel() const { return BWarnSharedTextrel; }

  void setDefineCommon(bool PEnable = true) { BDefineCommon = PEnable; }

  bool isDefineCommon() const { return BDefineCommon; }

  void setFatalWarnings(bool PEnable = true) { BFatalWarnings = PEnable; }

  bool isFatalWarnings() const { return BFatalWarnings; }

  void setWarningsAsErrors(bool PEnable = true) { BWarningsAsErrors = PEnable; }

  bool isWarningsAsErrors() const { return BWarningsAsErrors; }

  void setLTOOptRemarksFile(bool PEnable = false) {
    BLTOOptRemarksFile = PEnable;
  }

  bool hasLTOOptRemarksFile() const { return BLTOOptRemarksFile; }

  void setLTOOptRemarksDisplayHotness(std::string PSym) {
    if (!PSym.empty())
      BLTOOptRemarksDisplayHotness = true;
    else
      BLTOOptRemarksDisplayHotness = false;
  }

  bool hasLTOOptRemarksDisplayHotness() const {
    return BLTOOptRemarksDisplayHotness;
  }

  std::set<std::string> &getExcludeLTOFiles() { return ExcludeLTOFiles; }

  std::set<std::string> &getIncludeLTOFiles() { return IncludeLTOFiles; }

  StripSymbolMode getStripSymbolMode() const { return StripSymbols; }

  void setStripSymbols(StripSymbolMode PMode) { StripSymbols = PMode; }

  void setNoStdlib(bool PEnable = true) { BNoStdlib = PEnable; }

  bool nostdlib() const { return BNoStdlib; }

  void setShared() { HasShared = true; }

  bool hasShared() { return HasShared; }

  void setCref(bool PCref = true) { BCref = PCref; }

  void setNewDTags(bool PEnable = true) { BNewDTags = PEnable; }

  bool hasNewDTags() { return BNewDTags; }

  void setGCCref(std::string PSym) { GcCrefSym = PSym; }

  // LTO Functions, -flto -flto-options
  void setLTO(bool PLto = false) { Lto = PLto; }

  bool hasLTO() const { return Lto; }

  void setLTOOptions(llvm::StringRef OptionType);

  void addLTOCodeGenOptions(std::string O);

  void setSaveTemps(bool PSaveTemps) { Savetemps = PSaveTemps; }

  bool getSaveTemps() const { return Savetemps; }

  void setSaveTempsDir(const std::string &S) { SaveTempsDir = S; }

  const std::optional<std::string> &getSaveTempsDir() const {
    return SaveTempsDir;
  }

  bool preserveAllLTO() const;

  bool preserveSymbolsLTO() const;

  bool disableLTOLinkOrder() const;

  std::vector<llvm::StringRef> getLTOOptionsAsString() const;

  const std::vector<std::string> &getUnparsedLTOOptions() const {
    return UnparsedLTOOptions;
  }

  void getSymbolsFromFile(llvm::StringRef Filename, std::vector<std::string> &);

  void setCopyFarCallsFromFile(std::string File) {
    CopyFarCallsFromFile = File;
  }

  std::string copyFarCallsFromFile() const { return CopyFarCallsFromFile; }

  bool hasNoCopyFarCallsFromFile() const {
    return CopyFarCallsFromFile.empty();
  }

  /// No reuse of trampolines file.
  bool hasNoReuseOfTrampolinesFile() const {
    return NoReuseOfTrampolinesFile.empty();
  }

  std::string noReuseOfTrampolinesFile() const {
    return NoReuseOfTrampolinesFile;
  }

  void setNoReuseOfTrampolinesFile(std::string File) {
    NoReuseOfTrampolinesFile = File;
  }

  bool cref() { return BCref; }

  bool cref() const { return BCref; }

  CrefTableType &crefTable() { return CrefTable; }

  const CrefTableType &crefTable() const { return CrefTable; }

  const std::string &gcCref() const { return GcCrefSym; }

  // --use-move-veneer
  void setUseMovVeneer(bool PEnable = true) { BUseMovVeneer = PEnable; }

  bool getUseMovVeneer() const { return BUseMovVeneer; }

  // -M, --print-map
  void setPrintMap(bool PEnable = true) { BPrintMap = PEnable; }

  bool printMap() const { return BPrintMap; }

  void setWarnMismatch(bool PEnable) {
    if (PEnable) {
      WarnMismatch = WarnMismatchMode::WarnMismatch;
      return;
    }
    WarnMismatch = WarnMismatchMode::NoWarnMismatch;
  }

  bool hasOptionWarnNoWarnMismatch() const {
    return (WarnMismatch != WarnMismatchMode::None);
  }

  bool noWarnMismatch() const {
    return (WarnMismatch == WarnMismatchMode::NoWarnMismatch);
  }

  bool warnMismatch() const {
    return (WarnMismatch == WarnMismatchMode::WarnMismatch);
  }

  // --gc-sections
  void setGCSections(bool PEnable = true) { BGCSections = PEnable; }

  bool gcSections() const { return BGCSections; }

  // --print-gc-sections
  void setPrintGCSections(bool PEnable = true) { BPrintGCSections = PEnable; }

  bool printGCSections() const { return BPrintGCSections; }

  // --plugin-activity-file
  void setPluginActivityLogFile(llvm::StringRef File) {
    PluginActivityLogFile = File.str();
  }

  const std::optional<std::string> &getPluginActivityLogFile() const {
    return PluginActivityLogFile;
  }

  // --ld-generated-unwind-info
  void setGenUnwindInfo(bool PEnable = true) { BGenUnwindInfo = PEnable; }

  bool genUnwindInfo() const { return BGenUnwindInfo; }

  // --Map <file>
  std::string layoutFile() const { return MapFile; }

  void setMapFile(std::string PMapFile) { MapFile = PMapFile; }

  // --TrampolineMap  <file>
  llvm::StringRef getTrampolineMapFile() const { return TrampolineMapFile; }

  void setTrampolineMapFile(llvm::StringRef M) { TrampolineMapFile = M; }

  // -G, max GP size option
  void setGPSize(int Gpsize) { GPSize = Gpsize; }

  int getGPSize() const { return GPSize; }

  // --force-dynamic
  void setForceDynamic() { BForceDynamic = true; }

  bool forceDynamic() const { return BForceDynamic; }

  // --dynamic-list
  void setDynamicList() { BDynamicList = true; }

  bool hasDynamicList() const { return BDynamicList; }

  void setVersionScript() { BVersionScript = true; }

  bool hasVersionScript() const { return BVersionScript; }

  unsigned int getHashStyle() const { return HashStyle; }

  void setHashStyle(std::string HashStyleOption);

  // -----  link-in rpath  ----- //
  const RpathListType &getRpathList() const { return RpathList; }
  RpathListType &getRpathList() { return RpathList; }

  const_rpath_iterator rpathBegin() const { return RpathList.begin(); }
  rpath_iterator rpathBegin() { return RpathList.begin(); }
  const_rpath_iterator rpathEnd() const { return RpathList.end(); }
  rpath_iterator rpathEnd() { return RpathList.end(); }

  // -----  link-in script  ----- //
  const ScriptListType &getScriptList() const { return ScriptList; }
  ScriptListType &getScriptList() { return ScriptList; }

  const_script_iterator scriptBegin() const { return ScriptList.begin(); }
  script_iterator scriptBegin() { return ScriptList.begin(); }
  const_script_iterator scriptEnd() const { return ScriptList.end(); }
  script_iterator scriptEnd() { return ScriptList.end(); }

  // ----  forced undefined symbols ---- //
  const UndefSymListType &getUndefSymList() const { return UndefSymList; }
  UndefSymListType &getUndefSymList() { return UndefSymList; }

  // --- --export-dynamic-symbol
  const UndefSymListType &getExportDynSymList() const {
    return ExportDynSymList;
  }
  UndefSymListType &getExportDynSymList() { return ExportDynSymList; }

  const_undef_sym_iterator undefSymBegin() const {
    return UndefSymList.begin();
  }
  undef_sym_iterator undefSymBegin() { return UndefSymList.begin(); }
  const_undef_sym_iterator undefSymEnd() const { return UndefSymList.end(); }
  undef_sym_iterator undefSymEnd() { return UndefSymList.end(); }

  // ---- add dynamic symbols from list file ---- //
  const DynListType &getDynList() const { return DynList; }
  DynListType &getDynList() { return DynList; }

  const DynListType &getVersionScripts() const { return VersionScripts; }
  DynListType &getVersionScripts() { return VersionScripts; }

  // ---- add extern symbols from list file ---- //
  const ExtList &getExternList() const { return ExternList; }
  ExtList &getExternList() { return ExternList; }

  const_ext_list_iterator extListBegin() const { return ExternList.begin(); }
  const_ext_list_iterator extListEnd() const { return ExternList.end(); }

  const_dyn_list_iterator dynListBegin() const { return DynList.begin(); }
  dyn_list_iterator dynListBegin() { return DynList.begin(); }
  const_dyn_list_iterator dynListEnd() const { return DynList.end(); }
  dyn_list_iterator dynListEnd() { return DynList.end(); }

  // -----  filter and auxiliary filter  ----- //
  void setFilter(const std::string &PFilter) { Filter = PFilter; }

  const std::string &filter() const { return Filter; }

  bool hasFilter() const { return !Filter.empty(); }

  // -----  exclude libs  ----- //
  ExcludeLIBSType &excludeLIBS() { return ExcludeLIBS; }

  bool isInExcludeLIBS(llvm::StringRef ResolvedPath,
                       llvm::StringRef NameSpecPath) const;

  // -- findPos --
  void setMergeStrings(bool MergeStrings) { BMergeStrings = MergeStrings; }

  bool mergeStrings() const { return BMergeStrings; }

  void setEmitRelocs(bool EmitRelocs) {
    BEmitRelocs = EmitRelocs;
    ;
  }

  bool emitRelocs() const { return BEmitRelocs; }

  void setEmitGNUCompatRelocs(bool Value = true) {
    BEmitGNUCompatRelocs = Value;
  }

  bool emitGNUCompatRelocs() const { return BEmitGNUCompatRelocs; }

  // Align segments to a page boundary by default. Use --no-align-segments
  // to disable it.
  void setAlignSegments(bool Align = true) { BPageAlignSegments = Align; }

  bool alignSegmentsToPage() const { return BPageAlignSegments; }

  PreserveList &getPreserveList() { return PreserveCmdLine; }

  llvm::iterator_range<StringVectorIterT> codeGenOpts() const {
    return llvm::make_range(CodegenOpts.cbegin(), CodegenOpts.cend());
  }

  llvm::iterator_range<StringVectorIterT> asmOpts() const {
    return llvm::make_range(AsmOpts.cbegin(), AsmOpts.cend());
  }

  bool hasLTOAsmFile(void) const;

  llvm::iterator_range<StringVectorIterT> ltoAsmFile(void) const;

  void setLTOAsmFile(llvm::StringRef LtoAsmFile);

  bool hasLTOOutputFile(void) const;

  llvm::iterator_range<StringVectorIterT> ltoOutputFile(void) const;

  size_t ltoAsmFileSize() const { return LTOAsmFile.size(); }

  size_t ltoOutputFileSize() const { return LTOOutputFile.size(); }

  void setLTOOutputFile(llvm::StringRef LtoAsmFile);

  void setEmulation(std::string E) { Emulation = E; }

  llvm::StringRef getEmulation() const { return Emulation.c_str(); }

  void setLTOUseAs() { LTOUseAs = true; }

  bool ltoUseAssembler() const { return LTOUseAs; }

  void setLTOOptions(uint32_t LtoOption);

  bool rosegment() const { return Rosegment; }

  void setROSegment(bool R = false) { Rosegment = R; }

  bool verifyLink() const { return Verify; }

  void setVerifyLink(bool V = true) { Verify = V; }

  void setMapFileWithColor(bool Color = false) { Colormap = Color; }

  bool colorMap() const { return Colormap; }

  void setInsertTimingStats(bool T) { InsertTimingStats = T; };

  bool getInsertTimingStats() const { return InsertTimingStats; }

  ErrorStyleType getErrorStyle() const;

  bool setErrorStyle(std::string ErrorStyle);

  ScriptOptionType getScriptOption() const;

  bool setScriptOption(std::string ScriptOptions);

  const SymbolRenameMap &renameMap() const { return SymbolRenames; }
  SymbolRenameMap &renameMap() { return SymbolRenames; }

  const AddressMapType &addressMap() const { return AddressMap; }
  AddressMapType &addressMap() { return AddressMap; }

  /// image base
  const std::optional<uint64_t> &imageBase() const { return ImageBase; }

  void setImageBase(uint64_t Value) { ImageBase = Value; }

  /// entry point
  const std::string &entry() const;

  void setEntry(const std::string &PEntry);

  bool hasEntry() const;

  llvm::ArrayRef<std::string> mapStyle() const { return MapStyles; }

  bool setMapStyle(llvm::StringRef MapStyle);

  void setArgs(std::vector<const char *> &Argv) { CommandLineArgs = Argv; }

  const std::vector<const char *> &args() const { return CommandLineArgs; }

  // Take the ownership of the provided argument list and return a non-owning
  // reference.
  llvm::opt::InputArgList &setParsedArgs(llvm::opt::InputArgList ArgList) {
    ParsedArgs = std::move(ArgList);
    return ParsedArgs;
  }

  llvm::opt::InputArgList &parsedArgs() { return ParsedArgs; }

  // --Threads
  void enableThreads() { EnableThreads = true; }

  void disableThreads() { EnableThreads = false; }

  bool threadsEnabled() const { return EnableThreads; }

  void setNumThreads(int N) { NumThreads = N; }

  int numThreads() const { return NumThreads; }

  // SymDef File.
  void setSymDef(bool Enable = true) { BSymDef = Enable; }

  bool symDef() const { return BSymDef; }

  void setAllowBSSMixing(bool Enable = true) { BAllowBSSMixing = Enable; }
  bool allowBssMixing() const { return BAllowBSSMixing; }

  void setAllowBSSConversion(bool Enable = true) {
    BAllowBSSConversion = Enable;
  }
  bool allowBssConversion() const { return BAllowBSSConversion; }

  void setSymDefFile(std::string S) {
    setSymDef();
    SymDefFile = S;
  }

  std::string symDefFile() const { return SymDefFile; }

  bool setSymDefFileStyle(llvm::StringRef S) {
    SymDefFileStyle = S;
    return (SymDefFileStyle.lower() == "provide" ||
            SymDefFileStyle.lower() == "default");
  }

  llvm::StringRef symDefFileStyle() const { return SymDefFileStyle; }

  bool fixCortexA53Erratum843419() const { return BFixCortexA53Errata843419; }

  void setFixCortexA53Errata843419(bool EnableErrata = true) {
    BFixCortexA53Errata843419 = EnableErrata;
  }

  void setBuildCRef() { BBuildCref = true; }

  bool buildCRef() const { return BBuildCref; }

  void setVerify(llvm::StringRef VerifyType);

  uint32_t verify() const { return DiagEngine->getPrinter()->verify(); }

  std::set<std::string> &verifyRelocList() { return RelocVerify; }

  void setCompact(bool C = true) { Compact = C; }

  bool isCompact() const { return Compact; }

  void setCompactDyn(bool Value = true) { BCompactDyn = Value; }

  bool isCompactDyn() const { return BCompactDyn; }

  // --------------------ROPI/RWPI Support -----------------------------
  bool hasRWPI() const { return BRWPI; }

  void setRWPI() { BRWPI = true; }

  bool hasROPI() const { return BROPI; }

  void setROPI() { BROPI = true; }

  enum class Target2Policy { Abs, Rel, GotRel };

  void setTarget2Policy(Target2Policy Value) { Target2 = Value; }

  Target2Policy getTarget2Policy() const { return Target2; }

  // --------------------AArch64 execute-only Support ------------------

  bool hasExecuteOnlySegments() { return BExecuteOnly; }
  void setExecuteOnlySegments() { BExecuteOnly = true; }

  // -------------------Unresolved Symbol Policy -----------------------
  bool setUnresolvedSymbolPolicy(llvm::StringRef O) {
    ReportUndefPolicy = O;
    return (O == "ignore-all" || O == "report-all" ||
            O == "ignore-in-object-files" || O == "ignore-in-shared-libs");
  }

  bool setOrphanHandlingMode(llvm::StringRef O) {
    MOrphanMode = llvm::StringSwitch<OrphanMode>(O)
                      .CaseLower("error", OrphanMode::Error)
                      .CaseLower("warn", OrphanMode::Warn)
                      .CaseLower("place", OrphanMode::Place)
                      .Default(OrphanMode::Invalid);

    if (MOrphanMode == OrphanMode::Invalid)
      return false;
    return true;
  }

  llvm::StringRef reportUndefPolicy() const { return ReportUndefPolicy; }
  OrphanMode getOrphanMode() const { return MOrphanMode; }

  // ------------------- ThinLTO Cache Support -------------------------
  bool isLTOCacheEnabled() const {
    return LTOOptions & LTOOptionType::LTOCacheEnabled;
  }
  llvm::StringRef getLTOCacheDirectory() const { return LTOCacheDirectory; }

  //--------------------Timing statistics--------------------------------
  // --print-stats
  bool printTimingStats(const char *TimeRegion = nullptr) const;

  void setPrintTimingStats() { BPrintTimeStats = true; }

  bool allUserPluginStatsRequested() const {
    return BPrintAllUserPluginTimeStats;
  }

  // --emit-stats <file>
  std::string timingStatsFile() const { return TimingStatsFile; }

  void setTimingStatsFile(std::string StatsFile) {
    TimingStatsFile = StatsFile;
  }
  //--------------------Plugin Config--------------------------------
  void addPluginConfig(const std::string &Config) {
    PluginConfig.push_back(Config);
  }

  const std::vector<std::string> &getPluginConfig() const {
    return PluginConfig;
  }

  // ------------------Demangle Style----------------------------------
  bool setDemangleStyle(llvm::StringRef Option);

  bool shouldDemangle() const { return BDemangle; }

  // -----------------Arch specific checking --------------------------
  bool validateArchOptions() const { return ValidateArchOpts; }

  void setValidateArchOptions() { ValidateArchOpts = true; }

  void setABIstring(llvm::StringRef ABIStr) { ABIString = ABIStr; }

  llvm::StringRef abiString() const { return ABIString; }

  // ----------------- Disable Guard ------------------------
  void setDisableGuardForWeakUndefs() { DisableGuardForWeakUndefs = true; }

  bool getDisableGuardForWeakUndefs() const {
    return DisableGuardForWeakUndefs;
  }

  void setRISCVRelax(bool Value = true) { BRiscvRelax = Value; }

  bool getRISCVRelax() const { return BRiscvRelax; }

  void setRISCVZeroRelax(bool Relax) { RiscvZeroRelax = Relax; }

  bool getRISCVZeroRelax() const { return RiscvZeroRelax; }

  void setRISCVGPRelax(bool Relax) { RiscvGPRelax = Relax; }

  bool getRISCVGPRelax() const { return RiscvGPRelax; }

  void setRISCVRelaxToC(bool Value = true) { BRiscvRelaxToC = Value; }

  bool getRISCVRelaxToC() const { return BRiscvRelaxToC; }

  void setRISCVRelaxXqci(bool Value) { BRiscvRelaxXqci = Value; }

  bool getRISCVRelaxXqci() const { return BRiscvRelaxXqci; }

  bool warnCommon() const { return BWarnCommon; }

  void setWarnCommon() { BWarnCommon = true; }

  void setAllowIncompatibleSectionsMix(bool F) {
    AllowIncompatibleSectionsMix = F;
  }

  bool allowIncompatibleSectionsMix() const {
    return AllowIncompatibleSectionsMix;
  }

  void setShowProgressBar() { ProgressBar = true; }

  bool showProgressBar() const { return ProgressBar; }

  void setRecordInputfiles() { RecordInputFiles = true; }

  void setCompressTar() { CompressTar = true; }

  bool getCompressTar() const { return CompressTar; }

  void setHasMappingFile(bool HasMap) { HasMappingFile = HasMap; }

  bool hasMappingFile() const { return HasMappingFile; }

  void setMappingFileName(const std::string &File) { MappingFileName = File; }

  const std::string &getMappingFileName() const { return MappingFileName; }

  bool getRecordInputFiles() const { return RecordInputFiles; }

  bool getDumpMappings() const { return DumpMappings; }

  void setDumpMappings(bool Dump) { DumpMappings = Dump; }

  void setMappingDumpFile(const std::string Dump) { MappingDumpFile = Dump; }

  const std::string getMappingDumpFile() const { return MappingDumpFile; }

  const std::string getResponseDumpFile() const { return ResponseDumpFile; }

  void setResponseDumpFile(const std::string Dump) { ResponseDumpFile = Dump; }

  void setDumpResponse(bool Dump) { DumpResponse = true; }

  bool getDumpResponse() const { return DumpResponse; }

  void setTarFile(const std::string Filename) { TarFile = Filename; }

  const std::string getTarFile() const { return TarFile; }

  void setDisplaySummary() { DisplaySummary = true; }

  bool displaySummary() { return DisplaySummary; }

  void setSymbolTracingRequested() { SymbolTracingRequested = true; }

  bool isSymbolTracingRequested() const { return SymbolTracingRequested; }

  void setSectionTracingRequested() { SectionTracingRequested = true; }

  bool isSectionTracingRequested() const { return SectionTracingRequested; }

  // --------------Dynamic Linker-------------------------
  bool hasDynamicLinker() const { return BDynamicLinker; }

  void setHasDynamicLinker(bool Val) { BDynamicLinker = Val; }

  // -------------Default Map Styles ------------------------
  std::string getDefaultMapStyle() const { return DefaultMapStyle; }

  void setDefaultMapStyle(std::string Style) { DefaultMapStyle = Style; }

  bool isDefaultMapStyleText() const;

  bool isDefaultMapStyleYAML() const;

  // --unique-output-sections
  bool shouldEmitUniqueOutputSections() const {
    return EmitUniqueOutputSections;
  }

  void setEmitUniqueOutputSections(bool Emit) {
    EmitUniqueOutputSections = Emit;
  }

  // --reproduce-on-fail support
  void setReproduceOnFail(bool V) { RecordInputFilesOnFail = V; }

  bool isReproduceOnFail() const { return RecordInputFilesOnFail; }

  // -- enable relaxation on hexagon ----
  void enableRelaxation() { BRelaxation = true; }

  bool isLinkerRelaxationEnabled() const { return BRelaxation; }

  void setFatalInternalErrors(bool PEnable) { FatalInternalErrors = PEnable; }

  // Returns true if internal errors should be considered fatal.
  bool isFatalInternalErrors() const { return FatalInternalErrors; }

  // ----------------- --trace-merge-strings options --------------------------
  enum MergeStrTraceType { NONE, ALL, ALLOC, SECTIONS };

  MergeStrTraceType getMergeStrTraceType() const { return MergeStrTraceValue; }

  void addMergeStrTraceSection(const std::string Section) {
    MergeStrSectionsToTrace.push_back(llvm::Regex(Section));
  }

  bool shouldTraceMergeStrSection(const ELFSection *S) const;

  // --trace-linker-script
  bool shouldTraceLinkerScript() const;

  // The return value indicates m_MapStyle modification
  bool checkAndUpdateMapStyleForPrintMap();

  void enableGlobalStringMerge() { GlobalMergeNonAllocStrings = true; }

  bool shouldGlobalStringMerge() const { return GlobalMergeNonAllocStrings; }

  // --keep-labels
  void setKeepLabels() { BKeepLabels = true; }

  bool shouldKeepLabels() const { return BKeepLabels; }

  // --check-sections
  void setEnableCheckSectionOverlaps() { BEnableOverlapChecks = true; }

  // --no-check-sections
  void setDisableCheckSectionOverlaps() { BEnableOverlapChecks = false; }

  bool doCheckOverlaps() const { return BEnableOverlapChecks; }

  // --relax=<regex> support
  bool isLinkerRelaxationEnabled(llvm::StringRef Name) const;

  void addRelaxSection(llvm::StringRef Name);

  void setThinArchiveRuleMatchingCompatibility() {
    ThinArchiveRuleMatchingCompat = true;
  }

  bool isThinArchiveRuleMatchingCompatibilityEnabled() const {
    return ThinArchiveRuleMatchingCompat;
  }

  // --sort-common support
  void setSortCommon() { SortCommon = SortCommonSymbols::DescendingAlignment; }

  bool setSortCommon(llvm::StringRef value) {
    if (value.lower() == "ascending") {
      SortCommon = SortCommonSymbols::AscendingAlignment;
      return true;
    }
    if (value.lower() == "descending") {
      SortCommon = SortCommonSymbols::DescendingAlignment;
      return true;
    }
    return false;
  }

  bool isSortCommonEnabled() const { return !!SortCommon; }

  bool isSortCommonSymbolsAscendingAlignment() const {
    return isSortCommonEnabled() &&
           SortCommon.value() == SortCommonSymbols::AscendingAlignment;
  }

  bool isSortCommonSymbolsDescendingAlignment() const {
    return isSortCommonEnabled() &&
           SortCommon.value() == SortCommonSymbols::DescendingAlignment;
  }

  // --sort-section support
  bool setSortSection(llvm::StringRef value) {
    if (value.lower() == "alignment") {
      SortSection = SortSection::Alignment;
      return true;
    }
    if (value.lower() == "name") {
      SortSection = SortSection::Name;
      return true;
    }
    return false;
  }

  bool isSortSectionEnabled() const { return !!SortSection; }

  bool isSortSectionByName() const {
    return isSortSectionEnabled() && SortSection.value() == SortSection::Name;
  }

  bool isSortSectionByAlignment() const {
    return isSortSectionEnabled() &&
           SortSection.value() == SortSection::Alignment;
  }

  // --print-memory-usage support
  bool shouldPrintMemoryUsage() const { return BPrintMemoryUsage; }

  void setShowPrintMemoryUsage(bool ShowUsage) {
    BPrintMemoryUsage = ShowUsage;
  }

  void setLinkLaunchDirectory(std::string Dir) { LinkLaunchDirectory = Dir; }

  std::string getLinkLaunchDirectory() const { return LinkLaunchDirectory; }

  // -------------------------- Build ID support -------------
  void setDefaultBuildID() { BuildID = true; }

  void setBuildIDValue(llvm::StringRef Val) {
    BuildID = true;
    BuildIDValue = Val;
  }

  bool isBuildIDEnabled() const { return BuildID; }

  bool hasBuildIDValue() const { return !!BuildIDValue; }

  llvm::StringRef getBuildID() const { return BuildIDValue.value(); }

  // --patch-enable support
  void setPatchEnable() { PatchEnable = true; }
  bool isPatchEnable() const { return PatchEnable; }

  void setPatchBase(const std::string &Value) { PatchBase = Value; }
  const std::optional<std::string> &getPatchBase() const { return PatchBase; }

  void setIgnoreUnknownOptions() { IgnoreUnknownOptions = true; }

  bool shouldIgnoreUnknownOptions() const { return IgnoreUnknownOptions; }

  void setUnknownOptions(const std::vector<std::string> &Opts) {
    UnknownOptions = Opts;
  }

  const std::vector<std::string> &getUnknownOptions() const {
    return UnknownOptions;
  }

  void enableShowRMSectNameInDiag() { ShowRMSectNameInDiag = true; }

  bool shouldShowRMSectNameInDiag() const { return ShowRMSectNameInDiag; }

  // default plugins
  bool useDefaultPlugins() const { return UseDefaultPlugins; }

  void setNoDefaultPlugins() { UseDefaultPlugins = false; }

private:
  bool appendMapStyle(const std::string MapStyle);
  enum Status { YES, NO, Unknown };
  std::string LinkerPath;
  std::string DefaultLDScript;
  std::string Dyld;
  std::string DtInit;
  std::string DtFini;
  std::optional<std::string> OutputFileName;
  Status ExecStack = Unknown;           // execstack, noexecstack
  Status NoUndefined = Unknown;         // defs, --no-undefined
  Status MulDefs = Unknown;             // muldefs, --allow-multiple-definition
  std::optional<uint64_t> CommPageSize; // common-page-size=value
  std::optional<uint64_t> MaxPageSize;  // max-page-size=value
  bool BCombReloc = true;               // combreloc, nocombreloc
  bool BGlobal = false;                 // z,global
  bool BInitFirst = false;              // initfirst
  bool BNoCopyReloc = false;            // nocopyreloc
  bool BRelro = false;                  // relro, norelro
  bool BNow = false;                    // lazy, now
  bool Bsymbolic = false;               // --Bsymbolic
  bool BsymbolicFunctions = false;      // --Bsymbolic-functions
  bool Bgroup = false;
  bool BPIE = false;
  bool BColor = true;             // --color[=true,false,auto]
  bool BCreateEhFrameHdr = false; // --eh-frame-hdr
  bool BCreateEhFrameHdrSet = false;
  bool BOMagic = false;            // -N, --omagic
  bool BNMagic = false;            // -n, --nmagic
  bool BStripDebug = false;        // -S, --strip-debug
  bool BExportDynamic = false;     //-E, --export-dynamic
  bool BWarnSharedTextrel = false; // --warn-shared-textrel
  bool BWarnCommon = false;        // --warn-common
  bool BDefineCommon = false;      // -d, -dc, -dp
  bool BFatalWarnings = false;     // --fatal-warnings
  bool BWarningsAsErrors = false;  // -Werror
  bool BLTOOptRemarksFile = false; // --opt-record-file
  bool BLTOOptRemarksDisplayHotness = false; // --display-hotness-remarks
  bool BNoStdlib = false;                    // -nostdlib
  bool BPrintMap = false;                    // --print-map
  WarnMismatchMode WarnMismatch =
      WarnMismatchMode::None;        // --no{-warn}-mismatch
  bool BGCSections = false;          // --gc-sections
  bool BPrintGCSections = false;     // --print-gc-sections
  bool BGenUnwindInfo = true;        // --ld-generated-unwind-info
  bool BForceDynamic = false;        // --force-dynamic
  bool BDynamicList = false;         // --dynamic-list flag
  bool BVersionScript = false;       // --version-script
  bool BHasDyld = false;             // user set dynamic linker ?
  bool NoInhibitExec = false;        //--noinhibit-exec
  bool NoGnuStack = false;           //--nognustack
  bool BNoTrampolines = false;       //--no-trampolines
  bool BMergeStrings = true;         //--merge-strings
  bool BEmitRelocs = false;          //--emit-relocs
  bool BEmitGNUCompatRelocs = false; // --emit-gnu-compat-relocs
  bool BCref = false;                // --cref
  bool BBuildCref = false;           // noflag, buildCRef
  bool BUseMovVeneer = false;        // --use-mov-veneer
  bool BNoDelete = false;            // -z nodelete
  bool BNewDTags = false;            //--enable(disable)-new-dtags
  bool BWarnOnce = false;            // --warn-once
  bool BForceBTI = false;            // -z force-bti
  bool BForcePACPLT = false;         // -z pac-plt
  uint32_t GPSize = 8;               // -G, --gpsize
  bool Lto = false;
  bool LTOUseAs = false;                         // -flto-use-as
  StripSymbolMode StripSymbols = KeepAllSymbols; // Strip symbols ?
  bool BPageAlignSegments = true;   // Does the linker need to align segments to
                                    // a page.
  bool HasShared = false;           // -shared
  unsigned int HashStyle = SystemV; // HashStyle
  bool Savetemps = false;           // -save-temps
  std::optional<std::string> SaveTempsDir; // -save-temps=
  bool Rosegment = false; // merge read only with readonly/execute segments.
  std::vector<std::string>
      UnparsedLTOOptions;          // Unparsed -flto-options, to pass to plugin.
  uint32_t LTOOptions = 0;         // -flto-options
  bool Verify = true;              // Linker verifies output file.
  bool Colormap = false;           // Map file with color.
  bool EnableThreads = true;       // threads enabled ?
  uint32_t NumThreads = 1;         // thread count
  bool BSymDef = false;            // --symdef
  std::string SymDefFile;          // --symdef-file
  llvm::StringRef SymDefFileStyle; // --symdef-style
  bool BAllowBSSMixing = false;    // --disable-bss-mixing
  bool BAllowBSSConversion = false;       // --disable-bss-conversion
  bool BFixCortexA53Errata843419 = false; // --fix-cortex-a53-843419
  bool Compact = false;                   // --compact
  bool BRWPI = false;                     // --frwpi
  bool BROPI = false;                     // --fropi
  Target2Policy Target2 = Target2Policy::GotRel; // --target2
  bool BExecuteOnly = false;              // --execute-only
  bool BPrintTimeStats = false;           // --print-stats
  bool BPrintAllUserPluginTimeStats = false;
  bool BDemangle = true;                  // --demangle-style
  bool ValidateArchOpts = false;          // check -mabi with backend
  bool DisableGuardForWeakUndefs = false; // hexagon specific option to
                                          // disable guard functionality.
  bool BRiscvRelax = true;                // enable riscv relaxation
  bool RiscvZeroRelax = true;             // Zero-page relaxation
  bool RiscvGPRelax = true;               // GP relaxation
  bool BRiscvRelaxToC = true; // enable riscv relax to compressed code
  bool BRiscvRelaxXqci = false; // enable riscv relaxations for xqci
  bool AllowIncompatibleSectionsMix = false; // Allow incompatibleSections;
  bool ProgressBar = false;                  // Show progressbar.
  bool RecordInputFiles = false;             // --reproduce
  bool RecordInputFilesOnFail = false;       // --reproduce-on-fail
  // FIXME: Change the name to CompressReproduceTar
  bool CompressTar = false;         // --reproduce-compressed
  bool DisplaySummary = false;      // display linker run summary
  bool HasMappingFile = false;      // --Mapping-file
  bool DumpMappings = false;        // --Dump-Mapping-file
  bool DumpResponse = false;        // --Dump-Response-file
  bool InsertTimingStats = false;   // -emit-timing-stats-in-output
  bool FatalInternalErrors = false; // --fatal-internal-errors

  RpathListType RpathList;
  ScriptListType ScriptList;
  UndefSymListType UndefSymList;     // -u
  UndefSymListType ExportDynSymList; // --export-dynamic-symbol
  DynListType DynList;               // --dynamic-list files
  DynListType VersionScripts;        // --version-script files
  DynListType ExternList;            // --extern-list files
  std::string Filter;
  std::string MapFile; // Mapfile
  std::string TarFile; // --reproduce output tarfile name
  std::string TimingStatsFile;
  std::optional<std::string> PluginActivityLogFile; // --plugin-activity-file output path
  std::string MappingFileName;           // --Mapping-file
  std::string MappingDumpFile;           // --dump-mapping-file
  std::string ResponseDumpFile;          // --dump-response-file
  llvm::StringMap<int32_t> InputFileMap; // InputFile Map
  std::vector<llvm::Regex> SymbolTrace;
  std::vector<llvm::Regex> RelocTrace;
  std::vector<llvm::Regex> SectionTrace;
  std::vector<std::string> SymbolsToTrace;
  std::vector<std::string> SectionsToTrace;
  std::vector<std::string> RelocsToTrace;
  std::vector<llvm::Regex> MergeStrSectionsToTrace;
  MergeStrTraceType MergeStrTraceValue = MergeStrTraceType::NONE;
  std::set<std::string> RelocVerify;
  std::set<std::string> ExcludeLTOFiles;
  std::set<std::string> IncludeLTOFiles;
  PreserveList PreserveCmdLine;
  std::vector<std::string> CodegenOpts;
  std::vector<std::string> AsmOpts;
  CrefTableType CrefTable;
  std::string GcCrefSym;
  std::string Emulation;
  std::string CopyFarCallsFromFile;
  std::string NoReuseOfTrampolinesFile;
  std::string SoName;
  ExcludeLIBSType ExcludeLIBS;
  ErrorStyleType ErrorStyle = gnu;
  ScriptOptionType ScriptOption = MatchLLVM;
  std::vector<std::string> LTOAsmFile;
  std::vector<std::string> LTOOutputFile;
  bool BCompactDyn = false;          // z,compactdyn
  std::optional<uint64_t> ImageBase; // --image-base=value
  std::string Entry;
  SymbolRenameMap SymbolRenames;
  AddressMapType AddressMap;
  std::vector<const char *> CommandLineArgs;
  // A saved copy of parsed options. These options are saved here *the first
  // time* the driver is created, to be used later. Depending on various
  // factors, the driver may be generic (GnuLdDriver) or target-specific. Since
  // option ID depent on the option table used when parsing, the same option
  // table must be used when accessing these options. In practice, only generic
  // options may be used this way because the initial driver can be the generic
  // one.
  llvm::opt::InputArgList ParsedArgs;
  llvm::StringRef ReportUndefPolicy;
  OrphanMode MOrphanMode = OrphanMode::Place;
  std::string LTOCacheDirectory = "";
  std::vector<std::string> PluginConfig;
  llvm::StringRef ABIString = "";
  llvm::StringRef TrampolineMapFile; // TrampolineMap
  bool SymbolTracingRequested = false;
  bool SectionTracingRequested = false;
  std::vector<llvm::StringRef> RequestedTimeRegions;
  DiagnosticEngine *DiagEngine = nullptr;
  bool BDynamicLinker = true;
  std::string DefaultMapStyle = "txt";
  bool EmitUniqueOutputSections = false; // --unique-output-sections
  bool BRelaxation = false;              // --relaxation
  llvm::SmallVector<std::string, 8> MapStyles;
  bool GlobalMergeNonAllocStrings = false; // --global-merge-non-alloc-strings
  bool BKeepLabels = false;                // --keep-labels (RISC-V)
  bool BEnableOverlapChecks = true; // --check-sections/--no-check-sections
  bool ThinArchiveRuleMatchingCompat = false;
  bool BPrintMemoryUsage = false;              // --print-memory-usage
  std::optional<SortCommonSymbols> SortCommon; // --sort-common
  std::optional<SortSection> SortSection;      // --sort-section
  std::vector<llvm::Regex> RelaxSections;
  bool BuildID = false;
  std::optional<llvm::StringRef> BuildIDValue;
  bool PatchEnable = false;
  std::optional<std::string> PatchBase;
  bool IgnoreUnknownOptions = false;
  std::vector<std::string> UnknownOptions;
  std::string LinkLaunchDirectory;
  bool ShowRMSectNameInDiag = false;
  bool UseDefaultPlugins = true;
};

} // namespace eld

#endif
