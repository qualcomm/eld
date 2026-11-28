//===- LinkerConfig.cpp----------------------------------------------------===//
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
#include "eld/Config/LinkerConfig.h"
#include "eld/Config/Config.h"
#include "eld/Config/Version.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Input/ArchiveMemberInput.h"
#include "eld/Input/InputTree.h"
#include "eld/Support/MappingFile.h"
#include "eld/Support/MsgHandling.h"
#include "eld/Support/Path.h"
#include "eld/Support/StringUtils.h"
#include "eld/Target/GNULDBackend.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include <string>

using namespace eld;

//===----------------------------------------------------------------------===//
// LinkerConfig
//===----------------------------------------------------------------------===//
LinkerConfig::LinkerConfig(DiagnosticEngine *DiagEngine)
    : GenOptions(DiagEngine), Targets(), CodeGen(Unknown), CodePos(Unset),
      DiagEngine(DiagEngine), searchDirs(DiagEngine) {}

LinkerConfig::LinkerConfig(DiagnosticEngine *DiagEngine,
                           const std::string &PTripleString)
    : GenOptions(DiagEngine), Targets(PTripleString), CodeGen(Unknown),
      CodePos(Unset), DiagEngine(DiagEngine), searchDirs(DiagEngine) {}

LinkerConfig::~LinkerConfig() {}

void LinkerConfig::setChroot(llvm::StringRef C) {
  options().setChroot(C);
  raise(Diag::verbose_setting_chroot) << C;
}

std::string LinkerConfig::applyChroot(llvm::StringRef Path) const {
  llvm::StringRef Root = options().getChroot();
  if (!Root.empty() && !Path.empty() &&
      llvm::sys::path::is_separator(Path.front())) {
    while (Root.size() > 1 && llvm::sys::path::is_separator(Root.back()))
      Root = Root.drop_back();
    if (Root == "/" || Root == "\\")
      return Path.str();
    return (Root + Path).str();
  }
  return Path.str();
}

bool LinkerConfig::existsInChroot(llvm::StringRef Path) const {
  return llvm::sys::fs::exists(applyChroot(Path));
}

const sys::fs::Path *
LinkerConfig::findPluginConfigFile(llvm::StringRef Type,
                                   const std::string &Name,
                                   const std::string &PluginName) const {
  std::string ResolvedName = applyChroot(Name);
  // A missing rooted absolute config must not fall back to host search paths.
  if (ResolvedName != Name && !llvm::sys::fs::exists(ResolvedName))
    return nullptr;
  return directories().findFile(Type, ResolvedName, PluginName);
}

void LinkerConfig::addCommandLine(llvm::StringRef Option, bool Flag) {
  CommandLineVector.push_back(eld::make<Flags>(Option.str(), Flag));
}

void LinkerConfig::addCommandLine(llvm::StringRef Option,
                                  const char *Argument) {
  CommandLineVector.push_back(eld::make<Options>(Option.str(), Argument));
}

void LinkerConfig::addCommandLine(llvm::StringRef Option,
                                  const std::vector<std::string> &Args) {
  CommandLineVector.push_back(eld::make<MultiValueOption>(Option.str(), Args));
}

void LinkerConfig::addCommandLine(llvm::StringRef Option,
                                  llvm::StringRef Args) {
  if (Args.empty())
    return;
  std::vector<std::string> MapStyles = eld::string::split(Args.str(), ',');
  addCommandLine(Option, MapStyles);
}

// TODO: Use DIAG here.
void LinkerConfig::printOptions(llvm::raw_ostream &Outs, Module const &M,
                                bool UseColor) {
  Outs << "# Notable linker command/script options:\n";
  Outs << "# CPU Architecture Version: ";
  if (UseColor) {
    Outs.changeColor(llvm::raw_ostream::YELLOW)
        << targets().getTargetCPU() << "\n";
    Outs.resetColor();
  } else {
    Outs << targets().getTargetCPU() << "\n";
  }
  Outs << "# Target triple environment for the link: ";
  auto TheTriple = targets().triple();
  if (UseColor) {
    Outs.changeColor(llvm::raw_ostream::YELLOW)
        << TheTriple.getEnvironmentTypeName(TheTriple.getEnvironment()) << "\n";
    Outs.resetColor();
  } else {
    Outs << TheTriple.getEnvironmentTypeName(TheTriple.getEnvironment())
         << "\n";
  }

  Outs << "# Maximum GP size: ";
  if (UseColor) {
    Outs.changeColor(llvm::raw_ostream::YELLOW)
        << options().getGPSize() << "\n";
    Outs.resetColor();
  } else {
    Outs << options().getGPSize() << "\n";
  }
  Outs << "# Link type: ";
  if (UseColor)
    Outs.changeColor(llvm::raw_ostream::YELLOW);
  if (isCodeDynamic()) {
    Outs << "Dynamic";
    if (options().bsymbolic())
      Outs << " and Bsymbolic set\n";
    else
      Outs << " and Bsymbolic not set\n";
  } else {
    Outs << "Static\n";
  }
  // Print LTO flag status and parameters
  if (M.needLTOToBeInvoked() || options().hasLTO()) {
    std::vector<llvm::StringRef> LtoOptions;
    if (UseColor)
      Outs.resetColor();
    Outs << "# LTO Flag: ";
    if (UseColor)
      Outs.changeColor(llvm::raw_ostream::YELLOW);
    Outs << "Enabled\n";
    LtoOptions = options().getLTOOptionsAsString();
    if (!LtoOptions.empty()) {
      if (UseColor)
        Outs.resetColor();
      Outs << "# LTO Options: ";
      if (UseColor)
        Outs.changeColor(llvm::raw_ostream::YELLOW);
      while (!LtoOptions.empty()) {
        llvm::StringRef LtoOption = LtoOptions.back();
        LtoOptions.pop_back();
        Outs << LtoOption;
        if (LtoOptions.size() != 0)
          Outs << ", ";
      }
      Outs << "\n";
    }
  }

  if (UseColor)
    Outs.resetColor();

  if (!M.isBackendInitialized())
    return;

  Outs << "# ABI Page Size: ";
  if (UseColor)
    Outs.changeColor(llvm::raw_ostream::YELLOW);
  Outs << "0x";
  Outs.write_hex(M.getBackend().abiPageSize());
  Outs << "\n";
  if (UseColor)
    Outs.resetColor();
}

