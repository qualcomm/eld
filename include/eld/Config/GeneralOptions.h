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

  typedef llvm::StringMap<uint64_t> AddressMap;

  enum StripSymbolMode {
    KeepAllSymbols,
    StripTemporaries,
    StripLocals,
    StripAllSymbols
  };

  enum WarnMismatchMode { None, WarnMismatch, NoWarnMismatch };

  enum OrphanMode { Place, Warn, Error, Invalid };

  enum ErrorStyle { gnu, llvm };

  enum ScriptOption { MatchGNU, MatchLLVM };

  enum HashStyle { SystemV = 0x1, GNU = 0x2, Both = 0x3 };

  enum TraceType { T_Files = 0x1, T_Trampolines = 0x2, T_Symbols = 0x4 };

  enum ltoOptions {
    LTONone = 0x0,
    LTOVerbose = 0x1,
    LTOPreserve = 0x2,
    LTOCodeGen = 0x4,
    LTOAsmOpts = 0x10,
    LTOAsmFile = 0x20,
    LTOOutputFile = 0x40,
    LTODisableLinkOrder = 0x80,
    LTOCacheEnabled = 0x200
  };

  enum SortCommonSymbols { AscendingAlignment, DescendingAlignment };

  enum SortSection { Name, Alignment };

  typedef std::vector<std::string> RpathList;
  typedef RpathList::iterator rpath_iterator;
  typedef RpathList::const_iterator const_rpath_iterator;

  typedef std::vector<std::string> ScriptList;
  typedef ScriptList::iterator script_iterator;
  typedef ScriptList::const_iterator const_script_iterator;

  typedef std::vector<StrToken *> UndefSymList;
  typedef UndefSymList::iterator undef_sym_iterator;
  typedef UndefSymList::const_iterator const_undef_sym_iterator;

  typedef std::set<std::string> ExcludeLIBS;

  typedef ExcludeLIBS DynList;
  typedef DynList::iterator dyn_list_iterator;
  typedef DynList::const_iterator const_dyn_list_iterator;

  typedef ExcludeLIBS ExtList;
  typedef ExtList::iterator ext_list_iterator;
  typedef ExtList::const_iterator const_ext_list_iterator;

  typedef std::unordered_map<const ResolveInfo *,
                             std::vector<std::pair<const InputFile *, bool>>>
      CrefTable;

  typedef std::vector<std::string> PreserveList;
  typedef std::vector<std::string>::const_iterator StringVectorIterT;

