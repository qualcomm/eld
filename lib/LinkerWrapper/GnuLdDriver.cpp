//===- GnuLdDriver.cpp-----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Driver/GnuLdDriver.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Diagnostics/DiagnosticInfos.h"
#include "eld/Diagnostics/DiagnosticPrinter.h"
#include "eld/Input/InputAction.h"
#if defined(ELD_ENABLE_TARGET_ARM) || defined(ELD_ENABLE_TARGET_AARCH64)
#include "eld/Driver/ARMLinkDriver.h"
#endif
#ifdef ELD_ENABLE_TARGET_HEXAGON
#include "eld/Driver/HexagonLinkDriver.h"
#endif
#ifdef ELD_ENABLE_TARGET_RISCV
#include "eld/Driver/RISCVLinkDriver.h"
#endif
#ifdef ELD_ENABLE_TARGET_X86_64
#include "eld/Driver/x86_64LinkDriver.h"
#endif
#include "eld/Input/JustSymbolsAction.h"
#include "eld/Input/ZOption.h"
#include "eld/LayoutMap/TextLayoutPrinter.h"
#include "eld/LayoutMap/YamlLayoutPrinter.h"
#include "eld/Support/MappingFileReader.h"
#include "eld/Support/MsgHandling.h"
#include "eld/Support/OutputTarWriter.h"
#include "eld/Support/StringUtils.h"
#include "eld/Support/TargetRegistry.h"
#include "eld/Support/TargetSelect.h"
#include "eld/Target/TargetMachine.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include <fstream>
#include <thread>

using namespace llvm;
using namespace llvm::opt;
using namespace eld;

#define OPTTABLE_STR_TABLE_CODE
#include "eld/Driver/GnuLinkerOptions.inc"
#undef OPTTABLE_STR_TABLE_CODE

#define OPTTABLE_PREFIXES_TABLE_CODE
#include "eld/Driver/GnuLinkerOptions.inc"
#undef OPTTABLE_PREFIXES_TABLE_CODE

static constexpr llvm::opt::OptTable::Info infoTable[] = {
#define OPTION(PREFIXES_OFFSET, PREFIXED_NAME_OFFSET, ID, KIND, GROUP, ALIAS,  \
               ALIASARGS, FLAGS, VISIBILITY, PARAM, HELPTEXT,                  \
               HELPTEXTSFORVARIANTS, METAVAR, VALUES)                          \
  LLVM_CONSTRUCT_OPT_INFO(                                                     \
      PREFIXES_OFFSET, PREFIXED_NAME_OFFSET, GnuLdOptTable::ID, KIND,          \
      GnuLdOptTable::GROUP, GnuLdOptTable::ALIAS, ALIASARGS, FLAGS,            \
      VISIBILITY, PARAM, HELPTEXT, HELPTEXTSFORVARIANTS, METAVAR, VALUES),
#include "eld/Driver/GnuLinkerOptions.inc"
#undef OPTION
};

OPT_GnuLdOptTable::OPT_GnuLdOptTable()
    : GenericOptTable(OptionStrTable, OptionPrefixesTable, infoTable) {}

GnuLdDriver::GnuLdDriver(Flavor F)
    : m_DiagEngine(new eld::DiagnosticEngine(ShouldColorize())),
      m_Config(m_DiagEngine), m_Script(m_DiagEngine), m_Flavor(F) {
  std::unique_ptr<DiagnosticInfos> diagInfo =
      std::make_unique<DiagnosticInfos>(m_Config);
  m_DiagEngine->setInfoMap(std::move(diagInfo));
}

GnuLdDriver::~GnuLdDriver() { delete m_DiagEngine; }

GnuLdDriver *GnuLdDriver::Create(Flavor F, std::string Triple) {
  switch (F) {
#ifdef ELD_ENABLE_TARGET_HEXAGON
  case Flavor::Hexagon:
    return HexagonLinkDriver::Create(F, Triple);
#endif
#if defined(ELD_ENABLE_TARGET_ARM) || defined(ELD_ENABLE_TARGET_AARCH64)
  case Flavor::ARM:
  case Flavor::AArch64:
    return ARMLinkDriver::Create(F, Triple);
#endif
#ifdef ELD_ENABLE_TARGET_RISCV
  case Flavor::RISCV32:
  case Flavor::RISCV64:
    return RISCVLinkDriver::Create(F, Triple);
#endif
#ifdef ELD_ENABLE_TARGET_X86_64
  case Flavor::x86_64:
    return x86_64LinkDriver::Create(F, Triple);
#endif
  default:
    break;
  }
  return nullptr;
}

bool GnuLdDriver::emitStats(eld::Module &M) const {
  std::string File = m_Config.options().timingStatsFile();
  std::error_code error;
  llvm::raw_fd_ostream *StatsFile = nullptr;
  if (!File.empty()) {
    StatsFile = new llvm::raw_fd_ostream(File, error, llvm::sys::fs::OF_None);
    if (error) {
      m_Config.raise(Diag::fatal_unwritable_output) << File << error.message();
      return false;
    }
  }
  llvm::raw_fd_ostream *OutStream = &llvm::outs();
  if (StatsFile)
    OutStream = StatsFile;
  llvm::TimerGroup::printAll(*OutStream);
  llvm::TimerGroup::clearAll();
  M.getLinkerScript().printPluginTimers(*OutStream);
  delete StatsFile;
  return true;
}

bool GnuLdDriver::ShouldColorize() {
  const char *term = getenv("TERM");
  return term && (0 != strcmp(term, "dumb")) &&
         llvm::sys::Process::StandardOutIsDisplayed();
}

bool GnuLdDriver::checkAndRaiseTraceDiagEntry(eld::Expected<void> E) const {
  if (E)
    return true;
  m_Config.getDiagEngine()->raiseDiagEntry(std::move(E.error()));
  return false;
}

const char *GnuLdDriver::getLtoStatus() const { return "Enabled"; }

void GnuLdDriver::printVersionInfo() const {
  errs() << "Supported Targets: ";
  for (const auto &x : m_SupportedTargets)
    errs() << x << " ";
  errs() << "\n";
  if (!eld::getVendorName().empty()) {
    errs() << "Linker from " << eld::getVendorName() << " Version "
           << eld::getVendorVersion() << "\n";
  }
  errs() << "Linker based on LLVM version: " << eld::getELDVersion() << "\n";
  errs() << "Linker Plugin Support Enabled\n";
  errs() << "Linker Plugin Interface Version "
         << LINKER_PLUGIN_API_MAJOR_VERSION << "."
         << LINKER_PLUGIN_API_MINOR_VERSION << "\n";
  errs() << "LTO Support " << getLtoStatus() << "\n";
}
// Some command line options or some combinations of them are not allowed.
// This function checks for such errors.
template <class T>
bool GnuLdDriver::checkOptions(llvm::opt::InputArgList &Args) const {
  // check --thread-count and if threads are disabled.
  if (Args.getLastArg(T::thread_count)) {
    if (!m_Config.options().threadsEnabled()) {
      m_Config.raise(Diag::thread_count_with_no_threads);
      return false;
    }
  }
  return true;
}

int GnuLdDriver::getInteger(llvm::opt::InputArgList &Args, unsigned Key,
                            int Default) const {
  int V = Default;
  if (auto *Arg = Args.getLastArg(Key)) {
    if (Arg->getNumValues()) {
      StringRef S = Arg->getValue();
      S.getAsInteger(10, V);
    }
  }
  return V;
}

uint32_t GnuLdDriver::getUnsignedInteger(llvm::opt::Arg *arg,
                                         uint32_t Default) const {
  uint32_t V = Default;
  if (!arg || !arg->getNumValues())
    return V;
  StringRef S = arg->getValue();
  // getAsInteger returns true to signify the error
  // The string is considered erroneous if empty or if it overflows the type
  // of V.
  if (S.getAsInteger(10, V)) {
    m_Config.raise(Diag::invalid_value_for_option)
        << arg->getOption().getPrefixedName() << S;
    V = Default;
  }
  return V;
}

