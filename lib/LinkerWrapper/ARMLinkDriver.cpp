//===- ARMLinkDriver.cpp---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Driver/ARMLinkDriver.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Support/MsgHandling.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/raw_ostream.h"
#include <optional>

using namespace llvm;
using namespace llvm::opt;
using namespace eld;

#define OPTTABLE_STR_TABLE_CODE
#include "eld/Driver/ARMLinkerOptions.inc"
#undef OPTTABLE_STR_TABLE_CODE

#define OPTTABLE_PREFIXES_TABLE_CODE
#include "eld/Driver/ARMLinkerOptions.inc"
#undef OPTTABLE_PREFIXES_TABLE_CODE

static constexpr llvm::opt::OptTable::Info infoTable[] = {
#define OPTION(PREFIXES_OFFSET, PREFIXED_NAME_OFFSET, ID, KIND, GROUP, ALIAS,  \
               ALIASARGS, FLAGS, VISIBILITY, PARAM, HELPTEXT,                  \
               HELPTEXTSFORVARIANTS, METAVAR, VALUES, SUBCOMMANDIDS_OFFSET)                          \
  LLVM_CONSTRUCT_OPT_INFO(                                                     \
      PREFIXES_OFFSET, PREFIXED_NAME_OFFSET, ARMLinkOptTable::ID, KIND,        \
      ARMLinkOptTable::GROUP, ARMLinkOptTable::ALIAS, ALIASARGS, FLAGS,        \
      VISIBILITY, PARAM, HELPTEXT, HELPTEXTSFORVARIANTS, METAVAR, VALUES, SUBCOMMANDIDS_OFFSET),
#include "eld/Driver/ARMLinkerOptions.inc"
#undef OPTION
};

std::optional<Triple>
ARMLinkDriver::ParseEmulation(std::string pEmulation,
                              DiagnosticEngine *DiagEngine) {
  // armelf_linux_androideabi -- used for android emulation.
  std::optional<Triple> result =
      StringSwitch<std::optional<Triple>>(pEmulation)
          .Case("aarch64linux", Triple("aarch64", "", "linux", "gnu"))
          .Case("aarch64linux_androideabi",
                Triple("aarch64", "", "linux", "androideabi"))
          .Case("armelf_linux_eabi", Triple("arm", "", "linux", "gnueabi"))
          .Case("armelf_linux_androideabi",
                Triple("arm", "", "linux", "androideabi"))
          .Case("armelf", Triple("arm", "", "", ""))
          .Case("aarch64elf", Triple("aarch64", "", "", ""))
          .Default(std::nullopt);
  return result;
}

OPT_ARMLinkOptTable::OPT_ARMLinkOptTable()
    : GenericOptTable(OptionStrTable, OptionPrefixesTable, infoTable) {}

ARMLinkDriver *ARMLinkDriver::Create(eld::LinkerConfig &C,
                                     std::string InferredArch) {
  return eld::make<ARMLinkDriver>(C, InferredArch);
}

ARMLinkDriver::ARMLinkDriver(eld::LinkerConfig &C, std::string InferredArch)
    : GnuLdDriver(C, DriverFlavor::ARM_AArch64) {
  Config.targets().setArch(InferredArch);
}

ARMLinkDriver *ARMLinkDriver::Create(eld::LinkerConfig &C, bool is64bit) {
  return eld::make<ARMLinkDriver>(C, is64bit);
}

ARMLinkDriver::ARMLinkDriver(eld::LinkerConfig &C, bool is64bit)
    : GnuLdDriver(C, DriverFlavor::ARM_AArch64) {
  if (!is64bit)
    Config.targets().setArch("arm");
  else
    Config.targets().setArch("aarch64");
}