public:
  GeneralOptions(DiagnosticEngine *);
  ~GeneralOptions();

  /// stats
  void setStats(llvm::StringRef stats);

  /// trace
  eld::Expected<void> setTrace(const char *traceType);

  bool setRequestedTimingRegions(const char *timingRegion);

  void setTrace(bool enableTrace);

  bool traceSymbol(std::string const &pSym) const;

  bool traceSymbol(const LDSymbol &sym, const ResolveInfo &RI) const;

  bool traceSymbol(const ResolveInfo &RI) const;

  bool traceSection(std::string const &pSym) const;

  bool traceSection(const Section *S) const;

  bool traceReloc(std::string const &relocName) const;

  bool traceLTO(void) const;

  bool codegenOpts(void) const;

  bool asmopts(void) const;

  uint32_t trace() const { return m_DiagEngine->getPrinter()->trace(); }

  void setBsymbolic(bool pBsymbolic = true) { m_Bsymbolic = pBsymbolic; }

  bool Bsymbolic() const { return m_Bsymbolic; }

  void setBsymbolicFunctions(bool pBsymbolicFn = true) {
    m_BsymbolicFunctions = pBsymbolicFn;
  }

  bool BsymbolicFunctions() const { return m_BsymbolicFunctions; }

  void setPIE(bool pPIE = true) { m_bPIE = pPIE; }

  bool isPIE() const { return m_bPIE; }

  void setBgroup(bool pBgroup = true) { m_Bgroup = pBgroup; }

  bool Bgroup() const { return m_Bgroup; }

  void setLinkerPath(const std::string &path) { m_LinkerPath = path; }

  const std::string &linkerPath() const { return m_LinkerPath; }

  void setDyld(const std::string &pDyld) {
    m_Dyld = pDyld;
    m_bHasDyld = true;
  }

  std::string soname() const { return m_SoName; }

  void setSOName(std::string path) {
    size_t pos = path.find_last_of(sys::fs::separator);
    if (std::string::npos == pos)
      m_SoName = path;
    else
      m_SoName = path.substr(pos + 1);
  }

  const std::string &dyld() const { return m_Dyld; }

  void setDtInit(const std::string &pDtInit) { m_DtInit = pDtInit; }

  const std::string &dtinit() const { return m_DtInit; }

  void setDtFini(const std::string &pDtFini) { m_DtFini = pDtFini; }

  const std::string &dtfini() const { return m_DtFini; }

  bool hasDyld() const { return m_bHasDyld; }

  void setOutputFileName(const std::string &pName);

  std::string outputFileName() const {
    if (m_OutputFileName.has_value())
      return *m_OutputFileName;
    return "a.out";
  }

  bool hasOutputFileName() const { return m_OutputFileName.has_value(); }

  void setVerbose(int8_t pVerbose = 1);

  void setColor(bool pEnabled = true) { m_bColor = pEnabled; }

  bool color() const { return m_bColor; }

  void setNoUndefined(bool pEnable = true) {
    m_NoUndefined = (pEnable ? YES : NO);
  }

  void setNoInhibitExec(bool pEnable);

  bool noInhibitExec() const { return m_NoInhibitExec; }

  bool noGnuStack() const { return m_NoGnuStack; }

  void setNoTrampolines() { m_bNoTrampolines = true; }

  bool noTrampolines() const { return m_bNoTrampolines; }

  void setMulDefs(bool pEnable = true) { m_MulDefs = (pEnable ? YES : NO); }

  void setWarnOnce(bool pWarn = true) { m_bWarnOnce = pWarn; }

  bool warnOnce() const { return m_bWarnOnce; }

  void setEhFrameHdr(bool pEnable = true) {
    m_bCreateEhFrameHdr = pEnable;
    m_bCreateEhFrameHdrSet = true;
  }

  ///  -----  the -z options  -----  ///
  bool addZOption(const eld::ZOption &pOption);

  bool hasCombReloc() const { return m_bCombReloc; }

  bool hasNoUndefined() const { return (Unknown != m_NoUndefined); }

  bool isNoUndefined() const { return (YES == m_NoUndefined); }

  bool hasStackSet() const { return (Unknown != m_ExecStack); }

  bool hasExecStack() const { return (YES == m_ExecStack); }

  bool hasInitFirst() const { return m_bInitFirst; }

  bool hasMulDefs() const { return (Unknown != m_MulDefs); }

  bool isMulDefs() const { return (YES == m_MulDefs); }

  bool hasNoCopyReloc() const { return m_bNoCopyReloc; }

  bool hasRelro() const { return m_bRelro; }

  bool hasNow() const { return m_bNow; }

  void disableNow() { m_bNow = false; }

  bool hasGlobal() const { return m_bGlobal; }

  uint64_t commPageSize() const { return *m_CommPageSize; }

  uint64_t maxPageSize() const { return *m_MaxPageSize; }

  bool hasMaxPageSize() const {
    if (m_MaxPageSize)
      return true;
    return false;
  }

  bool hasCommPageSize() const {
    if (m_CommPageSize)
      return true;
    return false;
  }

  bool hasNoDelete() const { return m_bNoDelete; }

  bool hasForceBTI() const { return m_bForceBTI; }

  bool hasForcePACPLT() const { return m_bForcePACPLT; }

  bool hasEhFrameHdr() const { return m_bCreateEhFrameHdr; }
  bool isEhFrameHdrSet() const { return m_bCreateEhFrameHdrSet; }

  // -n, --nmagic
  void setNMagic(bool pMagic = true) { m_bNMagic = pMagic; }

  bool nmagic() const { return m_bNMagic; }

  // -N, --omagic
  void setOMagic(bool pMagic = true) { m_bOMagic = pMagic; }

  bool omagic() const { return m_bOMagic; }

  // -S, --strip-debug
  void setStripDebug(bool pStripDebug = true) { m_bStripDebug = pStripDebug; }

  bool stripDebug() const { return m_bStripDebug; }

  // -E, --export-dynamic
  void setExportDynamic(bool pExportDynamic = true) {
    m_bExportDynamic = pExportDynamic;
  }

  bool exportDynamic() const { return m_bExportDynamic; }

  // --warn-shared-textrel
  void setWarnSharedTextrel(bool pWarnSharedTextrel = true) {
    m_bWarnSharedTextrel = pWarnSharedTextrel;
  }

  bool warnSharedTextrel() const { return m_bWarnSharedTextrel; }

  void setBinaryInput(bool pBinaryInput = true) {
    m_bBinaryInput = pBinaryInput;
  }

  bool isBinaryInput() const { return m_bBinaryInput; }

  void setDefineCommon(bool pEnable = true) { m_bDefineCommon = pEnable; }

  bool isDefineCommon() const { return m_bDefineCommon; }

  void setFatalWarnings(bool pEnable = true) { m_bFatalWarnings = pEnable; }

  bool isFatalWarnings() const { return m_bFatalWarnings; }

  void setLTOOptRemarksFile(bool pEnable = false) {
    m_bLTOOptRemarksFile = pEnable;
  }

  bool hasLTOOptRemarksFile() const { return m_bLTOOptRemarksFile; }

  void setLTOOptRemarksDisplayHotness(std::string pSym) {
    if (!pSym.empty())
      m_bLTOOptRemarksDisplayHotness = true;
    else
      m_bLTOOptRemarksDisplayHotness = false;
  }

  bool hasLTOOptRemarksDisplayHotness() const {
    return m_bLTOOptRemarksDisplayHotness;
  }

  std::set<std::string> &getExcludeLTOFiles() { return m_ExcludeLTOFiles; }

  std::set<std::string> &getIncludeLTOFiles() { return m_IncludeLTOFiles; }

  StripSymbolMode getStripSymbolMode() const { return m_StripSymbols; }

  void setStripSymbols(StripSymbolMode pMode) { m_StripSymbols = pMode; }

  void setNoStdlib(bool pEnable = true) { m_bNoStdlib = pEnable; }

  bool nostdlib() const { return m_bNoStdlib; }

  void setShared() { m_hasShared = true; }

  bool hasShared() { return m_hasShared; }

  void setCref(bool pCref = true) { m_bCref = pCref; }

  void setNewDTags(bool pEnable = true) { m_bNewDTags = pEnable; }

  bool hasNewDTags() { return m_bNewDTags; }

  void setGCCref(std::string pSym) { GcCrefSym = pSym; }

  // LTO Functions, -flto -flto-options
  void setLTO(bool pLTO = false) { m_lto = pLTO; }

  bool hasLTO() const { return m_lto; }

  void setLTOOptions(llvm::StringRef optionType);

  void addLTOCodeGenOptions(std::string O);

  void setSaveTemps(bool pSaveTemps) { m_savetemps = pSaveTemps; }

  bool getSaveTemps() const { return m_savetemps; }

  void setSaveTempsDir(const std::string &S) { m_saveTempsDir = S; }

  const std::optional<std::string> &getSaveTempsDir() const {
    return m_saveTempsDir;
  }

  bool preserveAllLTO() const;

  bool preserveSymbolsLTO() const;

  bool disableLTOLinkOrder() const;

  std::vector<llvm::StringRef> getLTOOptionsAsString() const;

  const std::vector<std::string> &getUnparsedLTOOptions() const {
    return m_UnparsedLTOOptions;
  }

  void getSymbolsFromFile(llvm::StringRef filename, std::vector<std::string> &);

  void setCopyFarCallsFromFile(std::string file) {
    m_CopyFarCallsFromFile = file;
  }

  std::string copyFarCallsFromFile() const { return m_CopyFarCallsFromFile; }

  bool hasNoCopyFarCallsFromFile() const {
    return m_CopyFarCallsFromFile.empty();
  }

  /// No reuse of trampolines file.
  bool hasNoReuseOfTrampolinesFile() const {
    return m_NoReuseOfTrampolinesFile.empty();
  }

  std::string noReuseOfTrampolinesFile() const {
    return m_NoReuseOfTrampolinesFile;
  }

  void setNoReuseOfTrampolinesFile(std::string file) {
    m_NoReuseOfTrampolinesFile = file;
  }

  bool cref() { return m_bCref; }

  bool cref() const { return m_bCref; }

  CrefTable &crefTable() { return m_crefTable; }

  const CrefTable &crefTable() const { return m_crefTable; }

  const std::string &gcCref() const { return GcCrefSym; }

  // --use-move-veneer
  void setUseMovVeneer(bool pEnable = true) { m_bUseMovVeneer = pEnable; }

  bool getUseMovVeneer() const { return m_bUseMovVeneer; }

  // -M, --print-map
  void setPrintMap(bool pEnable = true) { m_bPrintMap = pEnable; }

  bool printMap() const { return m_bPrintMap; }

  void setWarnMismatch(bool pEnable) {
    if (pEnable) {
      m_WarnMismatch = WarnMismatch;
      return;
    }
    m_WarnMismatch = NoWarnMismatch;
  }

  bool hasOptionWarnNoWarnMismatch() const { return (m_WarnMismatch != None); }

  bool noWarnMismatch() const { return (m_WarnMismatch == NoWarnMismatch); }

  bool warnMismatch() const { return (m_WarnMismatch == WarnMismatch); }

  // --gc-sections
  void setGCSections(bool pEnable = true) { m_bGCSections = pEnable; }

  bool GCSections() const { return m_bGCSections; }

  // --print-gc-sections
  void setPrintGCSections(bool pEnable = true) { m_bPrintGCSections = pEnable; }

  bool printGCSections() const { return m_bPrintGCSections; }

  // --ld-generated-unwind-info
  void setGenUnwindInfo(bool pEnable = true) { m_bGenUnwindInfo = pEnable; }

  bool genUnwindInfo() const { return m_bGenUnwindInfo; }

  // --Map <file>
  std::string layoutFile() const { return m_MapFile; }

  void setMapFile(std::string mapFile) { m_MapFile = mapFile; }

  // --TrampolineMap  <file>
  llvm::StringRef getTrampolineMapFile() const { return TrampolineMapFile; }

  void setTrampolineMapFile(llvm::StringRef M) { TrampolineMapFile = M; }

  // -G, max GP size option
  void setGPSize(int gpsize) { m_GPSize = gpsize; }

  int getGPSize() const { return m_GPSize; }

  // --force-dynamic
  void setForceDynamic() { m_bForceDynamic = true; }

  bool forceDynamic() const { return m_bForceDynamic; }

  // --dynamic-list
  void setDynamicList() { m_bDynamicList = true; }

  bool hasDynamicList() const { return m_bDynamicList; }

  void setVersionScript() { m_bVersionScript = true; }

  bool hasVersionScript() const { return m_bVersionScript; }

  unsigned int getHashStyle() const { return m_HashStyle; }

  void setHashStyle(std::string hashStyle);

  // -----  link-in rpath  ----- //
  const RpathList &getRpathList() const { return m_RpathList; }
  RpathList &getRpathList() { return m_RpathList; }

  const_rpath_iterator rpath_begin() const { return m_RpathList.begin(); }
  rpath_iterator rpath_begin() { return m_RpathList.begin(); }
  const_rpath_iterator rpath_end() const { return m_RpathList.end(); }
  rpath_iterator rpath_end() { return m_RpathList.end(); }

  // -----  link-in script  ----- //
  const ScriptList &getScriptList() const { return m_ScriptList; }
  ScriptList &getScriptList() { return m_ScriptList; }

  const_script_iterator script_begin() const { return m_ScriptList.begin(); }
  script_iterator script_begin() { return m_ScriptList.begin(); }
  const_script_iterator script_end() const { return m_ScriptList.end(); }
  script_iterator script_end() { return m_ScriptList.end(); }

  // ----  forced undefined symbols ---- //
  const UndefSymList &getUndefSymList() const { return m_UndefSymList; }
  UndefSymList &getUndefSymList() { return m_UndefSymList; }

  // --- --export-dynamic-symbol
  const UndefSymList &getExportDynSymList() const { return m_ExportDynSymList; }
  UndefSymList &getExportDynSymList() { return m_ExportDynSymList; }

  const_undef_sym_iterator undef_sym_begin() const {
    return m_UndefSymList.begin();
  }
  undef_sym_iterator undef_sym_begin() { return m_UndefSymList.begin(); }
  const_undef_sym_iterator undef_sym_end() const {
    return m_UndefSymList.end();
  }
  undef_sym_iterator undef_sym_end() { return m_UndefSymList.end(); }

  // ---- add dynamic symbols from list file ---- //
  const DynList &getDynList() const { return m_DynList; }
  DynList &getDynList() { return m_DynList; }

  const DynList &getVersionScripts() const { return m_VersionScripts; }
  DynList &getVersionScripts() { return m_VersionScripts; }

  // ---- add extern symbols from list file ---- //
  const ExtList &getExternList() const { return m_ExternList; }
  ExtList &getExternList() { return m_ExternList; }

  const_ext_list_iterator ext_list_begin() const {
    return m_ExternList.begin();
  }
  const_ext_list_iterator ext_list_end() const { return m_ExternList.end(); }

  const_dyn_list_iterator dyn_list_begin() const { return m_DynList.begin(); }
  dyn_list_iterator dyn_list_begin() { return m_DynList.begin(); }
  const_dyn_list_iterator dyn_list_end() const { return m_DynList.end(); }
  dyn_list_iterator dyn_list_end() { return m_DynList.end(); }

  // -----  filter and auxiliary filter  ----- //
  void setFilter(const std::string &pFilter) { m_Filter = pFilter; }

  const std::string &filter() const { return m_Filter; }

  bool hasFilter() const { return !m_Filter.empty(); }

  // -----  exclude libs  ----- //
  ExcludeLIBS &excludeLIBS() { return m_ExcludeLIBS; }

  bool isInExcludeLIBS(llvm::StringRef ResolvedPath,
                       llvm::StringRef NameSpecPath) const;

  // -- findPos --
  void setMergeStrings(bool mergeStrings) { m_bMergeStrings = mergeStrings; }

  bool mergeStrings() const { return m_bMergeStrings; }

  void setEmitRelocs(bool emitRelocs) {
    m_bEmitRelocs = emitRelocs;
    ;
  }

  bool emitRelocs() const { return m_bEmitRelocs; }

  void setEmitGNUCompatRelocs(bool value = true) {
    m_bEmitGNUCompatRelocs = value;
  }

  bool emitGNUCompatRelocs() const { return m_bEmitGNUCompatRelocs; }

  // Align segments to a page boundary by default. Use --no-align-segments
  // to disable it.
  void setAlignSegments(bool align = true) { m_bPageAlignSegments = align; }

  bool alignSegmentsToPage() const { return m_bPageAlignSegments; }

  PreserveList &getPreserveList() { return m_PreserveCmdLine; }

  llvm::iterator_range<StringVectorIterT> codeGenOpts() const {
    return llvm::make_range(m_codegenOpts.cbegin(), m_codegenOpts.cend());
  }

  llvm::iterator_range<StringVectorIterT> asmOpts() const {
    return llvm::make_range(m_asmOpts.cbegin(), m_asmOpts.cend());
  }

  bool hasLTOAsmFile(void) const;

  llvm::iterator_range<StringVectorIterT> ltoAsmFile(void) const;

  void setLTOAsmFile(llvm::StringRef ltoAsmFile);

  bool hasLTOOutputFile(void) const;

  llvm::iterator_range<StringVectorIterT> ltoOutputFile(void) const;

  size_t ltoAsmFileSize() const { return m_LTOAsmFile.size(); }

  size_t ltoOutputFileSize() const { return m_LTOOutputFile.size(); }

  void setLTOOutputFile(llvm::StringRef ltoAsmFile);

  void setEmulation(std::string emulation) { m_Emulation = emulation; }

  llvm::StringRef getEmulation() const { return m_Emulation.c_str(); }

  void setLTOUseAs() { m_LTOUseAs = true; }

  bool ltoUseAssembler() const { return m_LTOUseAs; }

  void setLTOOptions(uint32_t ltoOption);

  bool rosegment() const { return m_rosegment; }

  void setROSegment(bool rosegment = false) { m_rosegment = rosegment; }

  bool verifyLink() const { return m_verify; }

  void setVerifyLink(bool verify = true) { m_verify = verify; }

  void setMapFileWithColor(bool color = false) { m_colormap = color; }

  bool colorMap() const { return m_colormap; }

  void setInsertTimingStats(bool t) { m_InsertTimingStats = t; };

  bool getInsertTimingStats() const { return m_InsertTimingStats; }

  ErrorStyle getErrorStyle() const;

  bool setErrorStyle(std::string errorStyle);

  ScriptOption getScriptOption() const;

  bool setScriptOption(std::string scriptOptions);

  const SymbolRenameMap &renameMap() const { return m_SymbolRenames; }
  SymbolRenameMap &renameMap() { return m_SymbolRenames; }

  const AddressMap &addressMap() const { return m_AddressMap; }
  AddressMap &addressMap() { return m_AddressMap; }

  /// image base
  const std::optional<uint64_t> &imageBase() const { return m_ImageBase; }

  void setImageBase(uint64_t Value) { m_ImageBase = Value; }

  /// entry point
  const std::string &entry() const;

  void setEntry(const std::string &pEntry);

  bool hasEntry() const;

  llvm::ArrayRef<std::string> mapStyle() const { return m_MapStyles; }

  bool setMapStyle(llvm::StringRef MapStyle);

  void setArgs(llvm::ArrayRef<const char *> &Argv) { CommandLineArgs = Argv; }

  const llvm::ArrayRef<const char *> &Args() const { return CommandLineArgs; }

  // --Threads
  void enableThreads() { m_enableThreads = true; }

  void disableThreads() { m_enableThreads = false; }

  bool threadsEnabled() const { return m_enableThreads; }

  void setNumThreads(int numThreads) { m_NumThreads = numThreads; }

  int numThreads() const { return m_NumThreads; }

  // SymDef File.
  void setSymDef(bool enable = true) { m_bSymDef = enable; }

  bool symDef() const { return m_bSymDef; }

  void setAllowBSSMixing(bool enable = true) { m_bAllowBSSMixing = enable; }
  bool AllowBSSMixing() const { return m_bAllowBSSMixing; }

  void setAllowBSSConversion(bool enable = true) {
    m_bAllowBSSConversion = enable;
  }
  bool AllowBSSConversion() const { return m_bAllowBSSConversion; }

  void setSymDefFile(std::string symDefFile) {
    setSymDef();
    m_SymDefFile = symDefFile;
  }

  std::string symDefFile() const { return m_SymDefFile; }

  bool setSymDefFileStyle(llvm::StringRef symDefFileStyle) {
    m_SymDefFileStyle = symDefFileStyle;
    return (m_SymDefFileStyle.lower() == "provide" ||
            m_SymDefFileStyle.lower() == "default");
  }

  llvm::StringRef symDefFileStyle() const { return m_SymDefFileStyle; }

  bool fixCortexA53Erratum843419() const { return m_bFixCortexA53Errata843419; }

  void setFixCortexA53Errata843419(bool enableErrata = true) {
    m_bFixCortexA53Errata843419 = enableErrata;
  }

  void setBuildCRef() { m_bBuildCref = true; }

  bool buildCRef() const { return m_bBuildCref; }

  void setVerify(llvm::StringRef VerifyType);

  uint32_t verify() const { return m_DiagEngine->getPrinter()->verify(); }

  std::set<std::string> &verifyRelocList() { return m_RelocVerify; }

  void setCompact(bool compact = true) { m_Compact = compact; }

  bool isCompact() const { return m_Compact; }

  void setCompactDyn(bool value = true) { m_bCompactDyn = value; }

  bool isCompactDyn() const { return m_bCompactDyn; }

  // --------------------ROPI/RWPI Support -----------------------------
  bool hasRWPI() const { return m_bRWPI; }

  void setRWPI() { m_bRWPI = true; }

  bool hasROPI() const { return m_bROPI; }

  void setROPI() { m_bROPI = true; }

  // --------------------AArch64 execute-only Support ------------------

  bool hasExecuteOnlySegments() { return m_bExecuteOnly; }
  void setExecuteOnlySegments() { m_bExecuteOnly = true; }

  // -------------------Unresolved Symbol Policy -----------------------
  bool setUnresolvedSymbolPolicy(llvm::StringRef O) {
    ReportUndefPolicy = O;
    return (O == "ignore-all" || O == "report-all" ||
            O == "ignore-in-object-files" || O == "ignore-in-shared-libs");
  }

  bool setOrphanHandlingMode(llvm::StringRef O) {
    m_Orphan_Mode = llvm::StringSwitch<OrphanMode>(O)
                        .CaseLower("error", OrphanMode::Error)
                        .CaseLower("warn", OrphanMode::Warn)
                        .CaseLower("place", OrphanMode::Place)
                        .Default(OrphanMode::Invalid);

    if (m_Orphan_Mode == OrphanMode::Invalid)
      return false;
    return true;
  }

  llvm::StringRef reportUndefPolicy() const { return ReportUndefPolicy; }
  OrphanMode getOrphanMode() const { return m_Orphan_Mode; }

  // ------------------- ThinLTO Cache Support -------------------------
  bool isLTOCacheEnabled() const { return m_LTOOptions & LTOCacheEnabled; }
  llvm::StringRef getLTOCacheDirectory() const { return m_LTOCacheDirectory; }

  //--------------------Timing statistics--------------------------------
  // --print-stats
  bool printTimingStats(const char *timeRegion = nullptr) const;

  void setPrintTimingStats() { m_bPrintTimeStats = true; }

  bool allUserPluginStatsRequested() const {
    return m_bPrintAllUserPluginTimeStats;
  }

  // --emit-stats <file>
  std::string timingStatsFile() const { return m_TimingStatsFile; }

  void setTimingStatsFile(std::string statsFile) {
    m_TimingStatsFile = statsFile;
  }
  //--------------------Plugin Config--------------------------------
  void addPluginConfig(const std::string &Config) {
    m_PluginConfig.push_back(Config);
  }

  const std::vector<std::string> &getPluginConfig() const {
    return m_PluginConfig;
  }

  // ------------------Demangle Style----------------------------------
  bool setDemangleStyle(llvm::StringRef Option);

  bool shouldDemangle() const { return m_bDemangle; }

  // -----------------Arch specific checking --------------------------
  bool validateArchOptions() const { return m_ValidateArchOpts; }

  void setValidateArchOptions() { m_ValidateArchOpts = true; }

  void setABIstring(llvm::StringRef ABIStr) { m_ABIString = ABIStr; }

  llvm::StringRef abiString() const { return m_ABIString; }

  // ----------------- Disable Guard ------------------------
  void setDisableGuardForWeakUndefs() { m_DisableGuardForWeakUndefs = true; }

  bool getDisableGuardForWeakUndefs() const {
    return m_DisableGuardForWeakUndefs;
  }

  void setRISCVRelax(bool value = true) { m_bRiscvRelax = value; }

  bool getRISCVRelax() const { return m_bRiscvRelax; }

  void setRISCVGPRelax(bool Relax) { m_RiscvGPRelax = Relax; }

  bool getRISCVGPRelax() const { return m_RiscvGPRelax; }

  void setRISCVRelaxToC(bool value = true) { m_bRiscvRelaxToC = value; }

  bool getRISCVRelaxToC() const { return m_bRiscvRelaxToC; }

  bool warnCommon() const { return m_bWarnCommon; }

  void setWarnCommon() { m_bWarnCommon = true; }

  void setAllowIncompatibleSectionsMix(bool f) {
    m_AllowIncompatibleSectionsMix = f;
  }

  bool allowIncompatibleSectionsMix() const {
    return m_AllowIncompatibleSectionsMix;
  }

  void setShowProgressBar() { m_ProgressBar = true; }

  bool showProgressBar() const { return m_ProgressBar; }

  void setRecordInputfiles() { m_RecordInputFiles = true; }

  void setCompressTar() { m_CompressTar = true; }

  bool getCompressTar() const { return m_CompressTar; }

  void setHasMappingFile(bool hasMap) { m_HasMappingFile = hasMap; }

  bool hasMappingFile() const { return m_HasMappingFile; }

  void setMappingFileName(const std::string &file) { m_MappingFileName = file; }

  const std::string &getMappingFileName() const { return m_MappingFileName; }

  bool getRecordInputFiles() const { return m_RecordInputFiles; }

  bool getDumpMappings() const { return m_DumpMappings; }

  void setDumpMappings(bool dump) { m_DumpMappings = dump; }

  void setMappingDumpFile(const std::string dump) { m_MappingDumpFile = dump; }

  const std::string getMappingDumpFile() const { return m_MappingDumpFile; }

  const std::string getResponseDumpFile() const { return m_ResponseDumpFile; }

  void setResponseDumpFile(const std::string dump) {
    m_ResponseDumpFile = dump;
  }

  void setDumpResponse(bool dump) { m_DumpResponse = true; }

  bool getDumpResponse() const { return m_DumpResponse; }

  void setTarFile(const std::string filename) { m_TarFile = filename; }

  const std::string getTarFile() const { return m_TarFile; }

  void setDisplaySummary() { m_DisplaySummary = true; }

  bool displaySummary() { return m_DisplaySummary; }

  void setSymbolTracingRequested() { m_SymbolTracingRequested = true; }

  bool isSymbolTracingRequested() const { return m_SymbolTracingRequested; }

  void setSectionTracingRequested() { m_SectionTracingRequested = true; }

  bool isSectionTracingRequested() const { return m_SectionTracingRequested; }

  // --------------Dynamic Linker-------------------------
  bool hasDynamicLinker() const { return m_bDynamicLinker; }

  void setHasDynamicLinker(bool val) { m_bDynamicLinker = val; }

  // -------------Default Map Styles ------------------------
  std::string getDefaultMapStyle() const { return m_DefaultMapStyle; }

  void setDefaultMapStyle(std::string Style) { m_DefaultMapStyle = Style; }

  bool isDefaultMapStyleText() const;

  bool isDefaultMapStyleYAML() const;

  // --unique-output-sections
  bool shouldEmitUniqueOutputSections() const {
    return m_EmitUniqueOutputSections;
  }

  void setEmitUniqueOutputSections(bool Emit) {
    m_EmitUniqueOutputSections = Emit;
  }

  // --reproduce-on-fail support
  void setReproduceOnFail(bool v) { m_RecordInputFilesOnFail = v; }

  bool isReproduceOnFail() const { return m_RecordInputFilesOnFail; }

  // -- enable relaxation on hexagon ----
  void enableRelaxation() { m_bRelaxation = true; }

  bool isLinkerRelaxationEnabled() const { return m_bRelaxation; }

  void setFatalInternalErrors(bool pEnable) { m_FatalInternalErrors = pEnable; }

  // Returns true if internal errors should be considered fatal.
  bool isFatalInternalErrors() const { return m_FatalInternalErrors; }

  // ----------------- --trace-merge-strings options --------------------------
  enum MergeStrTraceType { NONE, ALL, ALLOC, SECTIONS };

  MergeStrTraceType getMergeStrTraceType() const { return m_MergeStrTraceType; }

  void addMergeStrTraceSection(const std::string Section) {
    m_MergeStrSectionsToTrace.push_back(llvm::Regex(Section));
  }

  bool shouldTraceMergeStrSection(const ELFSection *S) const;

  // --trace-linker-script
  bool shouldTraceLinkerScript() const;

  // The return value indicates m_MapStyle modification
  bool checkAndUpdateMapStyleForPrintMap();

  void enableGlobalStringMerge() { GlobalMergeNonAllocStrings = true; }

  bool shouldGlobalStringMerge() const { return GlobalMergeNonAllocStrings; }

  // --keep-labels
  void setKeepLabels() { m_bKeepLabels = true; }

  bool shouldKeepLabels() const { return m_bKeepLabels; }

  // --check-sections
  void setEnableCheckSectionOverlaps() { m_bEnableOverlapChecks = true; }

  // --no-check-sections
  void setDisableCheckSectionOverlaps() { m_bEnableOverlapChecks = false; }

  bool doCheckOverlaps() const { return m_bEnableOverlapChecks; }

  // --relax=<regex> support
  bool isLinkerRelaxationEnabled(llvm::StringRef Name) const;

  void addRelaxSection(llvm::StringRef Name);

  void setThinArchiveRuleMatchingCompatibility() {
    m_ThinArchiveRuleMatchingCompat = true;
  }

  bool isThinArchiveRuleMatchingCompatibilityEnabled() const {
    return m_ThinArchiveRuleMatchingCompat;
  }

  // --sort-common support
  void setSortCommon() {
    m_SortCommon = SortCommonSymbols::DescendingAlignment;
  }

  bool setSortCommon(llvm::StringRef value) {
    if (value.lower() == "ascending") {
      m_SortCommon = SortCommonSymbols::AscendingAlignment;
      return true;
    } else if (value.lower() == "descending") {
      m_SortCommon = SortCommonSymbols::DescendingAlignment;
      return true;
    }
    return false;
  }

  bool isSortCommonEnabled() const { return !!m_SortCommon; }

  bool isSortCommonSymbolsAscendingAlignment() const {
    return isSortCommonEnabled() &&
           m_SortCommon.value() == SortCommonSymbols::AscendingAlignment;
  }

  bool isSortCommonSymbolsDescendingAlignment() const {
    return isSortCommonEnabled() &&
           m_SortCommon.value() == SortCommonSymbols::DescendingAlignment;
  }

  // --sort-section support
  bool setSortSection(llvm::StringRef value) {
    if (value.lower() == "alignment") {
      m_SortSection = SortSection::Alignment;
      return true;
    } else if (value.lower() == "name") {
      m_SortSection = SortSection::Name;
      return true;
    }
    return false;
  }

  bool isSortSectionEnabled() const { return !!m_SortSection; }

  bool isSortSectionByName() const {
    return isSortSectionEnabled() && m_SortSection.value() == SortSection::Name;
  }

  bool isSortSectionByAlignment() const {
    return isSortSectionEnabled() &&
           m_SortSection.value() == SortSection::Alignment;
  }

  // --print-memory-usage support
  bool shouldPrintMemoryUsage() const { return m_bPrintMemoryUsage; }

  void setShowPrintMemoryUsage(bool showUsage) {
    m_bPrintMemoryUsage = showUsage;
  }

  void setLinkLaunchDirectory(std::string Dir) { LinkLaunchDirectory = Dir; }

  std::string getLinkLaunchDirectory() const { return LinkLaunchDirectory; }

  // -------------------------- Build ID support -------------
  void setDefaultBuildID() { m_BuildID = true; }

  void setBuildIDValue(llvm::StringRef Val) {
    m_BuildID = true;
    m_BuildIDValue = Val;
  }

  bool isBuildIDEnabled() const { return m_BuildID; }

  bool hasBuildIDValue() const { return !!m_BuildIDValue; }

  llvm::StringRef getBuildID() const { return m_BuildIDValue.value(); }

  // --patch-enable support
  void setPatchEnable() { m_PatchEnable = true; }
  bool isPatchEnable() const { return m_PatchEnable; }

  void setPatchBase(const std::string &Value) { m_PatchBase = Value; }
  const std::optional<std::string> &getPatchBase() const { return m_PatchBase; }

  void setIgnoreUnknownOptions() { IgnoreUnknownOptions = true; }

  bool shouldIgnoreUnknownOptions() const { return IgnoreUnknownOptions; }

  void setUnknownOptions(const std::vector<std::string> &opts) {
    UnknownOptions = opts;
  }

  const std::vector<std::string> &getUnknownOptions() const {
    return UnknownOptions;
  }

  void enableShowRMSectNameInDiag() { ShowRMSectNameInDiag = true; }

  bool shouldShowRMSectNameInDiag() const { return ShowRMSectNameInDiag; }

  // default plugins
  bool useDefaultPlugins() const { return UseDefaultPlugins; }

  void setNoDefaultPlugins() { UseDefaultPlugins = false; }

  // -X or --discard-locals
  bool isStripTemporaryOrLocalSymbols() const {
    return (m_StripSymbols == StripTemporaries ||
            m_StripSymbols == StripLocals);
  }

