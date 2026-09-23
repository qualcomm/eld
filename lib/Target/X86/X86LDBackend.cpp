//===- X86LDBackend.cpp----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "X86.h"
#include "eld/Support/TargetRegistry.h"

//===----------------------------------------------------------------------===//
// Force static initialization.
//===----------------------------------------------------------------------===//
extern "C" void ELDInitializeX86LDBackend() {
  // Register both backends from the X86 family hook.
  eld::TargetRegistry::RegisterGNULDBackend(eld::TheX86_32Target,
                                            eld::createX86_32LDBackend);
  eld::TargetRegistry::RegisterGNULDBackend(eld::Thex86_64Target,
                                            eld::createx86_64LDBackend);
}
