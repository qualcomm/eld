//===- x86_64Emulation.cpp-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
#include "eld/Config/LinkerConfig.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Support/TargetRegistry.h"
#include "eld/Target/ELFEmulation.h"
#include "x86_64.h"
#include "llvm/ADT/StringSwitch.h"

using namespace llvm;

namespace eld {

static bool ELDEmulatex86_64ELF(LinkerScript &pScript, LinkerConfig &pConfig) {
  // Set Endianess and BitClass(32/64).
  pConfig.targets().setEndian(TargetOptions::Little);
  pConfig.targets().setBitClass(64);

  if (!ELDEmulateELF(pScript, pConfig))
    return false;

  return true;
}

//===----------------------------------------------------------------------===//
// emulatex86_64LD - the help function to emulate x86_64 ld
//===----------------------------------------------------------------------===//
bool emulatex86_64LD(LinkerScript &pScript, LinkerConfig &pConfig) {
  return ELDEmulatex86_64ELF(pScript, pConfig);
}

} // namespace eld

//===----------------------------------------------------------------------===//
// x86_64Emulation
//===----------------------------------------------------------------------===//
extern "C" void ELDInitializex86_64Emulation() {
  // Register the emulation
  eld::TargetRegistry::RegisterEmulation(eld::Thex86_64Target,
                                         eld::emulatex86_64LD);
}
