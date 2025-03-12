//===- AArch64TargetInfo.cpp-----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===-----------------------------------------------------------------------===//

#include "eld/Support/Target.h"
#include "eld/Support/TargetRegistry.h"

namespace eld {

eld::Target TheAArch64Target;

extern "C" void ELDInitializeAArch64LDTargetInfo() {
  // register into eld::TargetRegistry
  eld::RegisterTarget<llvm::Triple::aarch64> X(TheAArch64Target, "aarch64");
}

} // namespace eld
