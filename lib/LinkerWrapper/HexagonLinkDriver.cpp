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
               HELPTEXTSFORVARIANTS, METAVAR, VALUES)                          \
  LLVM_CONSTRUCT_OPT_INFO(                                                     \
      PREFIXES_OFFSET, PREFIXED_NAME_OFFSET, HexagonLinkOptTable::ID, KIND,    \
      HexagonLinkOptTable::GROUP, HexagonLinkOptTable::ALIAS, ALIASARGS,       \
      FLAGS, VISIBILITY, PARAM, HELPTEXT, HELPTEXTSFORVARIANTS, METAVAR,       \
      VALUES),
#include "eld/Driver/HexagonLinkerOptions.inc"
#undef OPTION
};

OPT_HexagonLinkOptTable::OPT_HexagonLinkOptTable()
    : GenericOptTable(OptionStrTable, OptionPrefixesTable, infoTable) {}

HexagonLinkDriver *HexagonLinkDriver::Create(Flavor F, std::string Triple) {
  return eld::make<HexagonLinkDriver>(F, Triple);
}

HexagonLinkDriver::HexagonLinkDriver(Flavor F, std::string Triple)
    : GnuLdDriver(F) {
  m_Config.targets().setArch("hexagon");

  if (!Triple.empty())
    m_Config.targets().setTriple(Triple);
}

opt::OptTable *
HexagonLinkDriver::parseOptions(ArrayRef<const char *> Args,
                                llvm::opt::InputArgList &ArgList) {
  OPT_HexagonLinkOptTable *Table = eld::make<OPT_HexagonLinkOptTable>();
  unsigned missingIndex;
  unsigned missingCount;
  ArgList = Table->ParseArgs(Args.slice(1), missingIndex, missingCount);
  if (missingCount) {
    m_Config.raise(eld::diag::error_missing_arg_value)
        << ArgList.getArgString(missingIndex) << missingCount;
    return nullptr;
  }
  if (ArgList.hasArg(OPT_HexagonLinkOptTable::help)) {
    Table->printHelp(outs(), Args[0], "Hexagon Linker", false,
                     /*ShowAllAliases=*/true);
    return nullptr;
  }
  if (ArgList.hasArg(OPT_HexagonLinkOptTable::help_hidden)) {
    Table->printHelp(outs(), Args[0], "Hexagon Linker", true,
                     /*ShowAllAliases=*/true);
    return nullptr;
  }
  if (ArgList.hasArg(OPT_HexagonLinkOptTable::version)) {
    printVersionInfo();
    return nullptr;
  }
  // -repository-version
  if (ArgList.hasArg(OPT_HexagonLinkOptTable::repository_version)) {
    printRepositoryVersion();
    return nullptr;
  }
  return Table;
}

// Start the link step.
int HexagonLinkDriver::link(llvm::ArrayRef<const char *> Args,
                            llvm::ArrayRef<llvm::StringRef> ELDFlagsArgs) {
  std::vector<const char *> allArgs = getAllArgs(Args, ELDFlagsArgs);
  if (!ELDFlagsArgs.empty())
    m_Config.raise(eld::diag::note_eld_flags_without_output_name)
        << llvm::join(ELDFlagsArgs, " ");

  llvm::opt::InputArgList ArgList(allArgs.data(),
                                  allArgs.data() + allArgs.size());
  m_Config.options().setArgs(Args);

  std::vector<eld::InputAction *> Action;

  //===--------------------------------------------------------------------===//
  // Special functions.
  //===--------------------------------------------------------------------===//
  // FIXME: Refactor this code to a common-place.
  static int StaticSymbol;
  std::string lfile =
      llvm::sys::fs::getMainExecutable(allArgs[0], &StaticSymbol);
  SmallString<128> lpath(lfile);
  llvm::sys::path::remove_filename(lpath);
  m_Config.options().setLinkerPath(std::string(lpath));
  //===--------------------------------------------------------------------===//
  // Begin Link preprocessing
  //===--------------------------------------------------------------------===//
  {
    Table = parseOptions(allArgs, ArgList);
    if (ArgList.hasArg(OPT_HexagonLinkOptTable::help) ||
        ArgList.hasArg(OPT_HexagonLinkOptTable::help_hidden) ||
        ArgList.hasArg(OPT_HexagonLinkOptTable::version) ||
        ArgList.hasArg(OPT_HexagonLinkOptTable::repository_version)) {
      return LINK_SUCCESS;
    }
    if (!Table)
      return LINK_FAIL;
    if (!processLLVMOptions<OPT_HexagonLinkOptTable>(ArgList))
      return LINK_FAIL;
    if (!processTargetOptions<OPT_HexagonLinkOptTable>(ArgList))
      return LINK_FAIL;
    if (!processOptions<OPT_HexagonLinkOptTable>(ArgList))
      return LINK_FAIL;
    if (!checkOptions<OPT_HexagonLinkOptTable>(ArgList))
      return LINK_FAIL;

    if (!ELDFlagsArgs.empty())
      m_Config.raise(eld::diag::note_eld_flags)
          << m_Config.options().outputFileName()
          << llvm::join(ELDFlagsArgs, " ");

    if (!overrideOptions<OPT_HexagonLinkOptTable>(ArgList))
      return LINK_FAIL;
    if (!createInputActions<OPT_HexagonLinkOptTable>(ArgList, Action))
      return LINK_FAIL;
    if (!doLink<OPT_HexagonLinkOptTable>(ArgList, Action))
      return LINK_FAIL;
  }
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
  // --gpsize
  m_Config.options().setGPSize(getInteger(Args, T::gpsize, 8));

  // --disable-guard-for-weak-undefs
  if (Args.hasArg(T::disable_guard_for_weak_undef))
    m_Config.options().setDisableGuardForWeakUndefs();

  // --relax
  if (Args.hasArg(T::relax))
    m_Config.options().enableRelaxation();

  // --relax=<regex>
  for (auto *arg : Args.filtered(T::relax_value)) {
    // Enable relaxation when a pattern is provided.
    m_Config.options().enableRelaxation();
    m_Config.options().addRelaxSection(arg->getValue());
  }

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
