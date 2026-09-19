//===- RISCVEmulation.cpp--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#include "RISCV.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Input/ELDDirectory.h"
#include "eld/Support/TargetRegistry.h"
#include "eld/Target/ELFEmulation.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/FileSystem.h"

using namespace llvm;

namespace eld {

static bool ELDEmulateRISCVELF(LinkerScript &pScript, LinkerConfig &pConfig) {
  // RISCV is little endian for now.
  pConfig.targets().setEndian(TargetOptions::Little);
  if (pConfig.targets().getArch() == "riscv32")
    pConfig.targets().setBitClass(32);
  if (pConfig.targets().getArch() == "riscv64")
    pConfig.targets().setBitClass(64);

  if (!ELDEmulateELF(pScript, pConfig))
    return false;

  if (pConfig.options().nostdlib())
    return true;

  if (pConfig.targets().is64Bits()) {
    auto Sysroot = pConfig.directories().hasSysRoot()
                       ? pConfig.directories().sysroot().native()
                       : "";
    static const char *RISCV64Dirs[] = {
        "=/usr/local/lib/riscv64-linux-gnu",
        "=/lib/riscv64-linux-gnu",
        "=/usr/lib/riscv64-linux-gnu",
        "=/usr/local/lib64/lp64d",
        "=/usr/local/lib64",
        "=/lib64/lp64d",
        "=/lib64",
        "=/usr/lib64/lp64d",
        "=/usr/lib64",
        "=/usr/riscv64-linux-gnu/lib64/lp64d",
        "=/usr/riscv64-linux-gnu/lib64",
        "=/usr/riscv64-linux-gnu/lib",
    };
    for (const char *Dir : RISCV64Dirs) {
      auto EldDir = ELDDirectory(Dir, Sysroot);
      if (llvm::sys::fs::is_directory(EldDir.name()))
        pConfig.directories().insert(Dir);
    }
  }
  return true;
}

//===----------------------------------------------------------------------===//
// emulateRISCVLD - the help function to emulate RISCV ld
//===----------------------------------------------------------------------===//
bool emulateRISCVLD(LinkerScript &pScript, LinkerConfig &pConfig) {
  return ELDEmulateRISCVELF(pScript, pConfig);
}

} // namespace eld

//===----------------------------------------------------------------------===//
// RISCVEmulation
//===----------------------------------------------------------------------===//
extern "C" void ELDInitializeRISCVEmulation() {
  // Register the emulation
  eld::TargetRegistry::RegisterEmulation(eld::TheRISCV32Target,
                                         eld::emulateRISCVLD);
  eld::TargetRegistry::RegisterEmulation(eld::TheRISCV64Target,
                                         eld::emulateRISCVLD);
}