template <class T>
bool GnuLdDriver::processOptions(llvm::opt::InputArgList &Args) {
  // --color=mode
  bool res = ShouldColorize();

  if (llvm::opt::Arg *arg = Args.getLastArg(T::color)) {
    res = llvm::StringSwitch<bool>(arg->getValue())
              .Case("never", false)
              .Case("always", true)
              .Case("auto", res)
              .Default(false);
  }
  m_Config.options().setColor(res);
  m_Config.getPrinter()->setUseColor(res);

  // --error-limit
  if (llvm::opt::Arg *arg = Args.getLastArg(T::error_limit))
    m_Config.getPrinter()->setUserErrorLimit(getUnsignedInteger(arg, 10));

  // --warn-limit
  if (llvm::opt::Arg *arg = Args.getLastArg(T::warn_limit))
    m_Config.getPrinter()->setUserWarningLimit(getUnsignedInteger(arg, 10));

  // -t
  if (Args.hasArg(T::dash_t))
    m_Config.options().setTrace(true);

  // --trace
  for (auto *arg : Args.filtered(T::trace))
    checkAndRaiseTraceDiagEntry(m_Config.options().setTrace(arg->getValue()));

  // --trace-symbol, -y
  for (llvm::opt::Arg *arg : Args.filtered(T::trace_symbol)) {
    std::string symbolName = std::string(arg->getValue());
    std::string trace = "symbol=" + symbolName;
    checkAndRaiseTraceDiagEntry(m_Config.options().setTrace(trace.c_str()));
  }

  // --trace-reloc
  for (llvm::opt::Arg *arg : Args.filtered(T::trace_reloc)) {
    std::string symbolName = std::string(arg->getValue());
    std::string trace = "reloc=" + symbolName;
    checkAndRaiseTraceDiagEntry(m_Config.options().setTrace(trace.c_str()));
  }

  // --trace-lto
  if (Args.hasArg(T::trace_lto)) {
    checkAndRaiseTraceDiagEntry(m_Config.options().setTrace("lto"));
    m_Config.addCommandLine(Table->getOptionName(T::trace_lto), true);
  }

  // --trace-merge-strings
  for (llvm::opt::Arg *Arg : Args.filtered(T::trace_merge_strings)) {
    std::string Args = Arg->getValue();
    std::string Trace = "merge-strings=" + Args;
    checkAndRaiseTraceDiagEntry(m_Config.options().setTrace(Trace.c_str()));
  }

  // --trace-section
  for (llvm::opt::Arg *arg : Args.filtered(T::trace_section)) {
    auto sectionName = std::string(arg->getValue());
    auto trace = "section=" + sectionName;
    checkAndRaiseTraceDiagEntry(m_Config.options().setTrace(trace.c_str()));
  }

  // --relocation-options
  for (auto *arg : Args.filtered(T::verify_options))
    m_Config.options().setVerify(arg->getValue());

  // -soname
  if (llvm::opt::Arg *arg = Args.getLastArg(T::soname)) {
    m_Config.options().setSOName(arg->getValue());
    m_Config.addCommandLine(Table->getOptionName(T::soname), arg->getValue());
  }

  // -rpath
  for (auto *arg : Args.filtered(T::rpath))
    m_Config.options().getRpathList().push_back(arg->getValue());

  // --script, -T
  if (Args.hasArg(T::T)) {
    // Dont align segments if a linker script is passed.
    m_Config.options().setAlignSegments(false);
    m_Config.addCommandLine(Table->getOptionName(T::T), true);
  }

  // --just-symbols, -R
  if (Args.hasArg(T::R))
    m_Config.addCommandLine(Table->getOptionName(T::R), true);

  for (auto *arg : Args.filtered(T::u))
    m_Config.options().getUndefSymList().emplace_back(
        eld::make<StrToken>(arg->getValue()));

  // --sysroot
  if (llvm::opt::Arg *arg = Args.getLastArg(T::sysroot))
    m_Config.setSysRoot(arg->getValue());

  // --fatal-warnings
  m_Config.options().setFatalWarnings(Args.hasArg(T::fatal_warnings));

  // --no-fatal-warnings
  if (Args.hasArg(T::no_fatal_warnings))
    m_Config.options().setFatalWarnings(false);

  // --opt-record-file
  if (Args.hasArg(T::opt_record_file)) {
    m_Config.options().setLTOOptRemarksFile(true);
    m_Config.addCommandLine(Table->getOptionName(T::opt_record_file), true);
  }

  // --display-hotness
  std::vector<std::string> remarks;
  for (auto *arg : Args.filtered(T::display_hotness)) {
    m_Config.options().setLTOOptRemarksDisplayHotness(arg->getValue());
    remarks.push_back(arg->getValue());
  }
  m_Config.addCommandLine(Table->getOptionName(T::display_hotness), remarks);

  // add all search directories
  std::vector<std::string> searchDirs;
  for (auto *Ldir : Args.filtered(T::L)) {
    if (!m_Config.directories().insert(Ldir->getValue()))
      m_Config.raise(Diag::cannot_open_search_dir) << Ldir->getValue();
    searchDirs.push_back(Ldir->getValue());
  }
  m_Config.addCommandLine(Table->getOptionName(T::L), searchDirs);

  // Add current directory to search path.
  llvm::SmallString<128> curPath;
  llvm::sys::fs::current_path(curPath);
  m_Config.directories().insert(std::string(curPath));

  // -pie
  m_Config.options().setPIE(Args.hasFlag(T::pie, T::no_pie, false));

  // --verbose
  if (Args.hasArg(T::verbose))
    m_Config.options().setVerbose();

  // --verbose=0,1,2,...
  if (llvm::opt::Arg *arg = Args.getLastArg(T::verbose_level)) {
    llvm::StringRef value = arg->getValue();
    uint32_t verboseLevel = 0;
    if (value.getAsInteger(0, verboseLevel)) {
      m_Config.raise(Diag::invalid_value_for_option)
          << arg->getOption().getPrefixedName() << arg->getValue();
      return false;
    }
    // Just to be GNU compatible.
    if (verboseLevel > 2) {
      m_Config.raise(Diag::invalid_value_for_option)
          << arg->getOption().getPrefixedName() << arg->getValue();
      return false;
    }
    m_Config.options().setVerbose(verboseLevel);
  }

  // --emit-stats
  if (llvm::opt::Arg *arg = Args.getLastArg(T::emit_timing_stats)) {
    m_Config.options().setPrintTimingStats();
    m_Config.options().setTimingStatsFile(arg->getValue());
  }

  // --time-region
  if (llvm::opt::Arg *arg = Args.getLastArg(T::time_region)) {
    m_Config.options().setPrintTimingStats();
    if (!m_Config.options().setRequestedTimingRegions(arg->getValue())) {
      m_Config.raise(Diag::invalid_value_for_option)
          << arg->getOption().getPrefixedName() << arg->getValue();
      return false;
    }
  }

  if (Args.hasArg(T::print_timing_stats))
    m_Config.options().setPrintTimingStats();

  // -Bsymbolic
  m_Config.options().setBsymbolic(Args.hasArg(T::Bsymbolic));

  // -Bsymbolic-functions
  if (Args.hasArg(T::Bsymbolic_functions))
    m_Config.options().setBsymbolicFunctions(true);

  // -Bgroup
  m_Config.options().setBgroup(Args.hasArg(T::Bgroup));

  // --dynamic-linker
  if (llvm::opt::Arg *arg = Args.getLastArg(T::dynamic_linker))
    m_Config.options().setDyld(arg->getValue());

  // -init
  if (llvm::opt::Arg *arg = Args.getLastArg(T::init))
    m_Config.options().setDtInit(arg->getValue());

  // -fini
  if (llvm::opt::Arg *arg = Args.getLastArg(T::fini))
    m_Config.options().setDtFini(arg->getValue());

  // --no-undefined.
  m_Config.options().setNoUndefined(Args.hasArg(T::no_undefined));

  // --allow-multiple-definition
  m_Config.options().setMulDefs(Args.hasArg(T::allow_multiple_definition));

  // --warn-once
  m_Config.options().setWarnOnce(Args.hasArg(T::warn_once));

  // --noinhibit-exec
  m_Config.options().setNoInhibitExec(Args.hasArg(T::noinhibit_exec));

  // --eh-frame-hdr
  if (Args.hasArg(T::eh_frame_hdr))
    m_Config.options().setEhFrameHdr(true);

  // -s, --strip-debug
  bool hasStripDebug = Args.hasArg(T::strip_debug) || Args.hasArg(T::strip_all);
  m_Config.options().setStripDebug(hasStripDebug);
  m_Config.addCommandLine(Table->getOptionName(T::strip_debug), hasStripDebug);

  // --discard-all
  if (Args.hasArg(T::discard_all))
    m_Config.options().setStripSymbols(eld::GeneralOptions::StripLocals);
  else {
    // --strip-all
    if (Args.hasArg(T::strip_all)) {
      m_Config.options().setStripSymbols(eld::GeneralOptions::StripAllSymbols);
      m_Config.addCommandLine(Table->getOptionName(T::strip_all), true);
    } else {
      // --discard-locals
      if (Args.hasArg(T::discard_locals))
        m_Config.options().setStripSymbols(
            eld::GeneralOptions::StripTemporaries);
    }
  }

  // --export-dynamic, -E
  m_Config.options().setExportDynamic(Args.hasArg(T::export_dynamic));

  // --export-dynamic-symbol
  for (auto *Arg : Args.filtered(T::export_dynamic_symbol))
    m_Config.options().getExportDynSymList().emplace_back(
        eld::make<StrToken>(Arg->getValue()));

  // -d, -dc
  m_Config.options().setDefineCommon(Args.hasArg(T::d));

  // -nostdlib
  m_Config.options().setNoStdlib(Args.hasArg(T::nostdlib));

  // -M
  if (Args.hasArg(T::MapText))
    m_Config.options().setPrintMap(true);

  // --hash-style
  if (llvm::opt::Arg *arg = Args.getLastArg(T::hash_style)) {
    m_Config.options().setHashStyle(arg->getValue());
    m_Config.addCommandLine(Table->getOptionName(T::hash_style),
                            arg->getValue());
  }

  // -Map
  if (llvm::opt::Arg *arg = Args.getLastArg(T::Map)) {
    m_Config.options().setMapFile(arg->getValue());
    m_Config.addCommandLine(Table->getOptionName(T::Map), arg->getValue());
  }

  // -TrampolineMap
  if (llvm::opt::Arg *arg = Args.getLastArg(T::TrampolineMap)) {
    m_Config.options().setTrampolineMapFile(arg->getValue());
    m_Config.addCommandLine(Table->getOptionName(T::TrampolineMap),
                            arg->getValue());
  }

  // -flto-use-as
  if (Args.hasArg(T::flto_use_as)) {
    m_Config.options().setLTOUseAs();
    m_Config.addCommandLine(Table->getOptionName(T::flto_use_as), true);
  }

  // -color-map
  m_Config.options().setMapFileWithColor(Args.hasArg(T::color_map));

  // If -M option is used, lets try to use color.
  m_Config.options().setMapFileWithColor(Args.hasArg(T::PrintMap));

  // -MapDetail
  for (llvm::opt::Arg *arg : Args.filtered(T::MapDetail)) {
    eld::Expected<void> E = eld::LayoutPrinter::setLayoutDetail(
        arg->getValue(), m_Config.getDiagEngine());
    if (!E) {
      m_Config.raiseDiagEntry(std::move(E.error()));
      return false;
    }
  }

  // -MapStyle
  for (auto *style : Args.filtered(T::MapStyle)) {
    // Record commandline in binary map
    m_Config.addCommandLine(Table->getOptionName(T::MapStyle),
                            style->getValue());
    if (!m_Config.options().setMapStyle(style->getValue())) {
      m_Config.raise(Diag::invalid_option_mapstyle);
      return false;
    }
  }

  // --cref
  if (Args.getLastArg(T::cref))
    m_Config.options().setCref();

  // --gc-cref
  if (llvm::opt::Arg *arg = Args.getLastArg(T::gc_cref)) {
    m_Config.options().setGCCref(arg->getValue());
    m_Config.addCommandLine(Table->getOptionName(T::gc_cref), arg->getValue());
  }

  // --rosegment
  if (!m_Config.options().rosegment())
    m_Config.options().setROSegment(Args.hasArg(T::rosegment));

  // -emit-timing-stats-in-output
  if (Args.hasArg(T::emit_timing_stats_in_output))
    m_Config.options().setInsertTimingStats(true);

  // --error-style=[gnu|llvm]
  if (llvm::opt::Arg *arg = Args.getLastArg(T::error_style)) {
    if (!m_Config.options().setErrorStyle(arg->getValue())) {
      m_Config.raise(Diag::invalid_option_error_style);
      return false;
    }
  }

  // --script-options=[match-gnu|match-llvm]
  if (llvm::opt::Arg *arg = Args.getLastArg(T::script_options)) {
    if (!m_Config.options().setScriptOption(arg->getValue())) {
      m_Config.raise(Diag::invalid_option_match_error_style);
      return false;
    }
  }

  // --warn-shared-textrel
  if (Args.hasArg(T::warn_shared_textrel))
    m_Config.options().setWarnSharedTextrel(true);

  // --warn-common
  if (Args.hasArg(T::warn_common))
    m_Config.options().setWarnCommon();

  // --no-warn-shared_textrel
  if (Args.hasArg(T::no_warn_shared_textrel))
    m_Config.options().setWarnSharedTextrel(false);

  // --enable-newdtags, --disable-newdtags
  if (Args.hasArg(T::enable_newdtags) && Args.hasArg(T::disable_newdtags)) {
    errs() << "Cannot specify enable and disable  DTAGS at same time!\n";
    return false;
  }

  // --enabld-new-dtags
  if (Args.hasArg(T::enable_newdtags))
    m_Config.options().setNewDTags(true);

  // --enabld-disable-new-dtags
  if (Args.hasArg(T::disable_newdtags))
    m_Config.options().setNewDTags(false);

  // --emit-relocs
  if (Args.hasArg(T::emit_relocs)) {
    m_Config.options().setEmitGNUCompatRelocs(true);
    m_Config.options().setEmitRelocs(true);
    m_Config.addCommandLine(Table->getOptionName(T::emit_relocs), true);
  }

  // --emit-relocs-llvm
  if (Args.hasArg(T::emit_relocs_llvm))
    m_Config.options().setEmitRelocs(true);

  // --no-emit-relocs
  if (Args.hasArg(T::no_emit_relocs)) {
    m_Config.options().setEmitGNUCompatRelocs(false);
    m_Config.options().setEmitRelocs(false);
  }

  // --no-merge-strings
  m_Config.options().setMergeStrings(!Args.hasArg(T::no_merge_strings));
  m_Config.addCommandLine(Table->getOptionName(T::no_merge_strings),
                          Args.hasArg(T::no_merge_strings));

  // --{no-}warn-mismatch
  if (Args.getLastArg(T::no_warn_mismatch))
    m_Config.options().setWarnMismatch(false);

  if (Args.getLastArg(T::warn_mismatch))
    m_Config.options().setWarnMismatch(true);

  // --no-trampolines
  if (Args.hasArg(T::no_trampolines)) {
    m_Config.options().setNoTrampolines();
    m_Config.addCommandLine(Table->getOptionName(T::no_trampolines), true);
  }

  // --copy-farcalls-from-file
  if (llvm::opt::Arg *arg = Args.getLastArg(T::copy_farcalls_from_file))
    m_Config.options().setCopyFarCallsFromFile(arg->getValue());

  // --noreuse-trampolines-from-file
  if (llvm::opt::Arg *arg = Args.getLastArg(T::no_reuse_trampolines_file))
    m_Config.options().setNoReuseOfTrampolinesFile(arg->getValue());

  // --force-dynamic
  if (Args.hasArg(T::force_dynamic))
    m_Config.options().setForceDynamic();

  // -flto
  bool opt_flto = Args.hasArg(T::flto);
  m_Config.options().setLTO(opt_flto);
  m_Config.addCommandLine(Table->getOptionName(T::flto), opt_flto);

  // --save-temps
  m_Config.options().setSaveTemps(Args.hasArg(T::save_temps));

  if (const Arg *arg = Args.getLastArg(T::save_temps_EQ)) {
    m_Config.options().setSaveTempsDir(arg->getValue());
    m_Config.options().setSaveTemps(true);
  }

  // --flto-options
  std::vector<std::string> ltoOptions;
  for (auto *arg : Args.filtered(T::flto_options)) {
    m_Config.options().setLTOOptions(arg->getValue());
    ltoOptions.push_back(arg->getValue());
  }
  m_Config.addCommandLine(Table->getOptionName(T::flto_options), ltoOptions);

  // --no-align-segments
  if (Args.hasArg(T::no_align_segments))
    m_Config.options().setAlignSegments(false);

  bool enableFatalInternalErrors = Args.hasFlag(
      T::fatal_internal_errors, T::no_fatal_internal_errors, /*default=*/false);
  m_Config.options().setFatalInternalErrors(enableFatalInternalErrors);

  // set up entry point from -e
  if (llvm::opt::Arg *arg = Args.getLastArg(T::entrypoint)) {
    m_Config.options().setEntry(arg->getValue());
    m_Config.addCommandLine(Table->getOptionName(T::entrypoint),
                            arg->getValue());
  }

  // --wrap
  std::vector<std::string> wrapString;
  for (auto *arg : Args.filtered(T::wrap)) {
    std::string wname = arg->getValue();
    wrapString.push_back(wname);
    std::string to_wrap_str = eld::Saver.save("__wrap_" + wname).str();
    m_Config.options().renameMap().insert(std::make_pair(wname, to_wrap_str));

    // add __real_wname -> wname
    std::string from_real_str = eld::Saver.save("__real_" + wname).str();
    m_Config.options().renameMap().insert(std::make_pair(from_real_str, wname));
  } // end of for
  if (Args.hasArg(T::wrap))
    m_Config.addCommandLine(Table->getOptionName(T::wrap), wrapString);

  // -z option
  for (auto *arg : Args.filtered(T::dash_z)) {
    StringRef zOpt = arg->getValue();
    uint64_t zVal = 0;
    eld::ZOption::ZOptionKind zkind = eld::ZOption::Unknown;
    if (0 == zOpt.compare("combreloc"))
      zkind = eld::ZOption::CombReloc;
    else if (0 == zOpt.compare("nocombreloc"))
      zkind = eld::ZOption::NoCombReloc;
    else if (0 == zOpt.compare("global"))
      zkind = eld::ZOption::Global;
    else if (0 == zOpt.compare("defs"))
      zkind = eld::ZOption::Defs;
    else if (0 == zOpt.compare("initfirst"))
      zkind = eld::ZOption::InitFirst;
    else if (0 == zOpt.compare("muldefs"))
      zkind = eld::ZOption::MulDefs;
    else if (0 == zOpt.compare("nocopyreloc"))
      zkind = eld::ZOption::NoCopyReloc;
    else if (0 == zOpt.compare("nodefaultlib"))
      zkind = eld::ZOption::NoDefaultLib;
    else if (0 == zOpt.compare("relro"))
      zkind = eld::ZOption::Relro;
    else if (0 == zOpt.compare("norelro"))
      zkind = eld::ZOption::NoRelro;
    else if (0 == zOpt.compare("lazy"))
      zkind = eld::ZOption::Lazy;
    else if (0 == zOpt.compare("now"))
      zkind = eld::ZOption::Now;
    else if (0 == zOpt.compare("origin"))
      zkind = eld::ZOption::Origin;
    else if (0 == zOpt.compare("text"))
      zkind = eld::ZOption::Text;
    else if (0 == zOpt.compare("noexecstack"))
      zkind = eld::ZOption::NoExecStack;
    else if (0 == zOpt.compare("nognustack"))
      zkind = eld::ZOption::NoGnuStack;
    else if (0 == zOpt.compare("execstack"))
      zkind = eld::ZOption::ExecStack;
    else if (zOpt.starts_with("common-page-size=")) {
      zkind = eld::ZOption::CommPageSize;
      zOpt.drop_front(17).getAsInteger(0, zVal);
    } else if (zOpt.starts_with("max-page-size=")) {
      zkind = eld::ZOption::MaxPageSize;
      zOpt.drop_front(14).getAsInteger(0, zVal);
    } else if (0 == zOpt.compare("nodelete")) {
      zkind = eld::ZOption::NoDelete;
    } else if (0 == zOpt.compare("compactdyn")) {
      zkind = eld::ZOption::CompactDyn;
    } else if (0 == zOpt.compare("force-bti")) {
      zkind = eld::ZOption::ForceBTI;
    } else if (0 == zOpt.compare("pac-plt")) {
      zkind = eld::ZOption::ForcePACPLT;
    }
    if (!m_Config.options().addZOption(eld::ZOption(zkind, zVal))) {
      errs() << "Invalid -z option specified " << zOpt << "\n";
      return false;
    }
  }

  // --image-base
  if (llvm::opt::Arg *arg = Args.getLastArg(T::image_base)) {
    llvm::StringRef value = arg->getValue();
    uint64_t addr = 0;
    if (value.getAsInteger(0, addr)) {
      m_Config.raise(Diag::err_invalid_image_base) << value;
      return false;
    }
    m_Config.options().setImageBase(addr);
    if (m_Config.options().hasMaxPageSize() &&
        (addr % m_Config.options().maxPageSize()) != 0)
      m_Config.raise(Diag::warn_image_base_not_multiple_page_size) << value;
    m_Config.addCommandLine(Table->getOptionName(T::image_base), value);
  }

  // --section-start=section=addr
  for (llvm::opt::Arg *arg : Args.filtered(T::section_start)) {
    llvm::StringRef value = arg->getValue();
    const size_t pos = value.find('=');
    uint64_t addr = 0;
    if (value.substr(pos + 1).getAsInteger(0, addr)) {
      errs() << "Invalid value for" << arg->getOption().getPrefixedName()
             << ": " << arg->getValue() << "\n";
      return false;
    }
    m_Config.options().addressMap().insert(
        std::make_pair(value.substr(0, pos), addr));
  }

  if (llvm::opt::Arg *arg = Args.getLastArg(T::orphan_handling)) {
    llvm::StringRef value = arg->getValue();
    if (!m_Config.options().setOrphanHandlingMode(value)) {
      errs() << "Invalid value for" << arg->getOption().getPrefixedName()
             << ": " << arg->getValue() << "\n";
      return false;
    }
  }

  // -Tbss=value
  if (llvm::opt::Arg *arg = Args.getLastArg(T::Tbss)) {
    llvm::StringRef value = arg->getValue();
    uint64_t addr = 0;
    if (value.getAsInteger(0, addr)) {
      errs() << "Invalid value for" << arg->getOption().getPrefixedName()
             << ": " << arg->getValue() << "\n";
      return false;
    }
    m_Config.options().addressMap().insert(std::make_pair(".bss", addr));
  }

  // -Tdata=value
  if (llvm::opt::Arg *arg = Args.getLastArg(T::Tdata)) {
    llvm::StringRef value = arg->getValue();
    uint64_t addr = 0;
    if (value.getAsInteger(0, addr)) {
      errs() << "Invalid value for" << arg->getOption().getPrefixedName()
             << ": " << arg->getValue() << "\n";
      return false;
    }
    m_Config.options().addressMap().insert(std::make_pair(".data", addr));
  }

  // -Ttext=value
  if (llvm::opt::Arg *arg = Args.getLastArg(T::Ttext)) {
    llvm::StringRef value = arg->getValue();
    uint64_t addr = 0;
    if (value.getAsInteger(0, addr)) {
      errs() << "Invalid value for" << arg->getOption().getPrefixedName()
             << ": " << arg->getValue() << "\n";
      return false;
    }
    m_Config.options().addressMap().insert(std::make_pair(".text", addr));
  }

  // --dynamic-list
  for (auto *Arg : Args.filtered(T::dynamic_list))
    m_Config.options().getDynList().emplace(Arg->getValue());
  if (m_Config.options().getDynList().size())
    m_Config.options().setDynamicList();

  // --version-script
  for (auto *Arg : Args.filtered(T::version_script))
    m_Config.options().getVersionScripts().emplace(Arg->getValue());
  if (m_Config.options().getVersionScripts().size())
    m_Config.options().setVersionScript();

  // --extern-list
  for (auto *Arg : Args.filtered(T::extern_list))
    m_Config.options().getExternList().emplace(Arg->getValue());

  // --exclude-lto-filelist
  std::vector<std::string> ltoExcludes;
  std::vector<std::string> ltoincludes;
  if (m_Config.options().hasLTO()) {
    for (auto *Arg : Args.filtered(T::exclude_lto_filelist)) {
      m_Config.options().getExcludeLTOFiles().emplace(Arg->getValue());
      ltoExcludes.push_back(Arg->getValue());
    }
  } else {
    // --include-lto-filelist
    for (auto *Arg : Args.filtered(T::include_lto_filelist)) {
      m_Config.options().getIncludeLTOFiles().emplace(Arg->getValue());
      ltoincludes.push_back(Arg->getValue());
    }
  }
  m_Config.addCommandLine(Table->getOptionName(T::exclude_lto_filelist),
                          ltoExcludes);
  m_Config.addCommandLine(Table->getOptionName(T::include_lto_filelist),
                          ltoincludes);

  // --exclude-libs
  for (auto *Arg : Args.filtered(T::exclude_libs)) {
    llvm::StringRef list = Arg->getValue();
    while (list.size()) {
      auto libs = list.split(',');
      m_Config.options().excludeLIBS().insert(libs.first.str());
      list = libs.second;
    }
  }

  // --no-verify
  if (Args.hasArg(T::no_verify))
    m_Config.options().setVerifyLink(false);

  // --allow-incompatible-section-mix
  if (Args.hasArg(T::allow_incompatible_section_mix))
    m_Config.options().setAllowIncompatibleSectionsMix(true);

  if (llvm::opt::Arg *arg = Args.getLastArg(T::output_file)) {
    std::string outputFileName = arg->getValue();
    m_Config.options().setOutputFileName(outputFileName);
    m_Config.addCommandLine(Table->getOptionName(T::output_file),
                            outputFileName.c_str());
  }

  std::string conflictingOption;

  // -shared
  // This must occur after -pie/-no-pie is processed so the PIE mode is set
  // correctly.
  if (Args.getLastArg(T::shared)) {
    m_Config.options().setShared();
    m_Config.setCodeGenType(eld::LinkerConfig::DynObj);
    conflictingOption = "shared";
  } else if (m_Config.options().isPIE()) {
    m_Config.setCodeGenType(eld::LinkerConfig::DynObj);
    conflictingOption = "pie";
  } else if (Args.getLastArg(T::relocatable)) {
    m_Config.setCodeGenType(eld::LinkerConfig::Object);
    conflictingOption = "relocatable";
    if (Args.hasArg(T::gc_sections))
      m_Config.raise(Diag::warn_gc_sections_relocatable);
  } else
    m_Config.setCodeGenType(eld::LinkerConfig::Exec);

  // Disable --gc-sections, --print-gc-sections for Partial Linking.
  if (m_Config.codeGenType() != eld::LinkerConfig::Object) {
    // --gc-sections
    bool enableGC = Args.hasArg(T::gc_sections);
    m_Config.options().setGCSections(enableGC);
    m_Config.addCommandLine(Table->getOptionName(T::gc_sections), enableGC);
    // --print-gc-sections
    m_Config.options().setPrintGCSections(Args.hasArg(T::print_gc_sections));
  }

  // Disable emit relocs if -shared/-pie/relocatable
  if (m_Config.options().emitRelocs() && !conflictingOption.empty()) {
    m_Config.raise(Diag::warn_incompatible_option)
        << "-emit-relocs" << conflictingOption;
    m_Config.options().setEmitRelocs(false);
    m_Config.options().setEmitGNUCompatRelocs(false);
  }

  if ((m_Config.options().emitRelocs() ||
       m_Config.codeGenType() == eld::LinkerConfig::Object) &&
      (m_Config.options().getStripSymbolMode() !=
       eld::GeneralOptions::KeepAllSymbols)) {
    m_Config.raise(Diag::warn_strip_symbols) << "-emit-relocs/-r";
    m_Config.options().setStripSymbols(eld::GeneralOptions::KeepAllSymbols);
  }

  if (m_Config.options().isPatchEnable()) {
    if (!m_Config.options().isStripTemporaryOrLocalSymbols())
      m_Config.raise(Diag::warn_strip_symbols) << "--patch-enable";
    m_Config.options().setStripSymbols(eld::GeneralOptions::KeepAllSymbols);
  }

  //
  // Thread Options.
  //

  // --no-threads, --threads
  if (!Args.getLastArg(T::no_threads) || Args.getLastArg(T::threads)) {
    m_Config.options().enableThreads();
    m_Config.addCommandLine(Table->getOptionName(T::threads), true);
  } else if (Args.getLastArg(T::no_threads)) {
    // --no-threads
    m_Config.options().disableThreads();
    m_Config.options().setNumThreads(1);
    m_Config.addCommandLine(Table->getOptionName(T::threads), false);
  }

  // If the user user --enable-threads=all
  if (llvm::opt::Arg *arg = Args.getLastArg(T::enable_threads)) {
    llvm::StringRef Opt = arg->getValue();
    if (Opt == "all") {
      m_Config.setGlobalThreadingEnabled();
      m_Config.options().enableThreads();
    } else {
      errs() << "Invalid value for" << arg->getOption().getPrefixedName()
             << ": " << arg->getValue() << "\n";
      return false;
    }
  }

  if (m_Config.options().threadsEnabled()) {
    // --thread-count
    int numThreads =
        getInteger(Args, T::thread_count, std::thread::hardware_concurrency());
    m_Config.options().setNumThreads(numThreads);
    m_Config.addCommandLine(Table->getOptionName(T::thread_count),
                            std::to_string(numThreads).c_str());
  }

  //
  // SymDef Options.
  //

  // --symdef
  if (Args.getLastArg(T::symdef))
    m_Config.options().setSymDef();

  // --symdef-file=<file>
  if (llvm::opt::Arg *arg = Args.getLastArg(T::symdef_file))
    m_Config.options().setSymDefFile(arg->getValue());

  // --symdef-style=<style>
  if (llvm::opt::Arg *arg = Args.getLastArg(T::symdef_style)) {
    if (!m_Config.options().setSymDefFileStyle(arg->getValue())) {
      m_Config.raise(Diag::error_invalid_option_symdef_style)
          << arg->getValue();
      return false;
    }
    m_Config.setSymDefStyle(m_Config.options().symDefFileStyle());
  }

  // Disable symdef if -shared/-pie/-relocatable
  if (m_Config.options().symDef() && !conflictingOption.empty()) {
    m_Config.raise(Diag::warn_incompatible_option)
        << "-symdef/--symdef-file" << conflictingOption;
    m_Config.options().setSymDef(false);
  }

  // --unresolved-symbols=ignore-all,report-all,ignore-in-object-files,
  //                      ignore-in-shared-libs
  if (llvm::opt::Arg *arg = Args.getLastArg(T::unresolved_symbols)) {
    if (!m_Config.options().setUnresolvedSymbolPolicy(arg->getValue())) {
      errs() << "Invalid value for" << arg->getOption().getPrefixedName()
             << ": " << arg->getValue() << "\n";
      return false;
    }
  }

  // --plugin-config=<config>.yaml
  for (const auto *Arg : Args.filtered(T::plugin_config))
    m_Config.options().addPluginConfig(Arg->getValue());

  // --demangle-style
  if (llvm::opt::Arg *arg = Args.getLastArg(T::demangle_style)) {
    if (!m_Config.options().setDemangleStyle(arg->getValue())) {
      errs() << "Invalid value for" << arg->getOption().getPrefixedName()
             << ": " << arg->getValue() << "\n";
      return false;
    }
  }

  // --no-demangle
  if (Args.getLastArg(T::no_demangle))
    m_Config.options().setDemangleStyle("none");

  // --demangle
  if (Args.getLastArg(T::demangle))
    m_Config.options().setDemangleStyle("demangle");

  /// --progress-bar
  if (Args.getLastArg(T::progress_bar))
    m_Config.options().setShowProgressBar();

  std::optional<std::string> reproduceFileName;
  // --reproduce
  if (llvm::opt::Arg *arg = Args.getLastArg(T::reproduce)) {
    m_Config.options().setRecordInputfiles();
    reproduceFileName = arg->getValue();
  }

  // --reproduce-compressed
  if (llvm::opt::Arg *arg = Args.getLastArg(T::reproduce_compressed)) {
    m_Config.options().setRecordInputfiles();
    m_Config.options().setCompressTar();
    reproduceFileName = arg->getValue();
  }

  // --reproduce-on-fail
  if (llvm::opt::Arg *arg = Args.getLastArg(T::reproduce_on_fail)) {
    m_Config.options().setReproduceOnFail(true);
    reproduceFileName = arg->getValue();
  }

  if (reproduceFileName)
    m_Config.options().setTarFile(*reproduceFileName);

  std::optional<std::string> reproduceInEnvironment =
      llvm::sys::Process::GetEnv("ELD_REPRODUCE_CREATE_TAR");

  if (reproduceInEnvironment && !m_Config.options().getRecordInputFiles()) {
    m_Config.options().setRecordInputfiles();
    llvm::SmallString<256> outputPath;
    std::error_code EC =
        llvm::sys::fs::createTemporaryFile("reproduce", "tar", outputPath);
    if (EC) {
      m_Config.raise(Diag::unable_to_create_temporary_file) << "reproduce.tar";
      return false;
    }
    m_Config.options().setTarFile(outputPath.str().str());
    if (m_Config.getPrinter()->isVerbose())
      m_Config.raise(Diag::reproduce_in_env);
  }

  // --mapping-file
  if (llvm::opt::Arg *arg = Args.getLastArg(T::mapping_file)) {
    m_Config.options().setHasMappingFile(true);
    m_Config.options().setMappingFileName(arg->getValue());
    eld::MappingFileReader reader(arg->getValue());
    if (!reader.readMappingFile(m_Config))
      m_Config.raise(Diag::unable_to_find_mapping_file)
          << m_Config.options().getMappingFileName();
  }

  // --dump-mapping-file
  if (llvm::opt::Arg *arg = Args.getLastArg(T::dump_mapping_file)) {
    m_Config.options().setDumpMappings(true);
    m_Config.options().setMappingDumpFile(arg->getValue());
  }

  // --dump-response-file
  if (llvm::opt::Arg *arg = Args.getLastArg(T::dump_response_file)) {
    m_Config.options().setDumpResponse(true);
    m_Config.options().setResponseDumpFile(arg->getValue());
  }

  // --summary
  if (Args.getLastArg(T::summary))
    m_Config.options().setDisplaySummary();

  // --allow-bss-conversion
  if (Args.hasArg(T::allow_bss_conversion))
    m_Config.options().setAllowBSSConversion(true);

  // --no-dynamic-linker
  if (Args.hasArg(T::no_dynamic_linker))
    m_Config.options().setHasDynamicLinker(false);

  // --unique-output-sections
  if (Args.hasArg(T::unique_output_sections)) {
    if (m_Config.isLinkPartial())
      m_Config.options().setEmitUniqueOutputSections(true);
    else
      m_Config.raise(Diag::unique_output_sections_unsupported);
  }

  // --global-merge-non-alloc-strings
  if (Args.hasArg(T::global_merge_non_alloc_strings))
    m_Config.options().enableGlobalStringMerge();

  // --trace-linker-script
  if (Args.hasArg(T::trace_linker_script))
    checkAndRaiseTraceDiagEntry(m_Config.options().setTrace("linker-script"));

  // -Wall support
  for (auto *Arg : Args.filtered(T::W)) {
    m_Config.setWarningOption(Arg->getValue());
  }

  if (Args.hasArg(T::use_old_style_trampoline_name))
    m_Config.setUseOldStyleTrampolineName(true);

  // --check-sections
  if (Args.hasArg(T::enable_overlap_checks))
    m_Config.options().setEnableCheckSectionOverlaps();

  // --no-check-sections
  if (Args.hasArg(T::disable_overlap_checks))
    m_Config.options().setDisableCheckSectionOverlaps();

  if (Args.hasArg(T::thin_archive_rule_matching_compatibility))
    m_Config.options().setThinArchiveRuleMatchingCompatibility();

  // --sort-common
  if (Args.hasArg(T::sort_common))
    m_Config.options().setSortCommon();

  // --sort-common=ascending/descending
  if (llvm::opt::Arg *arg = Args.getLastArg(T::sort_common_val)) {
    if (!m_Config.options().setSortCommon(arg->getValue())) {
      m_Config.raise(Diag::invalid_option) << arg->getValue() << "sort-common";
      return false;
    }
  }

  // --sort-section=alignment/name
  if (llvm::opt::Arg *arg = Args.getLastArg(T::sort_section)) {
    if (!m_Config.options().setSortSection(arg->getValue())) {
      m_Config.raise(Diag::invalid_option) << arg->getValue() << "sort-section";
      return false;
    }
  }

  // --print-memory-usage
  m_Config.options().setShowPrintMemoryUsage(
      Args.hasArg(T::print_memory_usage));

  if (Args.hasArg(T::build_id))
    m_Config.options().setDefaultBuildID();

  if (auto *Arg = Args.getLastArg(T::build_id_val))
    m_Config.options().setBuildIDValue(Arg->getValue());

  // --ignore-unknown-opts
  if (Args.hasArg(T::ignore_unknown_opts))
    m_Config.options().setIgnoreUnknownOptions();

  // --no-default-plugins
  if (Args.hasArg(T::noDefaultPlugins))
    m_Config.options().setNoDefaultPlugins();

  m_Config.options().setUnknownOptions(Args.getAllArgValues(T::UNKNOWN));
  return true;
}

