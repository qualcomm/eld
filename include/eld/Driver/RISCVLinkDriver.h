//===- RISCVLinkDriver.h---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_DRIVER_RISCVLINKDRIVER_H
#define ELD_DRIVER_RISCVLINKDRIVER_H

#include "eld/Config/LinkerConfig.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Core/Module.h"
#include "eld/Driver/GnuLdDriver.h"
#include "llvm/ADT/StringRef.h"

// Create OptTable class for parsing actual command line arguments
class OPT_RISCVLinkOptTable : public llvm::opt::GenericOptTable {
public:
  enum {
    INVALID = 0,
#define OPTION(PREFIXES_OFFSET, PREFIXED_NAME_OFFSET, ID, KIND, GROUP, ALIAS,  \
               ALIASARGS, FLAGS, VISIBILITY, PARAM, HELPTEXT,                  \
               HELPTEXTSFORVARIANTS, METAVAR, VALUES, SUBCOMMANDIDS_OFFSET)                          \
  ID,
#include "eld/Driver/RISCVLinkerOptions.inc"
#undef OPTION
  };

  OPT_RISCVLinkOptTable();
};

class RISCVLinkDriver : public GnuLdDriver {
public:
  static RISCVLinkDriver *Create(eld::LinkerConfig &C,
                                 std::string InferredArchFromProgramName);

  RISCVLinkDriver(eld::LinkerConfig &C,
                  std::string InferredArchFromProgramName);

  static RISCVLinkDriver *Create(eld::LinkerConfig &C, bool is64bit);

  RISCVLinkDriver(eld::LinkerConfig &C, bool is64bit);

  virtual ~RISCVLinkDriver() {}

  // Main entry point.
  int link(llvm::ArrayRef<const char *> Args,
           llvm::ArrayRef<llvm::StringRef> ELDFlagsArgs) override;

  // Parse Options.
  std::optional<int> parseOptions(llvm::ArrayRef<const char *> ArgsArr,
                                  llvm::opt::InputArgList &ArgList) override;

  bool processLTOOptions(llvm::lto::Config &Conf,
                         std::vector<std::string> &LLVMOptions) override;

  // Check if the options are invalid.
  template <class T = OPT_RISCVLinkOptTable>
  bool checkOptions(llvm::opt::InputArgList &Args);

  // Process the linker options.
  template <class T = OPT_RISCVLinkOptTable>
  bool processOptions(llvm::opt::InputArgList &Args);

  // Process LLVM options.
  template <class T = OPT_RISCVLinkOptTable>
  bool processLLVMOptions(llvm::opt::InputArgList &Args);

  // Process Target specific options.
  template <class T = OPT_RISCVLinkOptTable>
  bool processTargetOptions(llvm::opt::InputArgList &Args);

  template <class T = OPT_RISCVLinkOptTable>
  bool createInputActions(llvm::opt::InputArgList &Args,
                          std::vector<eld::InputAction *> &actions);

  static bool isValidEmulation(llvm::StringRef Emulation) {
    return Emulation == "elf64lriscv" || Emulation == "elf32lriscv";
  }

  static std::string getInferredArch(llvm::StringRef Emulation) {
    if (Emulation == "elf32lriscv")
      return "riscv32";
    if (Emulation == "elf64lriscv")
      return "riscv64";
    return "unknown";
  }

  static bool isMyArch(llvm::StringRef MArch) {
    return MArch == "riscv32" || MArch == "riscv64";
  }
};

#endif
