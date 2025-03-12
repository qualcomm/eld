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
#include "eld/Support/StringUtils.h"
#include "eld/Target/GNULDBackend.h"
#include <string>

using namespace eld;

//===----------------------------------------------------------------------===//
// LinkerConfig
//===----------------------------------------------------------------------===//
LinkerConfig::LinkerConfig(DiagnosticEngine *diagEngine)
    : m_Options(diagEngine), m_Targets(), m_CodeGenType(Unknown),
      m_CodePosition(Unset), m_DiagEngine(diagEngine),
      m_SearchDirs(diagEngine) {}

LinkerConfig::LinkerConfig(DiagnosticEngine *diagEngine,
                           const std::string &pTripleString)
    : m_Options(diagEngine), m_Targets(pTripleString), m_CodeGenType(Unknown),
      m_CodePosition(Unset), m_DiagEngine(diagEngine),
      m_SearchDirs(diagEngine) {}

LinkerConfig::~LinkerConfig() {}

void LinkerConfig::addCommandLine(llvm::StringRef option, bool flag) {
  m_CommandLineVector.push_back(eld::make<Flags>(option.str(), flag));
}

void LinkerConfig::addCommandLine(llvm::StringRef option,
                                  const char *argument) {
  m_CommandLineVector.push_back(eld::make<Options>(option.str(), argument));
}

void LinkerConfig::addCommandLine(llvm::StringRef option,
                                  const std::vector<std::string> &args) {
  m_CommandLineVector.push_back(
      eld::make<MultiValueOption>(option.str(), args));
}

void LinkerConfig::addCommandLine(llvm::StringRef option,
                                  llvm::StringRef args) {
  if (args.empty())
    return;
  std::vector<std::string> mapStyles = eld::string::split(args.str(), ',');
  addCommandLine(option, mapStyles);
}

// TODO: Use DIAG here.
void LinkerConfig::printOptions(llvm::raw_ostream &outs,
                                GNULDBackend const &backend, bool useColor) {
  outs << "# Notable linker command/script options:\n";
  outs << "# CPU Architecture Version: ";
  if (useColor) {
    outs.changeColor(llvm::raw_ostream::YELLOW)
        << targets().getTargetCPU() << "\n";
    outs.resetColor();
  } else {
    outs << targets().getTargetCPU() << "\n";
  }
  outs << "# Target triple environment for the link: ";
  auto TheTriple = targets().triple();
  if (useColor) {
    outs.changeColor(llvm::raw_ostream::YELLOW)
        << TheTriple.getEnvironmentTypeName(TheTriple.getEnvironment()) << "\n";
    outs.resetColor();
  } else {
    outs << TheTriple.getEnvironmentTypeName(TheTriple.getEnvironment())
         << "\n";
  }

  outs << "# Maximum GP size: ";
  if (useColor) {
    outs.changeColor(llvm::raw_ostream::YELLOW)
        << options().getGPSize() << "\n";
    outs.resetColor();
  } else {
    outs << options().getGPSize() << "\n";
  }
  outs << "# Link type: ";
  if (useColor)
    outs.changeColor(llvm::raw_ostream::YELLOW);
  if (isCodeDynamic()) {
    outs << "Dynamic";
    if (options().Bsymbolic())
      outs << " and Bsymbolic set\n";
    else
      outs << " and Bsymbolic not set\n";
  } else {
    outs << "Static\n";
  }
  // Print LTO flag status and parameters
  if (backend.getModule().needLTOToBeInvoked() || options().hasLTO()) {
    std::vector<llvm::StringRef> ltoOptions;
    if (useColor)
      outs.resetColor();
    outs << "# LTO Flag: ";
    if (useColor)
      outs.changeColor(llvm::raw_ostream::YELLOW);
    outs << "Enabled\n";
    ltoOptions = options().getLTOOptionsAsString();
    if (!ltoOptions.empty()) {
      if (useColor)
        outs.resetColor();
      outs << "# LTO Options: ";
      if (useColor)
        outs.changeColor(llvm::raw_ostream::YELLOW);
      while (!ltoOptions.empty()) {
        llvm::StringRef ltoOption = ltoOptions.back();
        ltoOptions.pop_back();
        outs << ltoOption;
        if (ltoOptions.size() != 0)
          outs << ", ";
      }
      outs << "\n";
    }
  }

  if (useColor)
    outs.resetColor();
  outs << "# ABI Page Size: ";
  if (useColor)
    outs.changeColor(llvm::raw_ostream::YELLOW);
  outs << "0x";
  outs.write_hex(backend.abiPageSize());
  outs << "\n";
  if (useColor)
    outs.resetColor();
}

