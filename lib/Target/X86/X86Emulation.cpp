//===- X86Emulation.cpp----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "X86.h"
#include "eld/Support/TargetRegistry.h"

//===----------------------------------------------------------------------===//
// X86Emulation
//===----------------------------------------------------------------------===//
extern "C" void ELDInitializeX86Emulation() {
  // Register both emulations from the X86 family hook.
  eld::TargetRegistry::RegisterEmulation(eld::TheX86_32Target,
                                         eld::emulateX86_32LD);
  eld::TargetRegistry::RegisterEmulation(eld::Thex86_64Target,
                                         eld::emulatex86_64LD);
}
