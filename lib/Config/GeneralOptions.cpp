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
GeneralOptions::GeneralOptions(DiagnosticEngine *diagEngine)
    : m_DiagEngine(diagEngine) {}

GeneralOptions::~GeneralOptions() { m_ScriptList.clear(); }

void GeneralOptions::setOutputFileName(const std::string &pName) {
  m_OutputFileName = pName;
}

bool GeneralOptions::addZOption(const ZOption &pOption) {
  switch (pOption.kind()) {
  case ZOption::CombReloc:
    m_bCombReloc = true;
    break;
  case ZOption::NoCombReloc:
    m_bCombReloc = false;
    break;
  case ZOption::Defs:
    m_NoUndefined = YES;
    break;
  case ZOption::InitFirst:
    m_bInitFirst = true;
    break;
  case ZOption::MulDefs:
    m_MulDefs = YES;
    break;
  case ZOption::NoCopyReloc:
    m_bNoCopyReloc = true;
    break;
  case ZOption::NoRelro:
    m_bRelro = false;
    break;
  case ZOption::Relro:
    m_bRelro = true;
    break;
  case ZOption::Lazy:
    m_bNow = false;
    break;
  case ZOption::Now:
    m_bNow = true;
    break;
  case ZOption::CommPageSize:
    m_CommPageSize = pOption.pageSize();
    break;
  case ZOption::MaxPageSize:
    m_MaxPageSize = pOption.pageSize();
    break;
  case ZOption::NoDelete:
    m_bNoDelete = true;
    break;
  case ZOption::Text:
    break;
  case ZOption::ExecStack:
    m_ExecStack = YES;
    break;
  case ZOption::NoExecStack:
    m_ExecStack = NO;
    break;
  case ZOption::NoGnuStack:
    m_NoGnuStack = true;
    break;
  case ZOption::Global:
    m_bGlobal = true;
    break;
  case ZOption::CompactDyn:
    m_bCompactDyn = true;
    break;
  case ZOption::ForceBTI:
    m_bForceBTI = true;
    break;
  case ZOption::ForcePACPLT:
    m_bForcePACPLT = true;
    break;
  case ZOption::Unknown:
  default:
    return false;
    break;
  }
  return true;
}

const std::string &GeneralOptions::entry() const { return m_Entry; }

void GeneralOptions::setEntry(const std::string &pEntry) { m_Entry = pEntry; }

bool GeneralOptions::hasEntry() const { return !m_Entry.empty(); }

void GeneralOptions::setTrace(bool enableTrace) {
  m_DiagEngine->getPrinter()->setTrace(m_DiagEngine->getPrinter()->TraceFiles);
}

void GeneralOptions::setVerbose(int8_t Verbose) {
  m_DiagEngine->getPrinter()->setVerbose(Verbose);
}

bool GeneralOptions::printTimingStats(const char *TimeRegion) const {
  if (!m_bPrintTimeStats)
    return false;
  if (m_RequestedTimeRegions.empty())
    return true;
  return ::std::any_of(
      m_RequestedTimeRegions.begin(), m_RequestedTimeRegions.end(),
      [&](const llvm::StringRef &TimingRegion) {
        return (TimingRegion.compare_insensitive(TimeRegion) == 0);
      });
}
bool GeneralOptions::setRequestedTimingRegions(const char *timingRegion) {
  StringRef TimeRegion(timingRegion);
  if (TimeRegion.compare_insensitive("all-user-plugins") == 0) {
    m_bPrintAllUserPluginTimeStats = true;
    m_RequestedTimeRegions.emplace_back(TimeRegion);
    return true;
  } else if (TimeRegion.starts_with_insensitive("Plugin")) {
    size_t pos = TimeRegion.find_last_of('=');
    if (pos == std::string::npos) {
      if (TimeRegion.compare_insensitive("Plugin") == 0) {
        m_RequestedTimeRegions.emplace_back(TimeRegion);
        return true;
      }
    } else {
      StringRef PluginName = TimeRegion.substr(pos + 1);
      m_RequestedTimeRegions.emplace_back(PluginName);
      return true;
    }
  }
  return false;
}

