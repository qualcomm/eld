//===- RISCVTargetInfo.cpp-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Support/Target.h"
#include "eld/Support/TargetRegistry.h"

namespace eld {

eld::Target TheRISCV32Target;
eld::Target TheRISCV64Target;

extern "C" void ELDInitializeRISCVLDTargetInfo() {
  // register into eld::TargetRegistry
  eld::RegisterTarget<llvm::Triple::riscv32> X(TheRISCV32Target, "riscv32");
  eld::RegisterTarget<llvm::Triple::riscv64> Y(TheRISCV64Target, "riscv64");
}

} // namespace eld
