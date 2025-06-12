//===- x86_64LinkDriver.h--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

//
//===----------------------------------------------------------------------===//
#ifndef ELD_DRIVER_X86_64LINKDRIVER_H
#define ELD_DRIVER_X86_64LINKDRIVER_H

#include "eld/Config/LinkerConfig.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Core/Module.h"
#include "eld/Driver/GnuLdDriver.h"
#include "llvm/ADT/StringRef.h"

// Create OptTable class for parsing actual command line arguments
class OPT_x86_64LinkOptTable : public llvm::opt::GenericOptTable {
public:
  enum {
    INVALID = 0,
#define OPTION(PREFIXES_OFFSET, PREFIXED_NAME_OFFSET, ID, KIND, GROUP, ALIAS,  \
               ALIASARGS, FLAGS, VISIBILITY, PARAM, HELPTEXT,                  \
               HELPTEXTSFORVARIANTS, METAVAR, VALUES)                          \
  ID,
#include "eld/Driver/x86_64LinkerOptions.inc"
#undef OPTION
  };

  OPT_x86_64LinkOptTable();
};

class x86_64LinkDriver : public GnuLdDriver {
public:
  static x86_64LinkDriver *Create(eld::LinkerConfig &C, Flavor F,
                                  std::string Triple);

  x86_64LinkDriver(eld::LinkerConfig &C, Flavor F, std::string Triple);

  virtual ~x86_64LinkDriver() {}

  static bool isValidEmulation(llvm::StringRef Emulation );

  // Main entry point.
  int link(llvm::ArrayRef<const char *> Args,
           llvm::ArrayRef<llvm::StringRef> ELDFlagsArgs) override;

  // Parse Options.
  llvm::opt::OptTable *parseOptions(llvm::ArrayRef<const char *> ArgsArr,
                                    llvm::opt::InputArgList &ArgList) override;

  // Check if the options are invalid.
  template <class T = OPT_x86_64LinkOptTable>
  bool checkOptions(llvm::opt::InputArgList &Args);

  // Process the linker options.
  template <class T = OPT_x86_64LinkOptTable>
  bool processOptions(llvm::opt::InputArgList &Args);

  // Process LLVM options.
  template <class T = OPT_x86_64LinkOptTable>
  bool processLLVMOptions(llvm::opt::InputArgList &Args);

  // Process Target specific options.
  template <class T = OPT_x86_64LinkOptTable>
  bool processTargetOptions(llvm::opt::InputArgList &Args);

  template <class T = OPT_x86_64LinkOptTable>
  bool createInputActions(llvm::opt::InputArgList &Args,
                          std::vector<eld::InputAction *> &actions);
};

#endif
