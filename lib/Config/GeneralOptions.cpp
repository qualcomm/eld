//===- GeneralOptions.cpp--------------------------------------------------===//
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
#include "eld/Config/GeneralOptions.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Diagnostics/DiagnosticPrinter.h"
#include "eld/Input/Input.h"
#include "eld/Input/ZOption.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Support/StringUtils.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Regex.h"

using namespace eld;
using namespace llvm;

//===----------------------------------------------------------------------===//
// GeneralOptions
//===----------------------------------------------------------------------===//
GeneralOptions::GeneralOptions(DiagnosticEngine *DE) : DiagEngine(DE) {}

GeneralOptions::~GeneralOptions() { ScriptList.clear(); }

void GeneralOptions::setOutputFileName(const std::string &PName) {
  OutputFileName = PName;
}

bool GeneralOptions::addZOption(const ZOption &POption) {
  switch (POption.kind()) {
  case ZOption::CombReloc:
    BCombReloc = true;
    break;
  case ZOption::NoCombReloc:
    BCombReloc = false;
    break;
  case ZOption::Defs:
    NoUndefined = YES;
    break;
  case ZOption::InitFirst:
    BInitFirst = true;
    break;
  case ZOption::MulDefs:
    MulDefs = YES;
    break;
  case ZOption::NoCopyReloc:
    BNoCopyReloc = true;
    break;
  case ZOption::NoRelro:
    BRelro = false;
    break;
  case ZOption::Relro:
    BRelro = true;
    break;
  case ZOption::Lazy:
    BNow = false;
    break;
  case ZOption::Now:
    BNow = true;
    break;
  case ZOption::CommPageSize:
    CommPageSize = POption.pageSize();
    break;
  case ZOption::MaxPageSize:
    MaxPageSize = POption.pageSize();
    break;
  case ZOption::NoDelete:
    BNoDelete = true;
    break;
  case ZOption::Text:
    break;
  case ZOption::ExecStack:
    ExecStack = YES;
    break;
  case ZOption::NoExecStack:
    ExecStack = NO;
    break;
  case ZOption::NoGnuStack:
    NoGnuStack = true;
    break;
  case ZOption::Global:
    BGlobal = true;
    break;
  case ZOption::CompactDyn:
    BCompactDyn = true;
    break;
  case ZOption::ForceBTI:
    BForceBTI = true;
    break;
  case ZOption::ForcePACPLT:
    BForcePACPLT = true;
    break;
  case ZOption::Unknown:
  default:
    return false;
    break;
  }
  return true;
}

const std::string &GeneralOptions::entry() const { return Entry; }

void GeneralOptions::setEntry(const std::string &PEntry) { Entry = PEntry; }

bool GeneralOptions::hasEntry() const { return !Entry.empty(); }

void GeneralOptions::setTrace(bool EnableTrace) {
  DiagEngine->getPrinter()->setTrace(DiagEngine->getPrinter()->TraceFiles);
}

void GeneralOptions::setVerbose(int8_t Verbose) {
  DiagEngine->getPrinter()->setVerbose(Verbose);
}

bool GeneralOptions::printTimingStats(const char *TimeRegion) const {
  if (!BPrintTimeStats)
    return false;
  if (RequestedTimeRegions.empty())
    return true;
  return ::std::any_of(RequestedTimeRegions.begin(), RequestedTimeRegions.end(),
                       [&](const llvm::StringRef &TimingRegion) {
                         return (TimingRegion.compare_insensitive(TimeRegion) ==
                                 0);
                       });
}
bool GeneralOptions::setRequestedTimingRegions(const char *TimingRegion) {
  StringRef TimeRegion(TimingRegion);
  if (TimeRegion.compare_insensitive("all-user-plugins") == 0) {
    BPrintAllUserPluginTimeStats = true;
    RequestedTimeRegions.emplace_back(TimeRegion);
    return true;
  }
  if (TimeRegion.starts_with_insensitive("Plugin")) {
    size_t pos = TimeRegion.find_last_of('=');
    if (pos == std::string::npos) {
      if (TimeRegion.compare_insensitive("Plugin") == 0) {
        RequestedTimeRegions.emplace_back(TimeRegion);
        return true;
      }
    } else {
      StringRef PluginName = TimeRegion.substr(pos + 1);
      RequestedTimeRegions.emplace_back(PluginName);
      return true;
    }
  }
  return false;
}

