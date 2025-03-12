//===- AArch64TargetMachine.cpp--------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "AArch64TargetMachine.h"
#include "AArch64.h"
#include "eld/Support/TargetRegistry.h"

using namespace eld;

AArch64BaseTargetMachine::AArch64BaseTargetMachine(
    const llvm::Target &pLLVMTarget, const eld::Target &pELDTarget,
    const std::string &pTriple)
    : ELDTargetMachine() {}

//===----------------------------------------------------------------------===//
// Initialize ELDTargetMachine
//===----------------------------------------------------------------------===//
extern "C" void ELDInitializeAArch64LDTarget() {
  // Register createTargetMachine function pointer to eld::Target
  eld::RegisterTargetMachine<eld::AArch64BaseTargetMachine> X(
      eld::TheAArch64Target);
}