private:
  bool appendMapStyle(const std::string MapStyle);
  enum status { YES, NO, Unknown };
  std::string m_LinkerPath;
  std::string m_DefaultLDScript;
  std::string m_Dyld;
  std::string m_DtInit;
  std::string m_DtFini;
  std::optional<std::string> m_OutputFileName;
  status m_ExecStack = Unknown;   // execstack, noexecstack
  status m_NoUndefined = Unknown; // defs, --no-undefined
  status m_MulDefs = Unknown;     // muldefs, --allow-multiple-definition
  std::optional<uint64_t> m_CommPageSize; // common-page-size=value
  std::optional<uint64_t> m_MaxPageSize;  // max-page-size=value
  bool m_bCombReloc = true;               // combreloc, nocombreloc
  bool m_bGlobal = false;                 // z,global
  bool m_bInitFirst = false;              // initfirst
  bool m_bNoCopyReloc = false;            // nocopyreloc
  bool m_bRelro = false;                  // relro, norelro
  bool m_bNow = false;                    // lazy, now
  bool m_Bsymbolic = false;               // --Bsymbolic
  bool m_BsymbolicFunctions = false;      // --Bsymbolic-functions
  bool m_Bgroup = false;
  bool m_bPIE = false;
  bool m_bColor = true;             // --color[=true,false,auto]
  bool m_bCreateEhFrameHdr = false; // --eh-frame-hdr
  bool m_bCreateEhFrameHdrSet = false;
  bool m_bNMagic = false;            // -n, --nmagic
  bool m_bOMagic = false;            // -N, --omagic
  bool m_bStripDebug = false;        // -S, --strip-debug
  bool m_bExportDynamic = false;     //-E, --export-dynamic
  bool m_bWarnSharedTextrel = false; // --warn-shared-textrel
  bool m_bWarnCommon = false;        // --warn-common
  bool m_bBinaryInput = false;   // -b [input-format], --format=[input-format]
  bool m_bDefineCommon = false;  // -d, -dc, -dp
  bool m_bFatalWarnings = false; // --fatal-warnings
  bool m_bLTOOptRemarksFile = false;           // --opt-record-file
  bool m_bLTOOptRemarksDisplayHotness = false; // --display-hotness-remarks
  bool m_bNoStdlib = false;                    // -nostdlib
  bool m_bPrintMap = false;                    // --print-map
  WarnMismatchMode m_WarnMismatch =
      GeneralOptions::None;            // --no{-warn}-mismatch
  bool m_bGCSections = false;          // --gc-sections
  bool m_bPrintGCSections = false;     // --print-gc-sections
  bool m_bGenUnwindInfo = true;        // --ld-generated-unwind-info
  bool m_bForceDynamic = false;        // --force-dynamic
  bool m_bDynamicList = false;         // --dynamic-list flag
  bool m_bVersionScript = false;       // --version-script
  bool m_bHasDyld = false;             // user set dynamic linker ?
  bool m_NoInhibitExec = false;        //--noinhibit-exec
  bool m_NoGnuStack = false;           //--nognustack
  bool m_bNoTrampolines = false;       //--no-trampolines
  bool m_bMergeStrings = true;         //--merge-strings
  bool m_bEmitRelocs = false;          //--emit-relocs
  bool m_bEmitGNUCompatRelocs = false; // --emit-gnu-compat-relocs
  bool m_bCref = false;                // --cref
  bool m_bBuildCref = false;           // noflag, buildCRef
  bool m_bUseMovVeneer = false;        // --use-mov-veneer
  bool m_bNoDelete = false;            // -z nodelete
  bool m_bNewDTags = false;            //--enable(disable)-new-dtags
  bool m_bWarnOnce = false;            // --warn-once
  bool m_bForceBTI = false;            // -z force-bti
  bool m_bForcePACPLT = false;         // -z pac-plt
  uint32_t m_GPSize = 8;               // -G, --gpsize
  bool m_lto = false;
  bool m_LTOUseAs = false;                         // -flto-use-as
  StripSymbolMode m_StripSymbols = KeepAllSymbols; // Strip symbols ?
  bool m_bPageAlignSegments = true; // Does the linker need to align segments to
                                    // a page.
  bool m_hasShared = false;         // -shared
  unsigned int m_HashStyle = SystemV;        // HashStyle
  bool m_savetemps = false;                  // -save-temps
  std::optional<std::string> m_saveTempsDir; // -save-temps=
  bool m_rosegment = false; // merge read only with readonly/execute segments.
  std::vector<std::string>
      m_UnparsedLTOOptions;    // Unparsed -flto-options, to pass to plugin.
  uint32_t m_LTOOptions = 0;   // -flto-options
  bool m_verify = true;        // Linker verifies output file.
  bool m_colormap = false;     // Map file with color.
  bool m_enableThreads = true; // threads enabled ?
  uint32_t m_NumThreads = 1;   // thread count
  bool m_bSymDef = false;      // --symdef
  std::string m_SymDefFile;    // --symdef-file
  llvm::StringRef m_SymDefFileStyle;        // --symdef-style
  bool m_bAllowBSSMixing = false;           // --disable-bss-mixing
  bool m_bAllowBSSConversion = false;       // --disable-bss-conversion
  bool m_bFixCortexA53Errata843419 = false; // --fix-cortex-a53-843419
  bool m_Compact = false;                   // --compact
  bool m_bRWPI = false;                     // --frwpi
  bool m_bROPI = false;                     // --fropi
  bool m_bExecuteOnly = false;              // --execute-only
  bool m_bPrintTimeStats = false;           // --print-stats
  bool m_bPrintAllUserPluginTimeStats = false;
  bool m_bDemangle = true;                  // --demangle-style
  bool m_ValidateArchOpts = false;          // check -mabi with backend
  bool m_DisableGuardForWeakUndefs = false; // hexagon specific option to
                                            // disable guard functionality.
  bool m_bRiscvRelax = true;                // enable riscv relaxation
  bool m_RiscvGPRelax = true;               // GP relaxation
  bool m_bRiscvRelaxToC = true; // enable riscv relax to compressed code
  bool m_AllowIncompatibleSectionsMix = false; // Allow incompatibleSections;
  bool m_ProgressBar = false;                  // Show progressbar.
  bool m_RecordInputFiles = false;             // --reproduce
  bool m_RecordInputFilesOnFail = false;       // --reproduce-on-fail
  // FIXME: Change the name to m_CompressReproduceTar
  bool m_CompressTar = false;         // --reproduce-compressed
  bool m_DisplaySummary = false;      // display linker run summary
  bool m_HasMappingFile = false;      // --Mapping-file
  bool m_DumpMappings = false;        // --Dump-Mapping-file
  bool m_DumpResponse = false;        // --Dump-Response-file
  bool m_InsertTimingStats = false;   // -emit-timing-stats-in-output
  bool m_FatalInternalErrors = false; // --fatal-internal-errors

  RpathList m_RpathList;
  ScriptList m_ScriptList;
  UndefSymList m_UndefSymList;     // -u
  UndefSymList m_ExportDynSymList; // --export-dynamic-symbol
  DynList m_DynList;               // --dynamic-list files
  DynList m_VersionScripts;        // --version-script files
  DynList m_ExternList;            // --extern-list files
  std::string m_Filter;
  std::string m_MapFile; // Mapfile
  std::string m_TarFile; // --reproduce output tarfile name
  std::string m_TimingStatsFile;
  std::string m_MappingFileName;           // --Mapping-file
  std::string m_MappingDumpFile;           // --dump-mapping-file
  std::string m_ResponseDumpFile;          // --dump-response-file
  llvm::StringMap<int32_t> m_inputFileMap; // InputFile Map
  std::vector<llvm::Regex> m_SymbolTrace;
  std::vector<llvm::Regex> m_RelocTrace;
  std::vector<llvm::Regex> m_SectionTrace;
  std::vector<std::string> m_SymbolsToTrace;
  std::vector<std::string> m_SectionsToTrace;
  std::vector<std::string> m_RelocsToTrace;
  std::vector<llvm::Regex> m_MergeStrSectionsToTrace;
  MergeStrTraceType m_MergeStrTraceType = MergeStrTraceType::NONE;
  std::set<std::string> m_RelocVerify;
  std::set<std::string> m_ExcludeLTOFiles;
  std::set<std::string> m_IncludeLTOFiles;
  PreserveList m_PreserveCmdLine;
  std::vector<std::string> m_codegenOpts;
  std::vector<std::string> m_asmOpts;
  CrefTable m_crefTable;
  std::string GcCrefSym;
  std::string m_Emulation;
  std::string m_CopyFarCallsFromFile;
  std::string m_NoReuseOfTrampolinesFile;
  std::string m_SoName;
  ExcludeLIBS m_ExcludeLIBS;
  ErrorStyle m_ErrorStyle = gnu;
  ScriptOption m_ScriptOption = MatchLLVM;
  std::vector<std::string> m_LTOAsmFile;
  std::vector<std::string> m_LTOOutputFile;
  bool m_bCompactDyn = false;          // z,compactdyn
  std::optional<uint64_t> m_ImageBase; // --image-base=value
  std::string m_Entry;
  SymbolRenameMap m_SymbolRenames;
  AddressMap m_AddressMap;
  llvm::ArrayRef<const char *> CommandLineArgs;
  llvm::StringRef ReportUndefPolicy;
  OrphanMode m_Orphan_Mode = OrphanMode::Place;
  std::string m_LTOCacheDirectory = "";
  std::vector<std::string> m_PluginConfig;
  llvm::StringRef m_ABIString = "";
  llvm::StringRef TrampolineMapFile; // TrampolineMap
  bool m_SymbolTracingRequested = false;
  bool m_SectionTracingRequested = false;
  std::vector<llvm::StringRef> m_RequestedTimeRegions;
  DiagnosticEngine *m_DiagEngine = nullptr;
  bool m_bDynamicLinker = true;
  std::string m_DefaultMapStyle = "txt";
  bool m_EmitUniqueOutputSections = false; // --unique-output-sections
  bool m_bRelaxation = false;              // --relaxation
  llvm::SmallVector<std::string, 8> m_MapStyles;
  bool GlobalMergeNonAllocStrings = false; // --global-merge-non-alloc-strings
  bool m_bKeepLabels = false;              // --keep-labels (RISC-V)
  bool m_bEnableOverlapChecks = true; // --check-sections/--no-check-sections
  bool m_ThinArchiveRuleMatchingCompat = false;
  bool m_bPrintMemoryUsage = false;              // --print-memory-usage
  std::optional<SortCommonSymbols> m_SortCommon; // --sort-common
  std::optional<SortSection> m_SortSection;      // --sort-section
  std::vector<llvm::Regex> m_RelaxSections;
  bool m_BuildID = false;
  std::optional<llvm::StringRef> m_BuildIDValue;
  bool m_PatchEnable = false;
  std::optional<std::string> m_PatchBase;
  bool IgnoreUnknownOptions = false;
  std::vector<std::string> UnknownOptions;
  std::string LinkLaunchDirectory;
  bool ShowRMSectNameInDiag = false;
  bool UseDefaultPlugins = true;
};

} // namespace eld

#endif
