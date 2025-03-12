//===- AArch64Emulation.cpp------------------------------------------------===//
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

#include "AArch64.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Support/TargetRegistry.h"
#include "eld/Target/ELFEmulation.h"

namespace eld {

static bool ELDEmulateAArch64ELF(LinkerScript &pScript, LinkerConfig &pConfig) {
  // set up bitclass and endian
  pConfig.targets().setEndian(TargetOptions::Little);
  pConfig.targets().setBitClass(64);

  return (ELDEmulateELF(pScript, pConfig));
}

//===----------------------------------------------------------------------===//
// emulateAArch64LD - the help function to emulate AArch64 ld
//===----------------------------------------------------------------------===//
bool emulateAArch64LD(LinkerScript &pScript, LinkerConfig &pConfig) {
  return ELDEmulateAArch64ELF(pScript, pConfig);
}

} // namespace eld

//===----------------------------------------------------------------------===//
// AArch64Emulation
//===----------------------------------------------------------------------===//
extern "C" void ELDInitializeAArch64Emulation() {
  // Register the emulation
  eld::TargetRegistry::RegisterEmulation(eld::TheAArch64Target,
                                         eld::emulateAArch64LD);
}
