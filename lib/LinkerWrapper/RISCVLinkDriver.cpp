//===- RISCVLinkDriver.cpp-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Driver/RISCVLinkDriver.h"
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

using namespace llvm;
using namespace llvm::opt;
using namespace eld;

#define OPTTABLE_STR_TABLE_CODE
#include "eld/Driver/RISCVLinkerOptions.inc"
#undef OPTTABLE_STR_TABLE_CODE

#define OPTTABLE_PREFIXES_TABLE_CODE
#include "eld/Driver/RISCVLinkerOptions.inc"
#undef OPTTABLE_PREFIXES_TABLE_CODE

static constexpr llvm::opt::OptTable::Info infoTable[] = {
#define OPTION(PREFIXES_OFFSET, PREFIXED_NAME_OFFSET, ID, KIND, GROUP, ALIAS,  \
               ALIASARGS, FLAGS, VISIBILITY, PARAM, HELPTEXT,                  \
               HELPTEXTSFORVARIANTS, METAVAR, VALUES, SUBCOMMANDIDS_OFFSET)                          \
  LLVM_CONSTRUCT_OPT_INFO(                                                     \
      PREFIXES_OFFSET, PREFIXED_NAME_OFFSET, RISCVLinkOptTable::ID, KIND,      \
      RISCVLinkOptTable::GROUP, RISCVLinkOptTable::ALIAS, ALIASARGS, FLAGS,    \
      VISIBILITY, PARAM, HELPTEXT, HELPTEXTSFORVARIANTS, METAVAR, VALUES, SUBCOMMANDIDS_OFFSET),
#include "eld/Driver/RISCVLinkerOptions.inc"
#undef OPTION
};

static Triple ParseEmulation(std::string pEmulation, Triple &triple,
                             DiagnosticEngine *DiagEngine) {
  Triple result = StringSwitch<Triple>(pEmulation)
                      .Case("elf32lriscv", Triple("riscv32", "", "", ""))
                      .Case("elf64lriscv", Triple("riscv64", "", "", ""))
                      .Default(Triple("unknown", "", "", ""));
  // Report invalid emulation error for unknown emulation.
  if (result.getArchName() == "unknown")
    DiagEngine->raise(Diag::err_invalid_emulation) << pEmulation << "\n";
  return result;
}

OPT_RISCVLinkOptTable::OPT_RISCVLinkOptTable()
    : GenericOptTable(OptionStrTable, OptionPrefixesTable, infoTable) {}

RISCVLinkDriver *RISCVLinkDriver::Create(eld::LinkerConfig &C,
                                         std::string InferredArch) {
  return eld::make<RISCVLinkDriver>(C, InferredArch);
}

RISCVLinkDriver::RISCVLinkDriver(eld::LinkerConfig &C, std::string InferredArch)
    : GnuLdDriver(C, DriverFlavor::RISCV32_RISCV64) {
  Config.targets().setArch(InferredArch);
}

RISCVLinkDriver *RISCVLinkDriver::Create(eld::LinkerConfig &C, bool is64bit) {
  return eld::make<RISCVLinkDriver>(C, is64bit);
}

RISCVLinkDriver::RISCVLinkDriver(eld::LinkerConfig &C, bool is64bit)
    : GnuLdDriver(C, DriverFlavor::RISCV32_RISCV64) {
  if (!is64bit)
    Config.targets().setArch("riscv32");
  else
    Config.targets().setArch("riscv64");
}

std::optional<int>
RISCVLinkDriver::parseOptions(ArrayRef<const char *> Args,
                              llvm::opt::InputArgList &ArgList) {
  Table = eld::make<OPT_RISCVLinkOptTable>();
  unsigned missingIndex;
  unsigned missingCount;
  ArgList = Table->ParseArgs(Args.slice(1), missingIndex, missingCount);
  if (missingCount) {
    Config.raise(eld::Diag::error_missing_arg_value)
        << ArgList.getArgString(missingIndex) << missingCount;
    return LINK_FAIL;
  }
  if (ArgList.hasArg(OPT_RISCVLinkOptTable::help)) {
    Table->printHelp(outs(), Args[0], "RISCV Linker", false,
                     /*ShowAllAliases=*/true);
    return LINK_SUCCESS;
  }
  if (ArgList.hasArg(OPT_RISCVLinkOptTable::help_hidden)) {
    Table->printHelp(outs(), Args[0], "RISCV Linker", true,
                     /*ShowAllAliases=*/true);
    return LINK_SUCCESS;
  }
  if (ArgList.hasArg(OPT_RISCVLinkOptTable::version)) {
    printVersionInfo();
    return LINK_SUCCESS;
  }
  // --about
  if (ArgList.hasArg(OPT_RISCVLinkOptTable::about)) {
    printAboutInfo();
    return LINK_SUCCESS;
  }
  // -repository-version
  if (ArgList.hasArg(OPT_RISCVLinkOptTable::repository_version)) {
    printRepositoryVersion();
    return LINK_SUCCESS;
  }

  // --no-relax
  if (ArgList.hasArg(OPT_RISCVLinkOptTable::no_riscv_relax))
    Config.options().setRISCVRelax(false);

  // --no-relax-zero
  if (ArgList.hasArg(OPT_RISCVLinkOptTable::no_relax_zero))
    Config.options().setRISCVZeroRelax(false);

  // --no-relax-gp
  if (ArgList.hasArg(OPT_RISCVLinkOptTable::no_relax_gp))
    Config.options().setRISCVGPRelax(false);

  // --no-relax-c
  if (ArgList.hasArg(OPT_RISCVLinkOptTable::no_riscv_relax_compressed))
    Config.options().setRISCVRelaxToC(false);

  // --relax-xqci, --no-relax-xqci  (default)
  Config.options().setRISCVRelaxXqci(
      ArgList.hasFlag(OPT_RISCVLinkOptTable::riscv_relax_xqci,
                      OPT_RISCVLinkOptTable::no_riscv_relax_xqci, false));

  // --enable-bss-mixing
  if (ArgList.hasArg(OPT_RISCVLinkOptTable::enable_bss_mixing))
    Config.options().setAllowBSSMixing(true);
  else
    Config.options().setAllowBSSMixing(false);

  // --disable-bss-conversion
  if (ArgList.hasArg(OPT_RISCVLinkOptTable::disable_bss_conversion))
    Config.options().setAllowBSSConversion(false);
  else
    Config.options().setAllowBSSConversion(true);

  // --keep-labels
  if (ArgList.hasArg(OPT_RISCVLinkOptTable::keep_labels))
    Config.options().setKeepLabels();

  // --patch-enable
  if (ArgList.getLastArg(OPT_RISCVLinkOptTable::patch_enable))
    Config.options().setPatchEnable();

  // --patch-base
  if (llvm::opt::Arg *arg =
          ArgList.getLastArg(OPT_RISCVLinkOptTable::patch_base))
    Config.options().setPatchBase(arg->getValue());

  if (Config.options().isPatchEnable()) {
    if (Config.options().getStripSymbolMode() ==
        GeneralOptions::StripAllSymbols)
      Config.raise(Diag::warn_strip_symbols) << "--patch-enable";
    Config.options().setStripSymbols(eld::GeneralOptions::StripLocals);
  }

  Config.options().setUnknownOptions(
      ArgList.getAllArgValues(OPT_RISCVLinkOptTable::UNKNOWN));

  return {};
}

