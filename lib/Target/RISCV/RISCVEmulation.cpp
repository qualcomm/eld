//===- RISCVEmulation.cpp--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "RISCV.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Support/TargetRegistry.h"
#include "eld/Target/ELFEmulation.h"
#include "llvm/ADT/StringSwitch.h"

using namespace llvm;

namespace eld {

static bool ELDEmulateRISCVELF(LinkerScript &pScript, LinkerConfig &pConfig) {
  // RISCV is little endian for now.
  pConfig.targets().setEndian(TargetOptions::Little);
  if (pConfig.targets().getArch() == "riscv32")
    pConfig.targets().setBitClass(32);
  else
    pConfig.targets().setBitClass(64);

  if (!ELDEmulateELF(pScript, pConfig))
    return false;

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
