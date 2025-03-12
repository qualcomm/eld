//===- ARMEmulation.cpp----------------------------------------------------===//
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
#include "ARM.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Support/TargetRegistry.h"
#include "eld/Target/ELFEmulation.h"

namespace eld {

static bool ELDEmulateARMELF(LinkerScript &pScript, LinkerConfig &pConfig) {
  // set up bitclass and endian
  pConfig.targets().setEndian(TargetOptions::Little);
  pConfig.targets().setBitClass(32);

  return (ELDEmulateELF(pScript, pConfig));
}

//===----------------------------------------------------------------------===//
// emulateARMLD - the help function to emulate ARM ld
//===----------------------------------------------------------------------===//
bool emulateARMLD(LinkerScript &pScript, LinkerConfig &pConfig) {
  return ELDEmulateARMELF(pScript, pConfig);
}

} // namespace eld

//===----------------------------------------------------------------------===//
// ARMEmulation
//===----------------------------------------------------------------------===//
extern "C" void ELDInitializeARMEmulation() {
  // Register the emulation
  eld::TargetRegistry::RegisterEmulation(eld::TheARMTarget, eld::emulateARMLD);
  eld::TargetRegistry::RegisterEmulation(eld::TheThumbTarget,
                                         eld::emulateARMLD);
}