bool GeneralOptions::shouldTraceMergeStrSection(const ELFSection *S) const {
  switch (MergeStrTraceValue) {
  case NONE:
    return false;
  case ALL:
    return true;
  case ALLOC:
    return S->isAlloc();
  case SECTIONS:
    for (auto &Regex : MergeStrSectionsToTrace)
      if (Regex.match(S->name()))
        return true;
    return false;
  }
}

eld::Expected<void> GeneralOptions::setTrace(const char *PTraceType) {
  std::optional<uint32_t> TraceMe = DiagEngine->getPrinter()->trace();
  StringRef TraceType = PTraceType;
  if (TraceType.starts_with("reloc")) {
    TraceMe = DiagEngine->getPrinter()->TraceReloc;
    size_t Pos = TraceType.find_last_of('=');
    std::string Reloc = TraceType.substr(Pos + 1).str();
    RelocTrace.emplace_back(llvm::Regex(Reloc));
    RelocsToTrace.emplace_back(Reloc);
  } else if (TraceType.starts_with("symbol") &&
             TraceType != "symbol-versioning") {
    setSymbolTracingRequested();
    TraceMe = DiagEngine->getPrinter()->TraceSym;
    size_t Pos = TraceType.find_last_of('=');
    std::string Sym = TraceType.substr(Pos + 1).str();
    SymbolTrace.emplace_back(llvm::Regex(Sym));
    SymbolsToTrace.emplace_back(Sym);
  } else if (TraceType.starts_with("section")) {
    setSectionTracingRequested();
    TraceMe = DiagEngine->getPrinter()->TraceSection;
    size_t Pos = TraceType.find_last_of('=');
    std::string Sym = TraceType.substr(Pos + 1).str();
    SectionTrace.emplace_back(llvm::Regex(Sym));
    SectionsToTrace.emplace_back(Sym);
  } else if (TraceType.starts_with("merge-strings")) {
    size_t Pos = TraceType.find_last_of('=');
    std::string Arg = TraceType.substr(Pos + 1).str();
    GeneralOptions::MergeStrTraceType Type =
        llvm::StringSwitch<GeneralOptions::MergeStrTraceType>(Arg)
            .Case("all", GeneralOptions::MergeStrTraceType::ALL)
            .Case("allocatable_sections",
                  GeneralOptions::MergeStrTraceType::ALLOC)
            .Default(GeneralOptions::MergeStrTraceType::SECTIONS);
    MergeStrTraceValue = Type;
    if (Type == SECTIONS)
      addMergeStrTraceSection(Arg);
    TraceMe = DiagEngine->getPrinter()->TraceMergeStrings;
  } else {
    TraceMe =
        llvm::StringSwitch<std::optional<uint32_t>>(TraceType)
            .Case("all-symbols", DiagEngine->getPrinter()->TraceSymbols)
            .Case("assignments", DiagEngine->getPrinter()->TraceAssignments)
            .Case("command-line", DiagEngine->getPrinter()->TraceCommandLine)
            .Case("files", DiagEngine->getPrinter()->TraceFiles)
            .Case("garbage-collection", DiagEngine->getPrinter()->TraceGC)
            .Case("live-edges", DiagEngine->getPrinter()->TraceGCLive)
            .Case("lto", DiagEngine->getPrinter()->TraceLTO)
            .Case("merge-strings", DiagEngine->getPrinter()->TraceMergeStrings)
            .Case("plugin", DiagEngine->getPrinter()->TracePlugin)
            .Case("threads", DiagEngine->getPrinter()->TraceThreads)
            .Case("trampolines", DiagEngine->getPrinter()->TraceTrampolines)
            .Case("wrap-symbols", DiagEngine->getPrinter()->TraceWrap)
            .Case("dynamic-linking",
                  DiagEngine->getPrinter()->TraceDynamicLinking)
            .Case("linker-script", DiagEngine->getPrinter()->TraceLinkerScript)
            .Case("symdef", DiagEngine->getPrinter()->TraceSymDef)
            .Case("symbol-versioning",
                  DiagEngine->getPrinter()->TraceSymbolVersioning)
            .Default(std::nullopt);
  }
  // Warn if trace category is unknown.
  if (!TraceMe)
    return std::make_unique<plugin::DiagnosticEntry>(
        plugin::WarningDiagnosticEntry(Diag::warn_unknown_trace_option,
                                       {PTraceType}));
  DiagEngine->getPrinter()->setTrace(*TraceMe);
  return {};
}

