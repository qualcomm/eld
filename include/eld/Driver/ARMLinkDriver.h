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
               HELPTEXTSFORVARIANTS, METAVAR, VALUES)                          \
  ID,
#include "eld/Driver/ARMLinkerOptions.inc"
#undef OPTION
  };

  OPT_ARMLinkOptTable();
};

class ARMLinkDriver : public GnuLdDriver {
public:
  static ARMLinkDriver *Create(eld::LinkerConfig &C, Flavor F,
                               std::string Triple);

  ARMLinkDriver(eld::LinkerConfig &C, Flavor F, std::string Triple);

  virtual ~ARMLinkDriver() {}

  // Main entry point.
  int link(llvm::ArrayRef<const char *> Args,
           llvm::ArrayRef<llvm::StringRef> ELDFlagsArgs) override;

  // Parse Options.
  llvm::opt::OptTable *parseOptions(llvm::ArrayRef<const char *> ArgsArr,
                                    llvm::opt::InputArgList &ArgList) override;

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
};

#endif