bool RISCVLinkDriver::processLTOOptions(llvm::lto::Config &Conf,
                                        std::vector<std::string> &LLVMOptions) {
  return GnuLdDriver::processLTOOptions<OPT_RISCVLinkOptTable>(Conf,
                                                               LLVMOptions);
}

// Start the link step.
int RISCVLinkDriver::link(llvm::ArrayRef<const char *> Args,
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
  if (!processLLVMOptions<OPT_RISCVLinkOptTable>(ArgList))
    return LINK_FAIL;
  if (!processTargetOptions<OPT_RISCVLinkOptTable>(ArgList))
    return LINK_FAIL;
  if (!processOptions<OPT_RISCVLinkOptTable>(ArgList))
    return LINK_FAIL;

  if (!ELDFlagsArgs.empty())
    Config.raise(eld::Diag::note_eld_flags)
        << Config.options().outputFileName() << llvm::join(ELDFlagsArgs, " ");

  if (!checkOptions<OPT_RISCVLinkOptTable>(ArgList))
    return LINK_FAIL;
  if (!overrideOptions<OPT_RISCVLinkOptTable>(ArgList))
    return LINK_FAIL;
  std::vector<eld::InputAction *> Action;
  if (!createInputActions<OPT_RISCVLinkOptTable>(ArgList, Action))
    return LINK_FAIL;
  if (!doLink<OPT_RISCVLinkOptTable>(ArgList, Action))
    return LINK_FAIL;
  return LINK_SUCCESS;
}

// Some command line options or some combinations of them are not allowed.
// This function checks for such errors.
template <class T>
bool RISCVLinkDriver::checkOptions(llvm::opt::InputArgList &Args) {
  return GnuLdDriver::checkOptions<T>(Args);
}

template <class T>
bool RISCVLinkDriver::processOptions(llvm::opt::InputArgList &Args) {
  if (!GnuLdDriver::processOptions<T>(Args))
    return false;
  // FIXME : remove duplicate code
  if (Config.options().isPatchEnable()) {
    if (Config.options().getStripSymbolMode() ==
        GeneralOptions::StripAllSymbols)
      Config.raise(Diag::warn_strip_symbols) << "--patch-enable";
    Config.options().setStripSymbols(eld::GeneralOptions::StripLocals);
  }
  return true;
}

template <class T>
bool RISCVLinkDriver::createInputActions(
    llvm::opt::InputArgList &Args, std::vector<eld::InputAction *> &actions) {
  return GnuLdDriver::createInputActions<T>(Args, actions);
}

template <class T>
bool RISCVLinkDriver::processTargetOptions(llvm::opt::InputArgList &Args) {
  bool result = GnuLdDriver::processTargetOptions<T>(Args);
  std::string emulation = Config.options().getEmulation().str();
  // If a specific emulation was requested, apply it now.
  if (!emulation.empty()) {
    llvm::Triple TheTriple = Config.targets().triple();
    Triple EmulationTriple =
        ParseEmulation(emulation, TheTriple, Config.getDiagEngine());
    if (EmulationTriple.getArch() != Triple::UnknownArch) {
      if (EmulationTriple.getArch() == Triple::riscv32)
        Config.targets().setArch("riscv32");
      if (EmulationTriple.getArch() == Triple::riscv64)
        Config.targets().setArch("riscv64");
      TheTriple.setArch(EmulationTriple.getArch());
    }
    if (EmulationTriple.getOS() != Triple::UnknownOS)
      TheTriple.setOS(EmulationTriple.getOS());
    if (EmulationTriple.getEnvironment() != Triple::UnknownEnvironment)
      TheTriple.setEnvironment(EmulationTriple.getEnvironment());
    Config.targets().setTriple(TheTriple);
  }
  return result;
}

template <class T>
bool RISCVLinkDriver::processLLVMOptions(llvm::opt::InputArgList &Args) {
  return GnuLdDriver::processLLVMOptions<T>(Args);
}