template <class T>
bool GnuLdDriver::createInputActions(llvm::opt::InputArgList &Args,
                                     std::vector<eld::InputAction *> &actions) {
  // # of regular objects, script, and namespec.
  size_t input_num = 0;
  int GroupMatchCount = 0;

  for (llvm::opt::Arg *arg : Args) {
    switch (arg->getOption().getID()) {
    // -T script, --default-script
    case T::default_script: {
      // --default-script is used only if a script is not specified.
      if (Args.hasArg(T::T))
        break;
    }
      LLVM_FALLTHROUGH;

    case T::T: {
      m_Config.options().getScriptList().push_back(arg->getValue());
      actions.push_back(eld::make<eld::ScriptAction>(
          arg->getValue(), eld::ScriptFile::LDScript, m_Config,
          m_Config.getPrinter()));
      ++input_num;
    } break;

    case T::R: {
      m_Config.options().getScriptList().push_back(arg->getValue());
      actions.push_back(eld::make<eld::JustSymbolsAction>(
          arg->getValue(), m_Config, m_Config.getPrinter()));
      ++input_num;
    } break;

    // --defsym=symbol=expr
    case T::defsym: {
      actions.push_back(
          eld::make<eld::DefSymAction>(arg->getValue(), m_Config.getPrinter()));
    } break;

    // -l namespec
    case T::l:
    case T::namespec: {
      actions.push_back(eld::make<eld::NamespecAction>(arg->getValue(),
                                                       m_Config.getPrinter()));
      ++input_num;
    } break;

    // --whole-archive
    case T::whole_archive:
      actions.push_back(
          eld::make<eld::WholeArchiveAction>(m_Config.getPrinter()));
      m_Config.addCommandLine(Table->getOptionName(T::whole_archive), true);
      break;

    // --no-whole-archive
    case T::no_whole_archive:
      actions.push_back(
          eld::make<eld::NoWholeArchiveAction>(m_Config.getPrinter()));
      m_Config.addCommandLine(Table->getOptionName(T::whole_archive), false);
      break;

    // --as-needed
    case T::as_needed:
      actions.push_back(eld::make<eld::AsNeededAction>(m_Config.getPrinter()));
      break;

    // --no-as-needed
    case T::no_as_needed:
      actions.push_back(
          eld::make<eld::NoAsNeededAction>(m_Config.getPrinter()));
      break;

    // FIXME: Shouldn't we also add -call_shared here?
    // -Bdynamic
    case T::Bdynamic:
    case T::dynamic:
      actions.push_back(eld::make<eld::BDynamicAction>(m_Config.getPrinter()));
      break;

    // FIXME: Shouldn't we also add -dn, -non_shared and -Bstatic here?
    // -Bstatic
    case T::static_link:
      actions.push_back(eld::make<eld::BStaticAction>(m_Config.getPrinter()));
      break;

    // --start-group
    case T::start_group: {
      if (arg->getNumValues() == 0 && m_Config.showCommandLineWarnings())
        m_Config.raise(Diag::warn_group_is_empty);
      if (GroupMatchCount) {
        m_Config.raise(Diag::nested_group_not_allowed);
        m_Config.raise(Diag::linking_had_errors);
        return false;
      }
      ++GroupMatchCount;
      actions.push_back(
          eld::make<eld::StartGroupAction>(m_Config.getPrinter()));
      m_Config.addCommandLine(Table->getOptionName(T::start_group), true);
    } break;

    // --end-group
    case T::end_group: {
      --GroupMatchCount;
      actions.push_back(eld::make<eld::EndGroupAction>(m_Config.getPrinter()));
      m_Config.addCommandLine(Table->getOptionName(T::end_group), true);
    } break;

    case T::input_format: {
      actions.push_back(eld::make<eld::InputFormatAction>(
          arg->getValue(), m_Config.getPrinter()));
      m_Config.addCommandLine(Table->getOptionName(T::input_format), true);
    } break;

    case T::INPUT: {
      actions.push_back(eld::make<eld::InputFileAction>(arg->getValue(),
                                                        m_Config.getPrinter()));
      ++input_num;
    } break;

    default:
      break;
    }
  }

  if (GroupMatchCount != 0) {
    m_Config.raise(Diag::mismatched_group);
    m_Config.raise(Diag::linking_had_errors);
    return false;
  }

  if (input_num == 0) {
    m_Config.raise(Diag::err_no_inputs);
    m_Config.raise(Diag::linking_had_errors);
    return false;
  }

  return true;
}

