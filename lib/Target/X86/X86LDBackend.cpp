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
  // Register the existing x86_64 linker backend. The i386 backend will be
  // registered here when it is added to the X86 family.
  eld::TargetRegistry::RegisterGNULDBackend(eld::Thex86_64Target,
                                            eld::createx86_64LDBackend);
}
