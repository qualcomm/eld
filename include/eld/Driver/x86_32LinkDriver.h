//===- x86_32LinkDriver.h--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_DRIVER_X86_32LINKDRIVER_H
#define ELD_DRIVER_X86_32LINKDRIVER_H

#include "eld/Driver/GnuLdDriver.h"
#include "llvm/ADT/StringRef.h"

class x86_32LinkDriver : public GnuLdDriver {
public:
  static x86_32LinkDriver *Create(eld::LinkerConfig &C,
                                  std::string InferredArch) {
    return eld::make<x86_32LinkDriver>(C, InferredArch);
  }

  x86_32LinkDriver(eld::LinkerConfig &C, std::string InferredArch)
      : GnuLdDriver(C, DriverFlavor::x86_32) {
    Config.targets().setArch(InferredArch);
  }

  static x86_32LinkDriver *Create(eld::LinkerConfig &C, bool is64bit) {
    return eld::make<x86_32LinkDriver>(C, is64bit);
  }

  x86_32LinkDriver(eld::LinkerConfig &C, bool)
      : GnuLdDriver(C, DriverFlavor::x86_32) {
    Config.targets().setArch("i386");
  }

  static bool isValidEmulation(llvm::StringRef Emulation) {
    return Emulation == "elf_i386";
  }

  static std::string getInferredArch(llvm::StringRef) { return "i386"; }

  static bool isMyArch(llvm::StringRef MArch) { return MArch == "i386"; }
};

#endif