template <class T>
bool GnuLdDriver::processLLVMOptions(llvm::opt::InputArgList &Args) const {
  // Parse and evaluate -mllvm options.
  std::vector<const char *> V;
  V.push_back("eld (LLVM option parsing)");
  for (auto *Arg : Args.filtered(T::mllvm))
    V.push_back(Arg->getValue());
  cl::ParseCommandLineOptions(V.size(), V.data());
  return true;
}

// march values for RISCV can also be rv32i, rv32g, rv32imc, rv64 etc
// These need to be converted to riscv32 or riscv64 for LLVM target
// registry entry to be found by the driver.
static inline std::string parseMarchShortName(llvm::StringRef ShortName) {
  if (ShortName.starts_with("rv32"))
    return "riscv32";
  else if (ShortName.starts_with("rv64"))
    return "riscv64";
  else
    return ShortName.str();
}

template <class T>
bool GnuLdDriver::processTargetOptions(llvm::opt::InputArgList &Args) {
  llvm::Triple triple;
  int marchPos = -1, mtriplePos = -1;
  // -mtriple.
  if (llvm::opt::Arg *arg = Args.getLastArg(T::mtriple)) {
    triple.setTriple(arg->getValue());
    if (arg->getValue() != nullptr)
      mtriplePos = arg->getIndex();
  } else {
    if (!m_Config.targets().hasTriple())
      triple.setTriple(llvm::sys::getDefaultTargetTriple());
    else
      triple = m_Config.targets().triple();
  }

  // -march=value
  std::string march;
  if (llvm::opt::Arg *arg = Args.getLastArg(T::march)) {
    march = arg->getValue();
    if (!march.empty()) {
      marchPos = arg->getIndex();
      march = parseMarchShortName(march);
    }
    m_Config.targets().setArch(march);
    m_Config.addCommandLine(Table->getOptionName(T::march), arg->getValue());
  }

  if ((marchPos == -1) && (mtriplePos == -1)) {
    std::string MArch = m_Config.targets().getArch();
    if (MArch != triple.getArchTypeName(triple.getArch()))
      triple.setTriple(MArch);
  } else if ((marchPos != -1) && mtriplePos == -1) {
    // If a triple is not passed in the command line, lets infer the triple from
    // march only if the architecture from the triple is not the same as
    // march.
    if (march != triple.getArchTypeName(triple.getArch()))
      triple.setTriple(march);
  } else if (marchPos > mtriplePos) {
    triple.setTriple(march);
  } else if (mtriplePos > marchPos) {
    m_Config.targets().setArch(triple.getArchTypeName(triple.getArch()).str());
  }

  if (llvm::opt::Arg *arg = Args.getLastArg(T::mcpu))
    m_Config.targets().setTargetCPU(arg->getValue());

  // --mabi=value
  if (llvm::opt::Arg *arg = Args.getLastArg(T::mabi)) {
    llvm::StringRef abi(arg->getValue());
    if (abi.size()) {
      m_Config.options().setABIstring(arg->getValue());
      m_Config.options().setValidateArchOptions();
      m_Config.addCommandLine(Table->getOptionName(T::mabi), arg->getValue());
    }
  }

  // -m <emulation>
  if (llvm::opt::Arg *arg = Args.getLastArg(T::emulation)) {
    m_Config.options().setEmulation(arg->getValue());
    m_Config.addCommandLine(Table->getOptionName(T::emulation),
                            arg->getValue());
  }

  m_Config.targets().setTriple(triple);
  return true;
}