const char *LinkerConfig::version() { return eld::getELDVersion().data(); }

std::string LinkerConfig::getFileFromHash(const std::string &Hash) const {
  const auto F = HashToPath.find(Hash);
  if (F == HashToPath.end())
    return Hash;
  return F->second;
}

std::string LinkerConfig::getHashFromFile(const std::string &FileName) const {
  const auto H = PathToHash.find(FileName);
  if (H == PathToHash.end())
    return FileName;
  return H->second;
}

std::string
LinkerConfig::getMappedThinArchiveMember(const std::string &ArchiveName,
                                         const std::string &MemberName) const {
  return getHashFromFile(
      ArchiveName + MappingFile::getThinArchiveMemSeparator() + MemberName);
}

MsgHandler LinkerConfig::raise(unsigned int PId) const {
  return DiagEngine->raise(PId);
}

void LinkerConfig::raiseDiagEntry(
    std::unique_ptr<plugin::DiagnosticEntry> DiagEntry) const {
  return DiagEngine->raiseDiagEntry(std::move(DiagEntry));
}

bool LinkerConfig::setWarningOption(llvm::StringRef WarnOption) {
  std::string WarnOpt = WarnOption.lower();
  if (WarnOpt == "all") {
    setShowAllWarnings();
    return true;
  }
  if (WarnOpt == "command-line") {
    setShowCommandLineWarning(true);
    return true;
  }
  if (WarnOpt == "linker-script") {
    setShowLinkerScriptWarning(true);
    return true;
  }
  if (WarnOpt == "no-linker-script") {
    setShowLinkerScriptWarning(false);
    return true;
  }
  if (WarnOpt == "error") {
    options().setWarningsAsErrors(true);
    return true;
  }
  if (WarnOpt == "no-error") {
    options().setWarningsAsErrors(false);
    return true;
  }
  if (WarnOpt == "zero-sized-sections") {
    setShowZeroSizedSectionsWarning(true);
    return true;
  }
  if (WarnOpt == "attribute-mix") {
    setShowAttributeMixWarning(true);
    return true;
  }
  if (WarnOpt == "no-attribute-mix") {
    setShowAttributeMixWarning(false);
    return true;
  }
  if (WarnOpt == "archive-file") {
    setShowArchiveFileWarning(true);
    return true;
  }
  if (WarnOpt == "no-archive-file") {
    setShowArchiveFileWarning(false);
    return true;
  }
  if (WarnOpt == "linker-script-memory") {
    setShowLinkerScriptMemoryWarning(true);
    return true;
  }
  if (WarnOpt == "no-linker-script-memory") {
    setShowLinkerScriptMemoryWarning(false);
    return true;
  }
  if (WarnOpt == "bad-dot-assignments") {
    setShowBadDotAssginmentsWarning(true);
    return true;
  }
  if (WarnOpt == "no-bad-dot-assignments") {
    setShowBadDotAssginmentsWarning(false);
    return true;
  }
  if (WarnOpt == "whole-archive") {
    setShowWholeArchiveWarning(true);
    return true;
  }
  if (WarnOpt == "no-whole-archive") {
    setShowWholeArchiveWarning(false);
    return true;
  }
  // -Wosabi and -Wno-osabi
  if (WarnOpt == "osabi") {
    setShowOSABIWarning(true);
  }
  if (WarnOpt == "no-osabi") {
    setShowOSABIWarning(false);
  }
  // -Wversion-script and -Wno-version-script
  if (WarnOpt == "version-script") {
    setShowVersionScriptWarning(true);
    return true;
  }
  if (WarnOpt == "no-version-script") {
    setShowVersionScriptWarning(false);
    return true;
  }
  if (WarnOpt == "conflicting-options") {
    setShowConflictingOptionsWarning(true);
    return true;
  }
  if (WarnOpt == "no-conflicting-options") {
    setShowConflictingOptionsWarning(false);
    return true;
  }
  return false;
}

bool LinkerConfig::shouldCreateReproduceTar() const {
  return options().getRecordInputFiles() || options().isReproduceOnFail();
}

void LinkerConfig::setSymDefStyle(llvm::StringRef Style) {
  if (Style.lower() == "provide")
    SymDefStyleValue = Provide;
  else
    SymDefStyleValue = Default;
}

bool LinkerConfig::isSymDefStyleDefault() const {
  return SymDefStyleValue == Default;
}

bool LinkerConfig::isSymDefStyleProvide() const {
  return SymDefStyleValue == Provide;
}

bool LinkerConfig::isSymDefStyleValid() const {
  return SymDefStyleValue != UnknownSymDefStyle;
}

std::string LinkerConfig::getSymDefString() const {
  return GenOptions.symDefFileStyle().upper();
}

#ifdef ELD_ENABLE_SYMBOL_VERSIONING
bool LinkerConfig::shouldBuildDynamicArtifact() const {
  return isCodeDynamic() || options().forceDynamic();
}
#endif