std::optional<int>
ARMLinkDriver::parseOptions(ArrayRef<const char *> Args,
                            llvm::opt::InputArgList &ArgList) {
  Table = eld::make<OPT_ARMLinkOptTable>();
  unsigned missingIndex;
  unsigned missingCount;
  ArgList = Table->ParseArgs(Args.slice(1), missingIndex, missingCount);
  if (missingCount) {
    Config.raise(eld::Diag::error_missing_arg_value)
        << ArgList.getArgString(missingIndex) << missingCount;
    return LINK_FAIL;
  }
  if (ArgList.hasArg(OPT_ARMLinkOptTable::help)) {
    Table->printHelp(outs(), Args[0], "ARM Linker", false,
                     /*ShowAllAliases=*/true);
    return LINK_SUCCESS;
  }
  if (ArgList.hasArg(OPT_ARMLinkOptTable::help_hidden)) {
    Table->printHelp(outs(), Args[0], "ARM Linker", true,
                     /*ShowAllAliases=*/true);
    return LINK_SUCCESS;
  }
  if (ArgList.hasArg(OPT_ARMLinkOptTable::version)) {
    printVersionInfo();
    return LINK_SUCCESS;
  }
  // --about
  if (ArgList.hasArg(OPT_ARMLinkOptTable::about)) {
    printAboutInfo();
    return LINK_SUCCESS;
  }
  // -repository-version
  if (ArgList.hasArg(OPT_ARMLinkOptTable::repository_version)) {
    printRepositoryVersion();
    return LINK_SUCCESS;
  }
  // --disable-bss-mixing
  if (ArgList.hasArg(OPT_ARMLinkOptTable::enable_bss_mixing))
    Config.options().setAllowBSSMixing(true);
  else
    Config.options().setAllowBSSMixing(false);

  // --disable-bss-convesion
  if (ArgList.hasArg(OPT_ARMLinkOptTable::disable_bss_conversion))
    Config.options().setAllowBSSConversion(false);
  else
    Config.options().setAllowBSSConversion(true);

  // --fix-cortex-a53-843419
  if (ArgList.hasArg(OPT_ARMLinkOptTable::fix_cortex_a53_843419))
    Config.options().setFixCortexA53Errata843419();

  // --use-mov-veneer
  if (ArgList.hasArg(OPT_ARMLinkOptTable::use_mov_veneer))
    Config.options().setUseMovVeneer(true);

  // --compact
  if (ArgList.hasArg(OPT_ARMLinkOptTable::compact))
    Config.options().setCompact(true);

  // -frwpi
  if (ArgList.hasArg(OPT_ARMLinkOptTable::frwpi))
    Config.options().setRWPI();

  // -fropi
  if (ArgList.hasArg(OPT_ARMLinkOptTable::fropi))
    Config.options().setROPI();

  if (ArgList.hasArg(OPT_ARMLinkOptTable::execute_only)) {
    Config.options().setExecuteOnlySegments();
    Config.options().setROSegment(true);
  }

  // -target2
  if (llvm::opt::Arg *arg = ArgList.getLastArg(OPT_ARMLinkOptTable::target2)) {
    llvm::StringRef Value = arg->getValue();
    if (auto ValueOpt =
            llvm::StringSwitch<std::optional<GeneralOptions::Target2Policy>>(
                Value)
                .Case("rel", GeneralOptions::Target2Policy::Rel)
                .Case("abs", GeneralOptions::Target2Policy::Abs)
                .Case("got-rel", GeneralOptions::Target2Policy::GotRel)
                .Default(std::nullopt)) {
      Config.options().setTarget2Policy(*ValueOpt);
    } else
      Config.raise(eld::Diag::error_invalid_target2) << Value;
  }

  Config.options().setUnknownOptions(
      ArgList.getAllArgValues(OPT_ARMLinkOptTable::UNKNOWN));

  return {};
}

bool ARMLinkDriver::processLTOOptions(llvm::lto::Config &Conf,
                                      std::vector<std::string> &LLVMOptions) {
  return GnuLdDriver::processLTOOptions<OPT_ARMLinkOptTable>(Conf, LLVMOptions);
}

