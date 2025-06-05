//===- x86_64LinkDriver.cpp------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#include "eld/Driver/x86_64LinkDriver.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace llvm::opt;

#define OPTTABLE_STR_TABLE_CODE
#include "eld/Driver/x86_64LinkerOptions.inc"
#undef OPTTABLE_STR_TABLE_CODE

#define OPTTABLE_PREFIXES_TABLE_CODE
#include "eld/Driver/x86_64LinkerOptions.inc"
#undef OPTTABLE_PREFIXES_TABLE_CODE

static constexpr llvm::opt::OptTable::Info infoTable[] = {
#define OPTION(PREFIXES_OFFSET, PREFIXED_NAME_OFFSET, ID, KIND, GROUP, ALIAS,  \
               ALIASARGS, FLAGS, VISIBILITY, PARAM, HELPTEXT,                  \
               HELPTEXTSFORVARIANTS, METAVAR, VALUES)                          \
  LLVM_CONSTRUCT_OPT_INFO(                                                     \
      PREFIXES_OFFSET, PREFIXED_NAME_OFFSET, x86_64LinkOptTable::ID, KIND,     \
      x86_64LinkOptTable::GROUP, x86_64LinkOptTable::ALIAS, ALIASARGS, FLAGS,  \
      VISIBILITY, PARAM, HELPTEXT, HELPTEXTSFORVARIANTS, METAVAR, VALUES),
#include "eld/Driver/x86_64LinkerOptions.inc"
#undef OPTION
};

OPT_x86_64LinkOptTable::OPT_x86_64LinkOptTable()
    : GenericOptTable(OptionStrTable, OptionPrefixesTable, infoTable) {}

x86_64LinkDriver *x86_64LinkDriver::Create(eld::LinkerConfig &C, Flavor F,
                                           std::string Triple) {
  return eld::make<x86_64LinkDriver>(C, F, Triple);
}

x86_64LinkDriver::x86_64LinkDriver(eld::LinkerConfig &C, Flavor F,
                                   std::string Triple)
    : GnuLdDriver(C, F) {
  Config.targets().setArch("x86_64");
}

opt::OptTable *
x86_64LinkDriver::parseOptions(ArrayRef<const char *> Args,
                               llvm::opt::InputArgList &ArgList) {
  OPT_x86_64LinkOptTable *Table = eld::make<OPT_x86_64LinkOptTable>();
  unsigned missingIndex;
  unsigned missingCount;
  ArgList = Table->ParseArgs(Args.slice(1), missingIndex, missingCount);
  if (missingCount) {
    Config.raise(eld::Diag::error_missing_arg_value)
        << ArgList.getArgString(missingIndex) << missingCount;
    return nullptr;
  }
  if (ArgList.hasArg(OPT_x86_64LinkOptTable::help)) {
    Table->printHelp(outs(), Args[0], "X86_64 Linker", false,
                     /*ShowAllAliases=*/true);
    return nullptr;
  }
  if (ArgList.hasArg(OPT_x86_64LinkOptTable::help_hidden)) {
    Table->printHelp(outs(), Args[0], "X86_64 Linker", true,
                     /*ShowAllAliases=*/true);
    return nullptr;
  }
  if (ArgList.hasArg(OPT_x86_64LinkOptTable::version)) {
    printVersionInfo();
    return nullptr;
  }
  // -repository-version
  if (ArgList.hasArg(OPT_x86_64LinkOptTable::repository_version)) {
    printRepositoryVersion();
    return nullptr;
  }

  return Table;
}

// Start the link step.
int x86_64LinkDriver::link(llvm::ArrayRef<const char *> Args,
                           llvm::ArrayRef<llvm::StringRef> ELDFlagsArgs) {
  std::vector<const char *> allArgs = getAllArgs(Args, ELDFlagsArgs);
  if (!ELDFlagsArgs.empty())
    Config.raise(eld::Diag::note_eld_flags_without_output_name)
        << llvm::join(ELDFlagsArgs, " ");
  llvm::opt::InputArgList ArgList(allArgs.data(),
                                  allArgs.data() + allArgs.size());
  Config.options().setArgs(Args);
  std::vector<eld::InputAction *> Action;

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
  {
    Table = parseOptions(allArgs, ArgList);
    if (ArgList.hasArg(OPT_x86_64LinkOptTable::help) ||
        ArgList.hasArg(OPT_x86_64LinkOptTable::help_hidden) ||
        ArgList.hasArg(OPT_x86_64LinkOptTable::version) ||
        ArgList.hasArg(OPT_x86_64LinkOptTable::repository_version)) {
      return LINK_SUCCESS;
    }
    if (!Table)
      return LINK_FAIL;
    if (!processLLVMOptions<OPT_x86_64LinkOptTable>(ArgList))
      return LINK_FAIL;
    if (!processTargetOptions<OPT_x86_64LinkOptTable>(ArgList))
      return LINK_FAIL;
    if (!processOptions<OPT_x86_64LinkOptTable>(ArgList))
      return LINK_FAIL;
    if (!checkOptions<OPT_x86_64LinkOptTable>(ArgList))
      return LINK_FAIL;
    if (!overrideOptions<OPT_x86_64LinkOptTable>(ArgList))
      return LINK_FAIL;
    if (!createInputActions<OPT_x86_64LinkOptTable>(ArgList, Action))
      return LINK_FAIL;
  }
  if (!doLink<OPT_x86_64LinkOptTable>(ArgList, Action))
    return LINK_FAIL;
  return LINK_SUCCESS;
}

template <class T>
bool x86_64LinkDriver::checkOptions(llvm::opt::InputArgList &Args) {
  return GnuLdDriver::checkOptions<T>(Args);
}

template <class T>
bool x86_64LinkDriver::processOptions(llvm::opt::InputArgList &Args) {
  return GnuLdDriver::processOptions<T>(Args);
}

template <class T>
bool x86_64LinkDriver::createInputActions(
    llvm::opt::InputArgList &Args, std::vector<eld::InputAction *> &actions) {
  return GnuLdDriver::createInputActions<T>(Args, actions);
}

template <class T>
bool x86_64LinkDriver::processTargetOptions(llvm::opt::InputArgList &Args) {
  return GnuLdDriver::processTargetOptions<T>(Args);
}

template <class T>
bool x86_64LinkDriver::processLLVMOptions(llvm::opt::InputArgList &Args) {
  return GnuLdDriver::processLLVMOptions<T>(Args);
}


bool x86_64LinkDriver::isValidEmulation(llvm::StringRef Emulation){
  return Emulation == "elf_x86_64" || Emulation == "elf_amd64";
}