void GnuLdDriver::ReproduceInterruptHandler() { writeReproduceTar(nullptr); }

bool writeDump(const std::string outputFile, llvm::StringRef contents) {
  std::error_code error;
  llvm::raw_fd_ostream *dumpFile =
      new llvm::raw_fd_ostream(outputFile, error, llvm::sys::fs::OF_None);
  if (error)
    return false;
  *dumpFile << contents;
  delete dumpFile;
  return true;
}

/// write output files for --reproduce and report any errors
/// Also used as signal handler callback
void GnuLdDriver::writeReproduceTar(void *cookie) {
  eld::OutputTarWriter *outputTar = ThisModule->getOutputTarWriter();
  DiagnosticEngine *DiagEngine = ThisModule->getConfig().getDiagEngine();
  bool mappingfile = outputTar->createMappingFile();
  if (!mappingfile)
    DiagEngine->raise(Diag::unable_to_add_ini_hash_entry);
  bool versionfile = outputTar->createVersionFile();
  if (!versionfile)
    DiagEngine->raise(Diag::unable_to_add_version_file)
        << outputTar->getVersionFileName() << outputTar->getTarFileName();
  bool out = outputTar->writeOutput(
      ThisModule->getConfig().options().showProgressBar());
  if (!out)
    DiagEngine->raise(Diag::unable_to_write_reproduce_tarball);
  if (ThisModule->getConfig().options().getDumpMappings())
    writeDump(ThisModule->getConfig().options().getMappingDumpFile(),
              outputTar->getMappings());
}