bool GeneralOptions::shouldTraceMergeStrSection(const ELFSection *S) const {
  switch (m_MergeStrTraceType) {
  case NONE:
    return false;
  case ALL:
    return true;
  case ALLOC:
    return S->isAlloc();
  case SECTIONS:
    for (auto &Regex : m_MergeStrSectionsToTrace)
      if (Regex.match(S->name()))
        return true;
    return false;
  }
}

eld::Expected<void> GeneralOptions::setTrace(const char *TraceType) {
  std::optional<uint32_t> traceMe = m_DiagEngine->getPrinter()->trace();
  StringRef traceType = TraceType;
  if (traceType.starts_with("reloc")) {
    traceMe = m_DiagEngine->getPrinter()->TraceReloc;
    size_t pos = traceType.find_last_of('=');
    std::string reloc = traceType.substr(pos + 1).str();
    m_RelocTrace.emplace_back(llvm::Regex(reloc));
    m_RelocsToTrace.emplace_back(reloc);
  } else if (traceType.starts_with("symbol")) {
    setSymbolTracingRequested();
    traceMe = m_DiagEngine->getPrinter()->TraceSym;
    size_t pos = traceType.find_last_of('=');
    std::string sym = traceType.substr(pos + 1).str();
    m_SymbolTrace.emplace_back(llvm::Regex(sym));
    m_SymbolsToTrace.emplace_back(sym);
  } else if (traceType.starts_with("section")) {
    setSectionTracingRequested();
    traceMe = m_DiagEngine->getPrinter()->TraceSection;
    size_t pos = traceType.find_last_of('=');
    std::string sym = traceType.substr(pos + 1).str();
    m_SectionTrace.emplace_back(llvm::Regex(sym));
    m_SectionsToTrace.emplace_back(sym);
  } else if (traceType.starts_with("merge-strings")) {
    size_t Pos = traceType.find_last_of('=');
    std::string Arg = traceType.substr(Pos + 1).str();
    GeneralOptions::MergeStrTraceType Type =
        llvm::StringSwitch<GeneralOptions::MergeStrTraceType>(Arg)
            .Case("all", GeneralOptions::MergeStrTraceType::ALL)
            .Case("allocatable_sections",
                  GeneralOptions::MergeStrTraceType::ALLOC)
            .Default(GeneralOptions::MergeStrTraceType::SECTIONS);
    m_MergeStrTraceType = Type;
    if (Type == SECTIONS)
      addMergeStrTraceSection(Arg);
    traceMe = m_DiagEngine->getPrinter()->TraceMergeStrings;
  } else {
    traceMe =
        llvm::StringSwitch<std::optional<uint32_t>>(traceType)
            .Case("all-symbols", m_DiagEngine->getPrinter()->TraceSymbols)
            .Case("assignments", m_DiagEngine->getPrinter()->TraceAssignments)
            .Case("command-line", m_DiagEngine->getPrinter()->TraceCommandLine)
            .Case("files", m_DiagEngine->getPrinter()->TraceFiles)
            .Case("garbage-collection", m_DiagEngine->getPrinter()->TraceGC)
            .Case("live-edges", m_DiagEngine->getPrinter()->TraceGCLive)
            .Case("lto", m_DiagEngine->getPrinter()->TraceLTO)
            .Case("merge-strings",
                  m_DiagEngine->getPrinter()->TraceMergeStrings)
            .Case("plugin", m_DiagEngine->getPrinter()->TracePlugin)
            .Case("threads", m_DiagEngine->getPrinter()->TraceThreads)
            .Case("trampolines", m_DiagEngine->getPrinter()->TraceTrampolines)
            .Case("wrap-symbols", m_DiagEngine->getPrinter()->TraceWrap)
            .Case("dynamic-linking",
                  m_DiagEngine->getPrinter()->TraceDynamicLinking)
            .Case("linker-script",
                  m_DiagEngine->getPrinter()->TraceLinkerScript)
            .Case("symdef", m_DiagEngine->getPrinter()->TraceSymDef)
            .Default(std::nullopt);
  }
  // Warn if trace category is unknown.
  if (!traceMe)
    return std::make_unique<plugin::DiagnosticEntry>(
        plugin::WarningDiagnosticEntry(diag::warn_unknown_trace_option,
                                       {TraceType}));
  m_DiagEngine->getPrinter()->setTrace(*traceMe);
  return {};
}

