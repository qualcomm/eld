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
  // Register the existing x86_64 emulation. The i386 emulation will be
  // registered here when the X86 family gains its 32-bit backend.
  eld::TargetRegistry::RegisterEmulation(eld::Thex86_64Target,
                                         eld::emulatex86_64LD);
}