namespace {

// Return true if the option takes a path that needs to overridden by reproduce.
template <class T> bool RewritePathOnReproduce(unsigned Option) {
  return false;
}

#ifdef ELD_ENABLE_TARGET_RISCV
template <>
bool RewritePathOnReproduce<OPT_RISCVLinkOptTable>(unsigned Option) {
  return Option == OPT_RISCVLinkOptTable::patch_base;
}
#endif

} // namespace

template <class T>
bool GnuLdDriver::processReproduceOption(
    llvm::opt::InputArgList &Args, eld::OutputTarWriter *outputTar,
    std::vector<eld::InputAction *> &actions) {
  // create response string
  llvm::SmallString<0> responseData;
  llvm::raw_svector_ostream os(responseData);
  if (!m_Config.options().getDumpResponse())
    os << getProgramName() << " ";
  size_t lastNamespecId = -1;

  auto zArgsRange = Args.filtered(T::dash_z);
  auto zArgIt = zArgsRange.begin();

  for (const auto *arg : Args) {
    switch (arg->getOption().getID()) {
    case T::dump_response_file:
    case T::dump_mapping_file:
    case T::reproduce:
    case T::reproduce_on_fail:
    case T::L:
      break;
    case T::l:
    case T::namespec: {
      for (size_t i = lastNamespecId + 1; i < actions.size(); ++i) {
        auto action = actions[i];
        if (action->getInputActionKind() == eld::InputAction::Namespec) {
          lastNamespecId = i;
          auto ipt = action->getInput();
          if (!ipt)
            return false;
          os << outputTar->rewritePath(ipt->getName()) << ' ';
          break;
        }
      }
      break;
    }
    case T::INPUT:
      os << outputTar->rewritePath(arg->getValue()) << ' ';
      break;
    case T::plugin_config: {
      const eld::sys::fs::Path *P = m_Config.directories().findFile(
          "plugin configuration file", arg->getValue(), "");
      outputTar->createAndAddConfigFile(arg->getValue(),
                                        P ? P->getFullPath() : "");
      os << arg->getSpelling() << ' ' << outputTar->rewritePath(arg->getValue())
         << ' ';
      break;
    }
    case T::output_file:
    case T::Map:
    case T::T:
    case T::R:
    case T::dynamic_list:
    case T::extern_list:
    case T::version_script:
      os << arg->getSpelling() << ' ' << outputTar->rewritePath(arg->getValue())
         << ' ';
      break;
    case T::dash_z: {
      ASSERT(zArgIt != zArgsRange.end(), "Expected valid z argument iterator!");
      os << arg->getSpelling() << ' ';
      llvm::StringRef zOpt = (*zArgIt)->getValue();
      os << zOpt << ' ';
      ++zArgIt;
      break;
    }
    default:
      if (RewritePathOnReproduce<T>(arg->getOption().getID())) {
        os << arg->getSpelling() << ' '
           << outputTar->rewritePath(arg->getValue()) << ' ';
      } else {
        os << arg->getAsString(Args) << ' ';
      }
      break;
    }
  }
  if (!outputTar->getLTOObjects().empty()) {
    os << "-flto-options=lto-output-file=";
    auto &LTOObjects = outputTar->getLTOObjects();
    for (size_t i = 0; i < LTOObjects.size() - 1; ++i)
      os << outputTar->rewritePath(LTOObjects[i]) << ",";
    os << outputTar->rewritePath(LTOObjects.back()) << " ";
  }
  os << "--mapping-file=" << outputTar->getMappingFileName() << "\n";
  outputTar->createResponseFile(responseData.str());
  if (m_Config.options().getDumpResponse())
    writeDump(m_Config.options().getResponseDumpFile(), responseData.str());
  return true;
}