void GeneralOptions::setVerify(llvm::StringRef VerifyType) {
  uint32_t verify = m_DiagEngine->getPrinter()->verify();
  if (VerifyType.starts_with("reloc")) {
    verify = m_DiagEngine->getPrinter()->VerifyReloc;
    auto relocList = VerifyType.rsplit('=').second;
    while (relocList.size()) {
      auto relocs = relocList.split(',');
      m_RelocVerify.insert(relocs.first.str());
      relocList = relocs.second;
    }
    m_DiagEngine->getPrinter()->setVerify(verify);
  } else {
    m_DiagEngine->raise(diag::warn_unknown_verify_type) << VerifyType;
    // issue a warning of unknown and ignored verify type
  }
}

// Reads the preserve list from the file specified in the preserve-file
// option
void GeneralOptions::getSymbolsFromFile(StringRef filename,
                                        std::vector<std::string> &symbols) {
  ErrorOr<std::unique_ptr<MemoryBuffer>> fileBuf =
      MemoryBuffer::getFile(filename);
  if (!fileBuf)
    report_fatal_error("Could not read preserve symbols from file\n");
  StringRef buffer = fileBuf->get()->getBuffer();
  while (!buffer.empty()) {
    std::pair<StringRef, StringRef> linePlus = buffer.split('\n');
    symbols.push_back(linePlus.first.rtrim().str());
    buffer = linePlus.second;
  }
}

// Set LTO Options based on command line
void GeneralOptions::setLTOOptions(llvm::StringRef optionType) {
  // Save the original option string too, in case the plugin may read it.
  m_UnparsedLTOOptions.push_back(optionType.str());
  if (optionType.starts_with("preserve-sym")) {
    m_LTOOptions |= LTOPreserve;
    size_t pos = optionType.find_last_of('=');
    std::string symList = optionType.substr(pos + 1).str();
    do {
      pos = symList.find_first_of(',');
      std::string sym = symList.substr(0, pos);
      symList = symList.substr(pos + 1);
      m_PreserveCmdLine.push_back(sym);
    } while (pos != std::string::npos);
  } else if (optionType.starts_with("codegen")) {
    m_LTOOptions |= LTOCodeGen;
    size_t pos = optionType.find_first_of('=');
    m_codegenOpts.push_back(optionType.substr(pos + 1).str());
  } else if (optionType.starts_with("preserve-file")) {
    m_LTOOptions |= LTOPreserve;
    size_t pos = optionType.find_last_of('=');
    std::string preserveFile = optionType.substr(pos + 1).str();
    getSymbolsFromFile(preserveFile, m_PreserveCmdLine);
  } else if (optionType.starts_with("asmopts")) {
    m_LTOOptions |= LTOAsmOpts;
    size_t pos = optionType.find_first_of('=');
    m_asmOpts.push_back(optionType.substr(pos + 1).str());
  } else if (optionType.starts_with("lto-asm-file")) {
    m_LTOOptions |= LTOAsmFile;
    size_t pos = optionType.find_first_of('=');
    setLTOAsmFile(optionType.substr(pos + 1).str());
  } else if (optionType.starts_with("lto-output-file")) {
    m_LTOOptions |= LTOOutputFile;
    size_t pos = optionType.find_first_of('=');
    setLTOOutputFile(optionType.substr(pos + 1).str());
  } else if (optionType.starts_with("cache")) {
    m_LTOOptions |= LTOCacheEnabled;
    size_t pos = optionType.find_first_of('=');
    if (pos != llvm::StringRef::npos)
      m_LTOCacheDirectory = optionType.substr(pos + 1).str();
  } else {
    m_LTOOptions |= llvm::StringSwitch<uint32_t>(optionType)
                        .Case("verbose", LTOVerbose)
                        .Case("preserveall", LTOPreserve)
                        .Case("disable-linkorder", LTODisableLinkOrder)
                        .Default(LTONone);
  }
}