void GeneralOptions::setVerify(llvm::StringRef VerifyType) {
  uint32_t Verify = DiagEngine->getPrinter()->verify();
  if (VerifyType.starts_with("reloc")) {
    Verify = DiagEngine->getPrinter()->VerifyReloc;
    auto RelocList = VerifyType.rsplit('=').second;
    while (RelocList.size()) {
      auto Relocs = RelocList.split(',');
      RelocVerify.insert(Relocs.first.str());
      RelocList = Relocs.second;
    }
    DiagEngine->getPrinter()->setVerify(Verify);
  } else {
    DiagEngine->raise(Diag::warn_unknown_verify_type) << VerifyType;
    // issue a warning of unknown and ignored verify type
  }
}

// Reads the preserve list from the file specified in the preserve-file
// option
void GeneralOptions::getSymbolsFromFile(StringRef Filename,
                                        std::vector<std::string> &Symbols) {
  ErrorOr<std::unique_ptr<MemoryBuffer>> FileBuf =
      MemoryBuffer::getFile(Filename);
  if (!FileBuf)
    report_fatal_error("Could not read preserve symbols from file\n");
  StringRef Buffer = FileBuf->get()->getBuffer();
  while (!Buffer.empty()) {
    std::pair<StringRef, StringRef> LinePlus = Buffer.split('\n');
    Symbols.push_back(LinePlus.first.rtrim().str());
    Buffer = LinePlus.second;
  }
}

// Set LTO Options based on command line
void GeneralOptions::setLTOOptions(llvm::StringRef OptionType) {
  // Save the original option string too, in case the plugin may read it.
  UnparsedLTOOptions.push_back(OptionType.str());
  if (OptionType.starts_with("preserve-sym")) {
    LTOOptions |= LTOPreserve;
    size_t Pos = OptionType.find_last_of('=');
    std::string SymList = OptionType.substr(Pos + 1).str();
    do {
      Pos = SymList.find_first_of(',');
      std::string Sym = SymList.substr(0, Pos);
      SymList = SymList.substr(Pos + 1);
      PreserveCmdLine.push_back(Sym);
    } while (Pos != std::string::npos);
  } else if (OptionType.starts_with("codegen")) {
    LTOOptions |= LTOCodeGen;
    size_t Pos = OptionType.find_first_of('=');
    CodegenOpts.push_back(OptionType.substr(Pos + 1).str());
  } else if (OptionType.starts_with("preserve-file")) {
    LTOOptions |= LTOPreserve;
    size_t Pos = OptionType.find_last_of('=');
    std::string PreserveFile = OptionType.substr(Pos + 1).str();
    getSymbolsFromFile(PreserveFile, PreserveCmdLine);
  } else if (OptionType.starts_with("asmopts")) {
    LTOOptions |= LTOAsmOpts;
    size_t Pos = OptionType.find_first_of('=');
    AsmOpts.push_back(OptionType.substr(Pos + 1).str());
  } else if (OptionType.starts_with("lto-asm-file")) {
    LTOOptions |= LTOAsmFileOpt;
    size_t Pos = OptionType.find_first_of('=');
    setLTOAsmFile(OptionType.substr(Pos + 1).str());
  } else if (OptionType.starts_with("lto-output-file")) {
    LTOOptions |= LTOOutputFileOpt;
    size_t Pos = OptionType.find_first_of('=');
    setLTOOutputFile(OptionType.substr(Pos + 1).str());
  } else if (OptionType.starts_with("cache")) {
    LTOOptions |= LTOCacheEnabled;
    size_t Pos = OptionType.find_first_of('=');
    if (Pos != llvm::StringRef::npos)
      LTOCacheDirectory = OptionType.substr(Pos + 1).str();
  } else {
    LTOOptions |=
        llvm::StringSwitch<uint32_t>(OptionType)
            .Case("verbose", LTOOptionType::LTOVerbose)
            .Case("preserveall", LTOOptionType::LTOPreserve)
            .Case("disable-linkorder", LTOOptionType::LTODisableLinkOrder)
            .Default(LTOOptionType::LTONone);
  }
}