// Start the link step.
int ARMLinkDriver::link(llvm::ArrayRef<const char *> Args,
                        llvm::ArrayRef<llvm::StringRef> ELDFlagsArgs) {
  std::vector<const char *> allArgs = getAllArgs(Args, ELDFlagsArgs);
  if (!ELDFlagsArgs.empty())
    Config.raise(eld::Diag::note_eld_flags_without_output_name)
        << llvm::join(ELDFlagsArgs, " ");
  Config.options().setArgs(allArgs);

  //===--------------------------------------------------------------------===//
  // Special functions.
  //===--------------------------------------------------------------------===//
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
  if (!processLLVMOptions<OPT_ARMLinkOptTable>(ArgList))
    return LINK_FAIL;
  if (!processTargetOptions<OPT_ARMLinkOptTable>(ArgList))
    return LINK_FAIL;
  if (!processOptions<OPT_ARMLinkOptTable>(ArgList))
    return LINK_FAIL;
  if (!checkOptions<OPT_ARMLinkOptTable>(ArgList))
    return LINK_FAIL;

  if (!ELDFlagsArgs.empty())
    Config.raise(eld::Diag::note_eld_flags)
        << Config.options().outputFileName() << llvm::join(ELDFlagsArgs, " ");

  if (!overrideOptions<OPT_ARMLinkOptTable>(ArgList))
    return LINK_FAIL;
  std::vector<eld::InputAction *> Action;
  if (!createInputActions<OPT_ARMLinkOptTable>(ArgList, Action))
    return LINK_FAIL;
  if (!doLink<OPT_ARMLinkOptTable>(ArgList, Action))
    return LINK_FAIL;
  return LINK_SUCCESS;
}

// Some command line options or some combinations of them are not allowed.
// This function checks for such errors.
template <class T>
bool ARMLinkDriver::checkOptions(llvm::opt::InputArgList &Args) {
  return GnuLdDriver::checkOptions<T>(Args);
}

template <class T>
bool ARMLinkDriver::processOptions(llvm::opt::InputArgList &Args) {
  return GnuLdDriver::processOptions<T>(Args);
}

template <class T>
bool ARMLinkDriver::createInputActions(
    llvm::opt::InputArgList &Args, std::vector<eld::InputAction *> &actions) {
  return GnuLdDriver::createInputActions<T>(Args, actions);
}

template <class T>
bool ARMLinkDriver::processTargetOptions(llvm::opt::InputArgList &Args) {
  bool result = GnuLdDriver::processTargetOptions<T>(Args);

  std::string emulation = Config.options().getEmulation().str();
  std::string arch = Config.targets().getArch();
  // If a specific emulation was requested, apply it now.
  if (!emulation.empty()) {
    llvm::Triple TheTriple = Config.targets().triple();
    std::optional<Triple> OptEmulationTriple =
        ParseEmulation(emulation, Config.getDiagEngine());
    // Report invalid emulation error for unknown emulation.
    if (!OptEmulationTriple) {
      DiagEngine->raise(Diag::err_invalid_emulation) << emulation << "\n";
      return false;
    }
    Triple EmulationTriple = OptEmulationTriple.value();
    if (EmulationTriple.getArch() != Triple::UnknownArch)
      TheTriple.setArch(EmulationTriple.getArch());
    if (EmulationTriple.getOS() != Triple::OSType::UnknownOS)
      TheTriple.setOS(EmulationTriple.getOS());
    if (EmulationTriple.getEnvironment() != Triple::UnknownEnvironment)
      TheTriple.setEnvironment(EmulationTriple.getEnvironment());
    Config.targets().setTriple(TheTriple);
  }
  return result;
}

template <class T>
bool ARMLinkDriver::processLLVMOptions(llvm::opt::InputArgList &Args) {
  llvm::Triple triple;
  if (llvm::opt::Arg *arg = Args.getLastArg(T::mtriple))
    triple.setTriple(arg->getValue());
  else
    triple.setTriple(llvm::sys::getDefaultTargetTriple());

  // Parse and evaluate -mllvm options.
  std::vector<const char *> V;
  V.push_back("eld (LLVM option parsing)");
  for (auto *Arg : Args.filtered(T::mllvm))
    V.push_back(Arg->getValue());

  cl::ParseCommandLineOptions(V.size(), V.data());
  return true;
}
