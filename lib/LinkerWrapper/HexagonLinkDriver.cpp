//===- HexagonLinkDriver.cpp-----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Driver/HexagonLinkDriver.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Support/Memory.h"
#include "eld/Support/MsgHandling.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace llvm::opt;

#define OPTTABLE_STR_TABLE_CODE
#include "eld/Driver/HexagonLinkerOptions.inc"
#undef OPTTABLE_STR_TABLE_CODE

#define OPTTABLE_PREFIXES_TABLE_CODE
#include "eld/Driver/HexagonLinkerOptions.inc"
#undef OPTTABLE_PREFIXES_TABLE_CODE

static constexpr llvm::opt::OptTable::Info infoTable[] = {
#define OPTION(PREFIXES_OFFSET, PREFIXED_NAME_OFFSET, ID, KIND, GROUP, ALIAS,  \
               ALIASARGS, FLAGS, VISIBILITY, PARAM, HELPTEXT,                  \
               HELPTEXTSFORVARIANTS, METAVAR, VALUES, SUBCOMMANDIDS_OFFSET)                          \
  LLVM_CONSTRUCT_OPT_INFO(                                                     \
      PREFIXES_OFFSET, PREFIXED_NAME_OFFSET, HexagonLinkOptTable::ID, KIND,    \
      HexagonLinkOptTable::GROUP, HexagonLinkOptTable::ALIAS, ALIASARGS,       \
      FLAGS, VISIBILITY, PARAM, HELPTEXT, HELPTEXTSFORVARIANTS, METAVAR,       \
      VALUES, SUBCOMMANDIDS_OFFSET),
#include "eld/Driver/HexagonLinkerOptions.inc"
#undef OPTION
};

OPT_HexagonLinkOptTable::OPT_HexagonLinkOptTable()
    : GenericOptTable(OptionStrTable, OptionPrefixesTable, infoTable) {}

HexagonLinkDriver *HexagonLinkDriver::Create(eld::LinkerConfig &C,

                                             std::string InferredArch) {
  return eld::make<HexagonLinkDriver>(C, InferredArch);
}

HexagonLinkDriver::HexagonLinkDriver(eld::LinkerConfig &C,
                                     std::string InferredArch)
    : GnuLdDriver(C, DriverFlavor::Hexagon) {
  Config.targets().setArch(InferredArch);
}

HexagonLinkDriver *HexagonLinkDriver::Create(eld::LinkerConfig &C,
                                             bool is64bit) {
  return eld::make<HexagonLinkDriver>(C, is64bit);
}

HexagonLinkDriver::HexagonLinkDriver(eld::LinkerConfig &C, bool is64bit)
    : GnuLdDriver(C, DriverFlavor::Hexagon) {
  Config.targets().setArch("hexagon");
}

std::optional<int>
HexagonLinkDriver::parseOptions(ArrayRef<const char *> Args,
                                llvm::opt::InputArgList &ArgList) {
  Table = eld::make<OPT_HexagonLinkOptTable>();
  unsigned missingIndex;
  unsigned missingCount;
  ArgList = Table->ParseArgs(Args.slice(1), missingIndex, missingCount);
  if (missingCount) {
    Config.raise(eld::Diag::error_missing_arg_value)
        << ArgList.getArgString(missingIndex) << missingCount;
    return LINK_FAIL;
  }
  if (ArgList.hasArg(OPT_HexagonLinkOptTable::help)) {
    Table->printHelp(outs(), Args[0], "Hexagon Linker", false,
                     /*ShowAllAliases=*/true);
    return LINK_SUCCESS;
  }
  if (ArgList.hasArg(OPT_HexagonLinkOptTable::help_hidden)) {
    Table->printHelp(outs(), Args[0], "Hexagon Linker", true,
                     /*ShowAllAliases=*/true);
    return LINK_SUCCESS;
  }
  if (ArgList.hasArg(OPT_HexagonLinkOptTable::version)) {
    printVersionInfo();
    return LINK_SUCCESS;
  }
  // --about
  if (ArgList.hasArg(OPT_HexagonLinkOptTable::about)) {
    printAboutInfo();
    return LINK_SUCCESS;
  }
  // -repository-version
  if (ArgList.hasArg(OPT_HexagonLinkOptTable::repository_version)) {
    printRepositoryVersion();
    return LINK_SUCCESS;
  }

  // --gpsize
  Config.options().setGPSize(
      getInteger(ArgList, OPT_HexagonLinkOptTable::gpsize, 8));

  // --disable-guard-for-weak-undefs
  if (ArgList.hasArg(OPT_HexagonLinkOptTable::disable_guard_for_weak_undef))
    Config.options().setDisableGuardForWeakUndefs();

  // --relax
  if (ArgList.hasArg(OPT_HexagonLinkOptTable::relax))
    Config.options().enableRelaxation();

  // --relax=<regex>
  for (auto *arg : ArgList.filtered(OPT_HexagonLinkOptTable::relax_value)) {
    // Enable relaxation when a pattern is provided.
    Config.options().enableRelaxation();
    Config.options().addRelaxSection(arg->getValue());
  }

  Config.options().setUnknownOptions(
      ArgList.getAllArgValues(OPT_HexagonLinkOptTable::UNKNOWN));
  return {};
}