void GeneralOptions::addLTOCodeGenOptions(std::string O) {
  CodegenOpts.push_back(O);
}

void GeneralOptions::setLTOOptions(uint32_t LtoOption) {
  LTOOptions |= LtoOption;
}

bool GeneralOptions::traceSymbol(std::string const &PSym) const {
  StringRef SymRef(PSym);
  // Try to improve performance a bit.
  if (!SymbolTrace.size())
    return false;
  if (DiagEngine->getPrinter()->traceSym()) {
    return llvm::any_of(SymbolsToTrace,
                        [&](const std::string &S) { return S == PSym; }) ||
           llvm::any_of(SymbolTrace, [&](const llvm::Regex &Regex) {
             return Regex.match(SymRef);
           });
  }
  return false;
}

bool GeneralOptions::traceSection(std::string const &PSec) const {
  StringRef SecRef(PSec);
  // Try to improve performance a bit.
  if (SectionTrace.empty())
    return false;
  if (DiagEngine->getPrinter()->traceSection()) {
    return llvm::any_of(SectionsToTrace,
                        [&](const std::string &S) { return S == PSec; }) ||
           llvm::any_of(SectionTrace, [&](const llvm::Regex &Regex) {
             return Regex.match(SecRef);
           });
  }
  return false;
}

bool GeneralOptions::traceSection(const Section *S) const {
  if (traceSection(S->name().str()))
    return true;
  const ELFSection *ELFSect = llvm::dyn_cast<const ELFSection>(S);
  std::optional<std::string> RMSectName;
  if (ELFSect)
    RMSectName = ELFSect->getRMSectName();
  if (RMSectName)
    return traceSection(RMSectName.value());
  return false;
}

bool GeneralOptions::traceReloc(std::string const &RelocName) const {
  StringRef RelocRef(RelocName);
  // Look for an exact match first
  return llvm::any_of(RelocsToTrace,
                      [&](const std::string &S) { return S == RelocName; }) ||
         llvm::any_of(RelocTrace, [&](const llvm::Regex &Regex) {
           return Regex.match(RelocRef);
         });
}

std::vector<llvm::StringRef> GeneralOptions::getLTOOptionsAsString() const {
  std::vector<llvm::StringRef> ReturnValue;
  if ((LTOOptions & LTOVerbose) == LTOVerbose)
    ReturnValue.push_back("verbose");
  if (((LTOOptions & LTOPreserve) == LTOPreserve) & PreserveCmdLine.empty())
    ReturnValue.push_back("preserveall");
  if ((LTOOptions & LTOCodeGen) == LTOCodeGen)
    ReturnValue.push_back("codegen");
  if ((LTOOptions & LTOAsmOpts) == LTOAsmOpts)
    ReturnValue.push_back("asmopts");
  if ((LTOOptions & LTODisableLinkOrder) == LTODisableLinkOrder)
    ReturnValue.push_back("Disable link order with linker scripts/LTO");
  // Extend this later or for other -flto-options
  return ReturnValue;
}

/// Returns true if LTO trace is required
bool GeneralOptions::traceLTO(void) const {
  return (trace() & DiagEngine->getPrinter()->TraceLTO) ||
         (LTOOptions & LTOVerbose);
}

/// Returns true if LTO has to preserve all bitcode symbols
bool GeneralOptions::preserveAllLTO(void) const {
  return (LTOOptions & LTOPreserve) && PreserveCmdLine.empty();
}

/// Returns true if a list of symbols to be preserved is supplied
bool GeneralOptions::preserveSymbolsLTO(void) const {
  return (LTOOptions & LTOPreserve) && !PreserveCmdLine.empty();
}

/// Returns true if code generator options are supplied
bool GeneralOptions::codegenOpts(void) const {
  return (LTOOptions & LTOCodeGen);
}

/// Returns true if assembler options are supplied
bool GeneralOptions::asmopts(void) const { return (LTOOptions & LTOAsmOpts); }

bool GeneralOptions::hasLTOAsmFile(void) const {
  return (LTOOptions & LTOAsmFileOpt);
}