const char *LinkerConfig::version() { return eld::getELDVersion().data(); }

const std::string LinkerConfig::getFileFromHash(const std::string &Hash) const {
  const auto F = m_HashToPath.find(Hash);
  if (F == m_HashToPath.end())
    return Hash;
  return F->second;
}

const std::string LinkerConfig::getHashFromFile(const std::string &File) const {
  const auto H = m_PathToHash.find(File);
  if (H == m_PathToHash.end())
    return File;
  return H->second;
}

std::string
LinkerConfig::getMappedThinArchiveMember(const std::string &archiveName,
                                         const std::string &memberName) const {
  return getHashFromFile(
      archiveName + MappingFile::getThinArchiveMemSeparator() + memberName);
}

MsgHandler LinkerConfig::raise(unsigned int pID) const {
  return m_DiagEngine->raise(pID);
}

void LinkerConfig::raiseDiagEntry(
    std::unique_ptr<plugin::DiagnosticEntry> diagEntry) const {
  return m_DiagEngine->raiseDiagEntry(std::move(diagEntry));
}

bool LinkerConfig::setWarningOption(llvm::StringRef warnOption) {
  std::string warnOpt = warnOption.lower();
  if (warnOpt == "all") {
    setShowAllWarnings();
    return true;
  }
  if (warnOpt == "command-line") {
    setShowCommandLineWarning(true);
    return true;
  }
  if (warnOpt == "linker-script") {
    setShowLinkerScriptWarning(true);
    return true;
  }
  if (warnOpt == "no-linker-script") {
    setShowLinkerScriptWarning(false);
    return true;
  }
  if (warnOpt == "zero-sized-sections") {
    setShowZeroSizedSectionsWarning(true);
    return true;
  }
  if (warnOpt == "attribute-mix") {
    setShowAttributeMixWarning(true);
    return true;
  }
  if (warnOpt == "no-attribute-mix") {
    setShowAttributeMixWarning(false);
    return true;
  }
  if (warnOpt == "archive-file") {
    setShowArchiveFileWarning(true);
    return true;
  }
  if (warnOpt == "no-archive-file") {
    setShowArchiveFileWarning(false);
    return true;
  }
  if (warnOpt == "linker-script-memory") {
    setShowLinkerScriptMemoryWarning(true);
    return true;
  }
  if (warnOpt == "no-linker-script-memory") {
    setShowLinkerScriptMemoryWarning(false);
    return true;
  }
  if (warnOpt == "bad-dot-assignments") {
    setShowBadDotAssginmentsWarning(true);
    return true;
  }
  if (warnOpt == "no-bad-dot-assignments") {
    setShowBadDotAssginmentsWarning(false);
    return true;
  }
  if (warnOpt == "whole-archive") {
    setShowWholeArchiveWarning(true);
    return true;
  }
  return false;
}

bool LinkerConfig::shouldCreateReproduceTar() const {
  return options().getRecordInputFiles() || options().isReproduceOnFail();
}

void LinkerConfig::setSymDefStyle(llvm::StringRef style) {
  if (style.lower() == "provide")
    m_SymDefStyle = Provide;
  else
    m_SymDefStyle = Default;
}

bool LinkerConfig::isSymDefStyleDefault() const {
  return m_SymDefStyle == Default;
}

bool LinkerConfig::isSymDefStyleProvide() const {
  return m_SymDefStyle == Provide;
}

bool LinkerConfig::isSymDefStyleValid() const {
  return m_SymDefStyle != UnknownSymDefStyle;
}

std::string LinkerConfig::getSymDefString() const {
  return m_Options.symDefFileStyle().upper();
}