bool HexagonLinkDriver::processLTOOptions(
    llvm::lto::Config &Conf, std::vector<std::string> &LLVMOptions) {
  return GnuLdDriver::processLTOOptions<OPT_HexagonLinkOptTable>(Conf,
                                                                 LLVMOptions);
}

// Start the link step.
int HexagonLinkDriver::link(llvm::ArrayRef<const char *> Args,
                            llvm::ArrayRef<llvm::StringRef> ELDFlagsArgs) {
  std::vector<const char *> allArgs = getAllArgs(Args, ELDFlagsArgs);
  if (!ELDFlagsArgs.empty())
    Config.raise(eld::Diag::note_eld_flags_without_output_name)
        << llvm::join(ELDFlagsArgs, " ");

  Config.options().setArgs(allArgs);

  //===--------------------------------------------------------------------===//
  // Special functions.
  //===--------------------------------------------------------------------===//
  // FIXME: Refactor this code to a common-place.
  static int StaticSymbol;
  std::string lfile =
      llvm::sys::fs::getMainExecutable(allArgs[0], &StaticSymbol);
  SmallString<128> lpath(lfile);
  llvm::sys::path::remove_filename(lpath);
  Config.options().setLinkerPath(std::string(lpath));
  //===--------------------------------------------------------------------===//
  // Begin Link preprocessing
  //===--------------------------------------------------------------------===//
  llvm::opt::InputArgList ArgListLocal;
  if (auto Ret = parseOptions(allArgs, ArgListLocal))
    return *Ret;

  // Save parsed options so they can be accessed later as needed. Right now it's
  // only used for LTO options, but can be expanded to track unused aguments.
  auto &ArgList = Config.options().setParsedArgs(std::move(ArgListLocal));
  if (!processLLVMOptions<OPT_HexagonLinkOptTable>(ArgList))
    return LINK_FAIL;
  if (!processTargetOptions<OPT_HexagonLinkOptTable>(ArgList))
    return LINK_FAIL;
  if (!processOptions<OPT_HexagonLinkOptTable>(ArgList))
    return LINK_FAIL;
  if (!checkOptions<OPT_HexagonLinkOptTable>(ArgList))
    return LINK_FAIL;

  if (!ELDFlagsArgs.empty())
    Config.raise(eld::Diag::note_eld_flags)
        << Config.options().outputFileName() << llvm::join(ELDFlagsArgs, " ");

  if (!overrideOptions<OPT_HexagonLinkOptTable>(ArgList))
    return LINK_FAIL;
  std::vector<eld::InputAction *> Action;
  if (!createInputActions<OPT_HexagonLinkOptTable>(ArgList, Action))
    return LINK_FAIL;
  if (!doLink<OPT_HexagonLinkOptTable>(ArgList, Action))
    return LINK_FAIL;
  return LINK_SUCCESS;
}

// Some command line options or some combinations of them are not allowed.
// This function checks for such errors.
template <class T>
bool HexagonLinkDriver::checkOptions(llvm::opt::InputArgList &Args) {
  return GnuLdDriver::checkOptions<T>(Args);
}

template <class T>
bool HexagonLinkDriver::processOptions(llvm::opt::InputArgList &Args) {
  return GnuLdDriver::processOptions<T>(Args);
}

template <class T>
bool HexagonLinkDriver::createInputActions(
    llvm::opt::InputArgList &Args, std::vector<eld::InputAction *> &actions) {
  return GnuLdDriver::createInputActions<T>(Args, actions);
}

template <class T>
bool HexagonLinkDriver::processTargetOptions(llvm::opt::InputArgList &Args) {
  return GnuLdDriver::processTargetOptions<T>(Args);
}

template <class T>
bool HexagonLinkDriver::processLLVMOptions(llvm::opt::InputArgList &Args) {
  return GnuLdDriver::processLLVMOptions<T>(Args);
}

bool HexagonLinkDriver::isValidEmulation(llvm::StringRef Emulation) {
  return llvm::StringSwitch<bool>(Emulation)
      .Cases({"hexagonelf", "v68", "v69", "v71", "v71t"}, true)
      .Cases({"v73", "v75", "v77", "v79", "v81", "v83", "v85", "v87", "v89",
              "v91"},
             true)
      .Default(false);
}