llvm::iterator_range<GeneralOptions::StringVectorIterT>
GeneralOptions::ltoAsmFile(void) const {
  return llvm::make_range(LTOAsmFile.cbegin(), LTOAsmFile.cend());
}

void GeneralOptions::setLTOAsmFile(StringRef LtoAsmFile) {
  size_t Pos = StringRef::npos;
  size_t LastPos = 0;
  while ((Pos = LtoAsmFile.find(",", LastPos)) != StringRef::npos) {
    LTOAsmFile.push_back(LtoAsmFile.slice(LastPos, Pos).str());
    LastPos = Pos + 1;
  }
  LTOAsmFile.push_back(LtoAsmFile.slice(LastPos, LtoAsmFile.size()).str());
}

bool GeneralOptions::hasLTOOutputFile(void) const {
  return (LTOOptions & LTOOutputFileOpt);
}

llvm::iterator_range<GeneralOptions::StringVectorIterT>
GeneralOptions::ltoOutputFile(void) const {
  return llvm::make_range(LTOOutputFile.cbegin(), LTOOutputFile.cend());
}

void GeneralOptions::setLTOOutputFile(StringRef LtoOutputFile) {
  size_t Pos = StringRef::npos;
  size_t LastPos = 0;
  while ((Pos = LtoOutputFile.find(",", LastPos)) != StringRef::npos) {
    LTOOutputFile.push_back(LtoOutputFile.slice(LastPos, Pos).str());
    LastPos = Pos + 1;
  }
  LTOOutputFile.push_back(
      LtoOutputFile.slice(LastPos, LtoOutputFile.size()).str());
}

bool GeneralOptions::disableLTOLinkOrder() const {
  return (LTOOptions & LTODisableLinkOrder);
}

/// Returns true if an input is in exclude libs list
bool GeneralOptions::isInExcludeLIBS(StringRef ResolvedPath,
                                     StringRef NameSpecPath) const {
  if (ExcludeLIBS.empty())
    return false;

  // Specifying "--exclude-libs ALL" excludes symbols in all archive libraries
  // from automatic export.
  if (ExcludeLIBS.count("ALL"))
    return true;

  if (ExcludeLIBS.count(NameSpecPath.str()))
    return true;

  if (ExcludeLIBS.count(ResolvedPath.str()))
    return true;

  if (ExcludeLIBS.count(llvm::sys::path::filename(ResolvedPath).str()))
    return true;

  return false;
}

bool GeneralOptions::setErrorStyle(std::string errStyle) {
  if (errStyle == "gnu") {
    ErrorStyle = gnu;
    return true;
  }
  if (errStyle == "llvm") {
    ErrorStyle = llvm;
    return true;
  }
  return false;
}

bool GeneralOptions::setScriptOption(std::string scriptOption) {
  if (scriptOption == "match-gnu") {
    ScriptOption = MatchGNU;
    return true;
  }
  if (scriptOption == "match-llvm") {
    ScriptOption = MatchLLVM;
    return true;
  }
  return false;
}

GeneralOptions::ScriptOptionType GeneralOptions::getScriptOption() const {
  return ScriptOption;
}

GeneralOptions::ErrorStyleType GeneralOptions::getErrorStyle() const {
  return ErrorStyle;
}

void GeneralOptions::setStats(llvm::StringRef Stats) {
  if (Stats == "all")
    DiagEngine->getPrinter()->setStats(DiagEngine->getPrinter()->AllStats);
}

void GeneralOptions::setHashStyle(std::string HashStyleOption) {
  HashStyle = llvm::StringSwitch<int>(HashStyleOption)
                  .Case("gnu", GeneralOptions::GNU)
                  .Case("sysv", GeneralOptions::SystemV)
                  .Case("both", GeneralOptions::Both)
                  .Default(GeneralOptions::SystemV);
}

bool GeneralOptions::setDemangleStyle(llvm::StringRef Option) {
  if (Option == "none") {
    BDemangle = false;
    return true;
  }
  if (Option == "demangle") {
    BDemangle = true;
    return true;
  }
  return false;
}

void GeneralOptions::setNoInhibitExec(bool PEnable) {
  NoInhibitExec = PEnable;
  if (PEnable)
    DiagEngine->getPrinter()->setNoInhibitExec();
}