void GnuLdDriver::defaultSignalHandler(void *cookie) {
  DiagnosticEngine *DiagEngine = ThisModule->getConfig().getDiagEngine();
  std::string commandLine = "";
  for (auto arg : ThisModule->getConfig().options().args()) {
    if (arg) {
      commandLine.append(std::string(arg));
      commandLine.append(" ");
    }
  }
  commandLine.append("--reproduce build.tar");
  llvm::SmallString<256> outputPath;
  std::error_code EC =
      llvm::sys::fs::createTemporaryFile("reproduce", "sh", outputPath);

  std::error_code error;
  auto file = std::make_unique<llvm::raw_fd_ostream>(outputPath.str(), error,
                                                     llvm::sys::fs::OF_None);

  bool pluginCrash = false;
  for (eld::Plugin *P : ThisModule->getScript().getPlugins()) {
    if (P->isRunning()) {
      pluginCrash = true;
      DiagEngine->raise(Diag::plugin_crash) << P->getName();
    }
  }

  if (!pluginCrash) {
    DiagEngine->raise(Diag::unexpected_linker_behavior);
  }

  // FIXME: EC should be checked before using outputPath variable.
  if (EC || error) {
    DiagEngine->raise(Diag::linker_crash_use_reproduce) << "--reproduce";
    return;
  }
  *file << commandLine;
  DiagEngine->raise(Diag::linker_crash_use_reproduce) << outputPath.str();
}

eld::Module *GnuLdDriver::ThisModule = nullptr;

template <class T>
bool GnuLdDriver::doLink(llvm::opt::InputArgList &Args,
                         std::vector<eld::InputAction *> &actions) {
  // Get the target specific parser.
  std::string error;
  llvm::Triple Triple = m_Config.targets().triple();
  const llvm::Target *LLVMTarget =
      llvm::TargetRegistry::lookupTarget(Triple.str(), error);
  if (nullptr == LLVMTarget) {
    m_Config.raise(Diag::cannot_find_target) << error;
    return false;
  }
  const eld::Target *ELDTarget =
      eld::TargetRegistry::lookupTarget(Triple.str(), error);
  if (nullptr == ELDTarget) {
    m_Config.raise(Diag::cannot_find_target) << error;
    return false;
  }

  // This is needed to make sure for -march aarch64,
  // default triple is not arm--linux-gnu else it will cause issues in LTO
  m_Config.targets().setTriple(Triple);
  std::unique_ptr<eld::ELDTargetMachine> target_machine(
      ELDTarget->createTargetMachine(m_Config.targets().triple().getTriple(),
                                     *LLVMTarget));
  eld::LayoutPrinter *layoutPrinter = nullptr;
  if (!m_Config.options().layoutFile().empty() || m_Config.options().printMap())
    layoutPrinter = eld::make<eld::LayoutPrinter>(m_Config);
  ThisModule = eld::make<eld::Module>(m_Script, m_Config, layoutPrinter);

  // Handle Map Style and set default MapStyle
  llvm::ArrayRef<std::string> MapStyles = m_Config.options().mapStyle();
  if (MapStyles.size()) {
    m_Config.options().setDefaultMapStyle(MapStyles[0]);
    if (m_Config.options().checkAndUpdateMapStyleForPrintMap())
      MapStyles = m_Config.options().mapStyle();
    // Create LayoutPrinters.
    m_Config.raise(Diag::mapstyles_used) << llvm::join(MapStyles, ",");
    for (auto &Style : MapStyles) {
      if (!ThisModule->createLayoutPrintersForMapStyle(Style))
        return false; // fail the link
    }
  } else {
    m_Config.raise(Diag::mapstyles_used)
        << m_Config.options().getDefaultMapStyle();
    if (!ThisModule->createLayoutPrintersForMapStyle(
            m_Config.options().getDefaultMapStyle()))
      return false; // fail the link
  }

  bool linkStatus = false;
  {
    eld::Linker linker(*ThisModule, m_Config);
    llvm::sys::AddSignalHandler(defaultSignalHandler, nullptr);
    m_Config.raise(Diag::default_signal_handler);
    linkStatus = linker.prepare(actions, ELDTarget);
    // llvm::errs() << "prepare: linkStatus: " << linkStatus << "\n";
    if (!linkStatus || m_Config.options().getRecordInputFiles())
      handleReproduce<T>(Args, actions, false);
    if (linkStatus)
      linkStatus = linker.link();
    // llvm::errs() << "link: linkStatus: " << linkStatus << "\n";
    if (!linkStatus || m_Config.options().getRecordInputFiles())
      handleReproduce<T>(Args, actions, true);
    linker.printLayout();
    linkStatus &= ThisModule->getPluginManager().callDestroyHook();
    // llvm::errs() << "destroy hook: linkStatus: " << linkStatus << "\n";
    linker.unloadPlugins();
    linkStatus &= emitStats(*ThisModule);
  }
  if (m_Config.options().displaySummary())
    m_Config.getDiagEngine()->finalize();
  if (!linkStatus)
    m_Config.raise(Diag::linking_had_errors);
  eld::freeArena();
  return linkStatus;
}

