//===- ARMLinkDriver.h-----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#ifndef ELD_DRIVER_ARMLINKDRIVER_H
#define ELD_DRIVER_ARMLINKDRIVER_H

#include "eld/Config/LinkerConfig.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Core/Module.h"
#include "eld/Driver/GnuLdDriver.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/TargetParser/Triple.h"
#include <optional>

namespace eld {
class DiagnosticEngine;
}

// Create OptTable class for parsing actual command line arguments
class OPT_ARMLinkOptTable : public llvm::opt::GenericOptTable {
public:
  enum {
    INVALID = 0,
#define OPTION(PREFIXES_OFFSET, PREFIXED_NAME_OFFSET, ID, KIND, GROUP, ALIAS,  \
               ALIASARGS, FLAGS, VISIBILITY, PARAM, HELPTEXT,                  \
               HELPTEXTSFORVARIANTS, METAVAR, VALUES, SUBCOMMANDIDS_OFFSET)                          \
  ID,
#include "eld/Driver/ARMLinkerOptions.inc"
#undef OPTION
  };

  OPT_ARMLinkOptTable();
};

class ARMLinkDriver : public GnuLdDriver {
public:
  static ARMLinkDriver *Create(eld::LinkerConfig &C,
                               std::string InferredArchFromProgramName);

  ARMLinkDriver(eld::LinkerConfig &C, std::string InferredArchFromProgramName);

  static ARMLinkDriver *Create(eld::LinkerConfig &C, bool is64bit);

  ARMLinkDriver(eld::LinkerConfig &C, bool is64bit);

  virtual ~ARMLinkDriver() {}

  // Main entry point.
  int link(llvm::ArrayRef<const char *> Args,
           llvm::ArrayRef<llvm::StringRef> ELDFlagsArgs) override;

  // Parse Options.
  std::optional<int> parseOptions(llvm::ArrayRef<const char *> ArgsArr,
                                  llvm::opt::InputArgList &ArgList) override;

  bool processLTOOptions(llvm::lto::Config &Conf,
                         std::vector<std::string> &LLVMOptions) override;

  // Check if the options are invalid.
  template <class T> bool checkOptions(llvm::opt::InputArgList &Args);

  // Process the linker options.
  template <class T> bool processOptions(llvm::opt::InputArgList &Args);

  // Process LLVM options.
  template <class T> bool processLLVMOptions(llvm::opt::InputArgList &Args);

  // Process Target specific options.
  template <class T> bool processTargetOptions(llvm::opt::InputArgList &Args);

  template <class T>
  bool createInputActions(llvm::opt::InputArgList &Args,
                          std::vector<eld::InputAction *> &actions);

  static std::optional<llvm::Triple>
  ParseEmulation(std::string pEmulation, eld::DiagnosticEngine *DiagEngine);

  static std::string getInferredArch(llvm::StringRef Emulation) {
    if (Emulation.starts_with("arm"))
      return "arm";
    if (Emulation.starts_with("aarch64"))
      return "aarch64";
    return "unknown";
  }

  static bool isValidEmulation(llvm::StringRef Emulation) {
    return Emulation.starts_with("arm") || Emulation.starts_with("aarch64");
  }

  static bool isMyArch(llvm::StringRef MArch) {
    return MArch == "arm" || MArch == "aarch64";
  }
};

#endif