void GeneralOptions::addLTOCodeGenOptions(std::string O) {
  m_codegenOpts.push_back(O);
}

void GeneralOptions::setLTOOptions(uint32_t ltoOption) {
  m_LTOOptions |= ltoOption;
}

bool GeneralOptions::traceSymbol(std::string const &pSym) const {
  StringRef SymRef(pSym);
  // Try to improve performance a bit.
  if (!m_SymbolTrace.size())
    return false;
  if (m_DiagEngine->getPrinter()->traceSym()) {
    return llvm::any_of(m_SymbolsToTrace,
                        [&](const std::string &S) { return S == pSym; }) ||
           llvm::any_of(m_SymbolTrace, [&](const llvm::Regex &Regex) {
             return Regex.match(SymRef);
           });
  }
  return false;
}

bool GeneralOptions::traceSection(std::string const &pSec) const {
  StringRef SecRef(pSec);
  // Try to improve performance a bit.
  if (m_SectionTrace.empty())
    return false;
  if (m_DiagEngine->getPrinter()->traceSection()) {
    return llvm::any_of(m_SectionsToTrace,
                        [&](const std::string &S) { return S == pSec; }) ||
           llvm::any_of(m_SectionTrace, [&](const llvm::Regex &Regex) {
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
  return llvm::any_of(m_RelocsToTrace,
                      [&](const std::string &S) { return S == RelocName; }) ||
         llvm::any_of(m_RelocTrace, [&](const llvm::Regex &Regex) {
           return Regex.match(RelocRef);
         });
}

std::vector<llvm::StringRef> GeneralOptions::getLTOOptionsAsString() const {
  std::vector<llvm::StringRef> returnValue;
  if ((m_LTOOptions & LTOVerbose) == LTOVerbose)
    returnValue.push_back("verbose");
  if (((m_LTOOptions & LTOPreserve) == LTOPreserve) & m_PreserveCmdLine.empty())
    returnValue.push_back("preserveall");
  if ((m_LTOOptions & LTOCodeGen) == LTOCodeGen)
    returnValue.push_back("codegen");
  if ((m_LTOOptions & LTOAsmOpts) == LTOAsmOpts)
    returnValue.push_back("asmopts");
  if ((m_LTOOptions & LTODisableLinkOrder) == LTODisableLinkOrder)
    returnValue.push_back("Disable link order with linker scripts/LTO");
  // Extend this later or for other -flto-options
  return returnValue;
}

/// Returns true if LTO trace is required
bool GeneralOptions::traceLTO(void) const {
  return (trace() & m_DiagEngine->getPrinter()->TraceLTO) ||
         (m_LTOOptions & LTOVerbose);
}

/// Returns true if LTO has to preserve all bitcode symbols
bool GeneralOptions::preserveAllLTO(void) const {
  return (m_LTOOptions & LTOPreserve) && m_PreserveCmdLine.empty();
}

/// Returns true if a list of symbols to be preserved is supplied
bool GeneralOptions::preserveSymbolsLTO(void) const {
  return (m_LTOOptions & LTOPreserve) && !m_PreserveCmdLine.empty();
}

/// Returns true if code generator options are supplied
bool GeneralOptions::codegenOpts(void) const {
  return (m_LTOOptions & LTOCodeGen);
}

/// Returns true if assembler options are supplied
bool GeneralOptions::asmopts(void) const { return (m_LTOOptions & LTOAsmOpts); }

bool GeneralOptions::hasLTOAsmFile(void) const {
  return (m_LTOOptions & LTOAsmFile);
}

llvm::iterator_range<GeneralOptions::StringVectorIterT>
GeneralOptions::ltoAsmFile(void) const {
  return llvm::make_range(m_LTOAsmFile.cbegin(), m_LTOAsmFile.cend());
}

void GeneralOptions::setLTOAsmFile(StringRef ltoAsmFile) {
  size_t pos = StringRef::npos;
  size_t lastPos = 0;
  while ((pos = ltoAsmFile.find(",", lastPos)) != StringRef::npos) {
    m_LTOAsmFile.push_back(ltoAsmFile.slice(lastPos, pos).str());
    lastPos = pos + 1;
  }
  m_LTOAsmFile.push_back(ltoAsmFile.slice(lastPos, ltoAsmFile.size()).str());
}

bool GeneralOptions::hasLTOOutputFile(void) const {
  return (m_LTOOptions & LTOOutputFile);
}

llvm::iterator_range<GeneralOptions::StringVectorIterT>
GeneralOptions::ltoOutputFile(void) const {
  return llvm::make_range(m_LTOOutputFile.cbegin(), m_LTOOutputFile.cend());
}

void GeneralOptions::setLTOOutputFile(StringRef ltoOutputFile) {
  size_t pos = StringRef::npos;
  size_t lastPos = 0;
  while ((pos = ltoOutputFile.find(",", lastPos)) != StringRef::npos) {
    m_LTOOutputFile.push_back(ltoOutputFile.slice(lastPos, pos).str());
    lastPos = pos + 1;
  }
  m_LTOOutputFile.push_back(
      ltoOutputFile.slice(lastPos, ltoOutputFile.size()).str());
}

bool GeneralOptions::disableLTOLinkOrder() const {
  return (m_LTOOptions & LTODisableLinkOrder);
}

/// Returns true if an input is in exclude libs list
bool GeneralOptions::isInExcludeLIBS(StringRef ResolvedPath,
                                     StringRef NameSpecPath) const {
  if (m_ExcludeLIBS.empty())
    return false;

  // Specifying "--exclude-libs ALL" excludes symbols in all archive libraries
  // from automatic export.
  if (m_ExcludeLIBS.count("ALL"))
    return true;

  if (m_ExcludeLIBS.count(NameSpecPath.str()))
    return true;

  if (m_ExcludeLIBS.count(ResolvedPath.str()))
    return true;

  if (m_ExcludeLIBS.count(llvm::sys::path::filename(ResolvedPath).str()))
    return true;

  return false;
}

bool GeneralOptions::setErrorStyle(std::string errStyle) {
  if (errStyle == "gnu") {
    m_ErrorStyle = gnu;
    return true;
  } else if (errStyle == "llvm") {
    m_ErrorStyle = llvm;
    return true;
  }
  return false;
}

bool GeneralOptions::setScriptOption(std::string scriptOption) {
  if (scriptOption == "match-gnu") {
    m_ScriptOption = MatchGNU;
    return true;
  } else if (scriptOption == "match-llvm") {
    m_ScriptOption = MatchLLVM;
    return true;
  }
  return false;
}

GeneralOptions::ScriptOption GeneralOptions::getScriptOption() const {
  return m_ScriptOption;
}

GeneralOptions::ErrorStyle GeneralOptions::getErrorStyle() const {
  return m_ErrorStyle;
}

void GeneralOptions::setStats(llvm::StringRef stats) {
  if (stats == "all")
    m_DiagEngine->getPrinter()->setStats(m_DiagEngine->getPrinter()->AllStats);
}

void GeneralOptions::setHashStyle(std::string hashStyle) {
  m_HashStyle = llvm::StringSwitch<int>(hashStyle)
                    .Case("gnu", GeneralOptions::GNU)
                    .Case("sysv", GeneralOptions::SystemV)
                    .Case("both", GeneralOptions::Both)
                    .Default(GeneralOptions::SystemV);
}

bool GeneralOptions::setDemangleStyle(llvm::StringRef Option) {
  if (Option == "none") {
    m_bDemangle = false;
    return true;
  }
  if (Option == "demangle") {
    m_bDemangle = true;
    return true;
  }
  return false;
}

void GeneralOptions::setNoInhibitExec(bool pEnable) {
  m_NoInhibitExec = pEnable;
  if (pEnable)
    m_DiagEngine->getPrinter()->setNoInhibitExec();
}

bool GeneralOptions::isDefaultMapStyleText() const {
  return (llvm::StringRef(m_DefaultMapStyle).compare_insensitive("txt") == 0) ||
         (llvm::StringRef(m_DefaultMapStyle).compare_insensitive("gnu") == 0) ||
         (llvm::StringRef(m_DefaultMapStyle).compare_insensitive("llvm") == 0);
}

bool GeneralOptions::isDefaultMapStyleYAML() const {
  return llvm::StringRef(m_DefaultMapStyle).compare_insensitive("yaml") == 0;
}

bool GeneralOptions::appendMapStyle(const std::string MapStyle) {
  std::vector<std::string> MapStyleSplit = eld::string::split(MapStyle, ',');
  for (std::string &style : MapStyleSplit) {
    // Check for valid map styles
    llvm::StringRef styleRef = llvm::StringRef(style);
    if (!(styleRef.equals_insensitive("llvm") ||
          styleRef.equals_insensitive("gnu") ||
          styleRef.equals_insensitive("yaml") ||
          styleRef.equals_insensitive("compressed") ||
          styleRef.equals_insensitive("txt"))) {
      return false;
    }
    if (std::find(m_MapStyles.begin(), m_MapStyles.end(), style) ==
        m_MapStyles.end()) {
      m_MapStyles.push_back(style);
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
  return m_DiagEngine->getPrinter()->traceLinkerScript();
}

bool GeneralOptions::checkAndUpdateMapStyleForPrintMap() {
  if (!m_bPrintMap)
    return false;
  if (m_MapStyles.size() == 1) {
    m_MapStyles.push_back("txt");
    return true;
  }
  return false;
}

bool GeneralOptions::isLinkerRelaxationEnabled(llvm::StringRef Name) const {
  if (!isLinkerRelaxationEnabled())
    return false;
  if (!m_RelaxSections.size())
    return true;
  llvm::StringRef RelaxSection(Name);
  return llvm::any_of(m_RelaxSections, [&](const llvm::Regex &Regex) {
    return Regex.match(RelaxSection);
  });
}

void GeneralOptions::addRelaxSection(llvm::StringRef Name) {
  m_RelaxSections.emplace_back(llvm::Regex(Name));
}

bool GeneralOptions::traceSymbol(const LDSymbol &sym,
                                 const ResolveInfo &RI) const {
  if (traceSymbol(RI.getName().str()))
    return true;
  InputFile *IF = RI.resolvedOrigin();
  if (ObjectFile *OF = llvm::dyn_cast<ObjectFile>(IF)) {
    auto optAuxSymName = OF->getAuxiliarySymbolName(sym.getSymbolIndex());
    if (optAuxSymName) {
      if (traceSymbol(optAuxSymName.value()))
        return true;
    }
  }
  return false;
}

bool GeneralOptions::traceSymbol(const ResolveInfo &RI) const {
  if (traceSymbol(RI.getName().str()))
    return true;
  LDSymbol *outSym = RI.outSymbol();
  // Out symbol can be false for ResolveInfo created for shared library symbols.
  // ResolveInfo created for shared library symbols only have outSymbol if the
  // symbol is referenced.
  /// resolvedOrigin can be nullptr for Relocation::symInfo() when relocation is
  /// not associated with a symbol.
  if (!outSym || !RI.resolvedOrigin())
    return false;
  InputFile *IF = RI.resolvedOrigin();
  if (ObjectFile *OF = llvm::dyn_cast<ObjectFile>(IF)) {
    auto optAuxSymName = OF->getAuxiliarySymbolName(outSym->getSymbolIndex());
    if (optAuxSymName) {
      if (traceSymbol(optAuxSymName.value()))
        return true;
    }
  }
  return false;
}