template <class T>
bool GnuLdDriver::overrideOptions(llvm::opt::InputArgList &Args) {
  return true;
}

template <class T>
bool GnuLdDriver::handleReproduce(llvm::opt::InputArgList &Args,
                                  std::vector<eld::InputAction *> &actions,
                                  bool writeFiles) {
  // FIXME: The below should perhaps be an assert?
  if (!m_Config.options().getRecordInputFiles() &&
      !m_Config.options().isReproduceOnFail())
    return true;
  // FIXME: Why call processReproduceOption<T>(...) twice? In the second run, we
  // can simply append any LTO objects instead of recomputing the entire thing.
  // call this twice to record information of adding new files to the
  // link
  processReproduceOption<T>(Args, ThisModule->getOutputTarWriter(), actions);
  // Register signal handlers only once.
  std::call_once(once_flag, [&]() {
    llvm::sys::AddSignalHandler(writeReproduceTar, nullptr);
    llvm::sys::SetInterruptFunction(ReproduceInterruptHandler);
    llvm::sys::SetInfoSignalFunction(ReproduceInterruptHandler);
    if (m_Config.getPrinter()->isVerbose())
      m_Config.raise(Diag::reproduce_signal_handler);
  });
  // If needed to write files, then write files
  if (writeFiles) {
    writeReproduceTar(nullptr);
  }
  return true;
}

std::string GnuLdDriver::getFlavorName() const {
  switch (m_Flavor) {
  case Flavor::AArch64:
    return "AArch64";
  case Flavor::ARM:
    return "ARM";
  case Flavor::Hexagon:
    return "Hexagon";
  case Flavor::RISCV32:
    return "RISCV32";
  case Flavor::RISCV64:
    return "RISCV64";
  case Flavor::x86_64:
    return "x86_64";
  case Flavor::Invalid:
    break;
  }
  ASSERT(false, "Invalid Flavor!");
}

void GnuLdDriver::printRepositoryVersion() const {
  std::string flavorName = getFlavorName();
  if (!flavorName.empty())
    outs() << flavorName << " ";
  outs() << "Linker repository revision: " << eld::getELDRepositoryVersion()
         << "\n";
  if (isLLVMRepositoryInfoAvailable())
    outs() << "LLVM repository revision: " << eld::getLLVMRepositoryVersion()
           << "\n";
}

std::vector<const char *> GnuLdDriver::getAllArgs(
    const std::vector<const char *> &args,
    const std::vector<llvm::StringRef> &ELDFlagsArgs) const {
  std::vector<const char *> allArgs(args.begin(), args.end());
  for (llvm::StringRef arg : ELDFlagsArgs)
    allArgs.push_back(arg.data());
  return allArgs;
}

#ifdef ELD_ENABLE_TARGET_HEXAGON
// Hexagon -- force instantiate
template bool GnuLdDriver::checkOptions<OPT_HexagonLinkOptTable>(
    llvm::opt::InputArgList &args) const;
template bool GnuLdDriver::processOptions<OPT_HexagonLinkOptTable>(
    llvm::opt::InputArgList &args);
template bool GnuLdDriver::processLLVMOptions<OPT_HexagonLinkOptTable>(
    llvm::opt::InputArgList &args) const;
template bool GnuLdDriver::processTargetOptions<OPT_HexagonLinkOptTable>(
    llvm::opt::InputArgList &args);
template bool GnuLdDriver::createInputActions<OPT_HexagonLinkOptTable>(
    llvm::opt::InputArgList &Args, std::vector<eld::InputAction *> &actions);
template bool GnuLdDriver::overrideOptions<OPT_HexagonLinkOptTable>(
    llvm::opt::InputArgList &args);
template bool GnuLdDriver::doLink<OPT_HexagonLinkOptTable>(
    llvm::opt::InputArgList &Args, std::vector<eld::InputAction *> &actions);
template bool GnuLdDriver::handleReproduce<OPT_HexagonLinkOptTable>(
    llvm::opt::InputArgList &Args, std::vector<eld::InputAction *> &actions,
    bool);
#endif

#if defined(ELD_ENABLE_TARGET_ARM) || defined(ELD_ENABLE_TARGET_AARCH64)
// ARM -- force instantiate
template bool GnuLdDriver::checkOptions<OPT_ARMLinkOptTable>(
    llvm::opt::InputArgList &args) const;
template bool
GnuLdDriver::processOptions<OPT_ARMLinkOptTable>(llvm::opt::InputArgList &args);
template bool GnuLdDriver::processLLVMOptions<OPT_ARMLinkOptTable>(
    llvm::opt::InputArgList &args) const;
template bool GnuLdDriver::processTargetOptions<OPT_ARMLinkOptTable>(
    llvm::opt::InputArgList &args);
template bool GnuLdDriver::createInputActions<OPT_ARMLinkOptTable>(
    llvm::opt::InputArgList &Args, std::vector<eld::InputAction *> &actions);
template bool GnuLdDriver::overrideOptions<OPT_ARMLinkOptTable>(
    llvm::opt::InputArgList &args);
template bool GnuLdDriver::doLink<OPT_ARMLinkOptTable>(
    llvm::opt::InputArgList &Args, std::vector<eld::InputAction *> &actions);
template bool GnuLdDriver::handleReproduce<OPT_ARMLinkOptTable>(
    llvm::opt::InputArgList &Args, std::vector<eld::InputAction *> &actions,
    bool);
#endif

#if ELD_ENABLE_TARGET_RISCV
// RISCV -- force instantiate
template bool GnuLdDriver::checkOptions<OPT_RISCVLinkOptTable>(
    llvm::opt::InputArgList &args) const;
template bool GnuLdDriver::processOptions<OPT_RISCVLinkOptTable>(
    llvm::opt::InputArgList &args);
template bool GnuLdDriver::processLLVMOptions<OPT_RISCVLinkOptTable>(
    llvm::opt::InputArgList &args) const;
template bool GnuLdDriver::processTargetOptions<OPT_RISCVLinkOptTable>(
    llvm::opt::InputArgList &args);
template bool GnuLdDriver::createInputActions<OPT_RISCVLinkOptTable>(
    llvm::opt::InputArgList &Args, std::vector<eld::InputAction *> &actions);
template bool GnuLdDriver::overrideOptions<OPT_RISCVLinkOptTable>(
    llvm::opt::InputArgList &args);
template bool GnuLdDriver::doLink<OPT_RISCVLinkOptTable>(
    llvm::opt::InputArgList &Args, std::vector<eld::InputAction *> &actions);
template bool GnuLdDriver::handleReproduce<OPT_RISCVLinkOptTable>(
    llvm::opt::InputArgList &Args, std::vector<eld::InputAction *> &actions,
    bool);
#endif

#ifdef ELD_ENABLE_TARGET_X86_64
// x86_64 -- force instantiate
template bool GnuLdDriver::checkOptions<OPT_x86_64LinkOptTable>(
    llvm::opt::InputArgList &args) const;
template bool GnuLdDriver::processOptions<OPT_x86_64LinkOptTable>(
    llvm::opt::InputArgList &args);
template bool GnuLdDriver::processLLVMOptions<OPT_x86_64LinkOptTable>(
    llvm::opt::InputArgList &args) const;
template bool GnuLdDriver::processTargetOptions<OPT_x86_64LinkOptTable>(
    llvm::opt::InputArgList &args);
template bool GnuLdDriver::createInputActions<OPT_x86_64LinkOptTable>(
    llvm::opt::InputArgList &Args, std::vector<eld::InputAction *> &actions);
template bool GnuLdDriver::overrideOptions<OPT_x86_64LinkOptTable>(
    llvm::opt::InputArgList &args);
template bool GnuLdDriver::doLink<OPT_x86_64LinkOptTable>(
    llvm::opt::InputArgList &Args, std::vector<eld::InputAction *> &actions);
template bool GnuLdDriver::handleReproduce<OPT_x86_64LinkOptTable>(
    llvm::opt::InputArgList &Args, std::vector<eld::InputAction *> &actions,
    bool);
#endif
