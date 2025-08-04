//===- x86_64TargetInfo.cpp------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Support/Target.h"
#include "eld/Support/TargetRegistry.h"
#include "llvm/Object/ELF.h"

namespace eld {

eld::Target Thex86_64Target;

extern "C" void ELDInitializex86_64LDTargetInfo() {
  // register into eld::TargetRegistry
  eld::RegisterTarget X(Thex86_64Target, "x86_64", llvm::ELF::EM_X86_64, true);
}

} // namespace eld