bool GeneralOptions::isDefaultMapStyleText() const {
  return (llvm::StringRef(DefaultMapStyle).compare_insensitive("txt") == 0) ||
         (llvm::StringRef(DefaultMapStyle).compare_insensitive("gnu") == 0) ||
         (llvm::StringRef(DefaultMapStyle).compare_insensitive("llvm") == 0);
}

bool GeneralOptions::isDefaultMapStyleYAML() const {
  return llvm::StringRef(DefaultMapStyle).compare_insensitive("yaml") == 0;
}

bool GeneralOptions::appendMapStyle(const std::string MapStyle) {
  std::vector<std::string> MapStyleSplit = eld::string::split(MapStyle, ',');
  for (std::string &Style : MapStyleSplit) {
    // Check for valid map styles
    llvm::StringRef StyleRef = llvm::StringRef(Style);
    if (!(StyleRef.equals_insensitive("llvm") ||
          StyleRef.equals_insensitive("gnu") ||
          StyleRef.equals_insensitive("yaml") ||
          StyleRef.equals_insensitive("compressed") ||
          StyleRef.equals_insensitive("txt"))) {
      return false;
    }
    if (std::find(MapStyles.begin(), MapStyles.end(), Style) ==
        MapStyles.end()) {
      MapStyles.push_back(Style);
    }
  }
  return true;
}

bool GeneralOptions::setMapStyle(llvm::StringRef MapStyle) {
  if (MapStyle.empty())
    return false;
  if (MapStyle.contains_insensitive("all")) {
    appendMapStyle("txt");
    appendMapStyle("yaml");
  } else {
    if (!appendMapStyle(MapStyle.lower()))
      return false;
  }
  return true;
}

bool GeneralOptions::shouldTraceLinkerScript() const {
  return DiagEngine->getPrinter()->traceLinkerScript();
}

bool GeneralOptions::checkAndUpdateMapStyleForPrintMap() {
  if (!BPrintMap)
    return false;
  if (MapStyles.size() == 1) {
    MapStyles.push_back("txt");
    return true;
  }
  return false;
}

bool GeneralOptions::isLinkerRelaxationEnabled(llvm::StringRef Name) const {
  if (!isLinkerRelaxationEnabled())
    return false;
  if (!RelaxSections.size())
    return true;
  llvm::StringRef RelaxSection(Name);
  return llvm::any_of(RelaxSections, [&](const llvm::Regex &Regex) {
    return Regex.match(RelaxSection);
  });
}

void GeneralOptions::addRelaxSection(llvm::StringRef Name) {
  RelaxSections.emplace_back(llvm::Regex(Name));
}

bool GeneralOptions::traceSymbol(const LDSymbol &Sym,
                                 const ResolveInfo &RI) const {
  if (traceSymbol(RI.getName().str()))
    return true;
  InputFile *IF = RI.resolvedOrigin();
  if (ObjectFile *OF = llvm::dyn_cast<ObjectFile>(IF)) {
    auto OptAuxSymName = OF->getAuxiliarySymbolName(Sym.getSymbolIndex());
    if (OptAuxSymName) {
      if (traceSymbol(OptAuxSymName.value()))
        return true;
    }
  }
  return false;
}

bool GeneralOptions::traceSymbol(const ResolveInfo &RI) const {
  if (traceSymbol(RI.getName().str()))
    return true;
  LDSymbol *OutSym = RI.outSymbol();
  // Out symbol can be false for ResolveInfo created for shared library symbols.
  // ResolveInfo created for shared library symbols only have outSymbol if the
  // symbol is referenced.
  /// resolvedOrigin can be nullptr for Relocation::symInfo() when relocation is
  /// not associated with a symbol.
  if (!OutSym || !RI.resolvedOrigin())
    return false;
  InputFile *IF = RI.resolvedOrigin();
  if (ObjectFile *OF = llvm::dyn_cast<ObjectFile>(IF)) {
    auto OptAuxSymName = OF->getAuxiliarySymbolName(OutSym->getSymbolIndex());
    if (OptAuxSymName) {
      if (traceSymbol(OptAuxSymName.value()))
        return true;
    }
  }
  return false;
}
