//===- x86_64TargetMachine.cpp---------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "x86_64TargetMachine.h"
#include "eld/Support/TargetRegistry.h"
#include "x86_64.h"

extern "C" void ELDInitializex86_64LDTarget() {
  // Register createTargetMachine function pointer to eld::Target
  eld::RegisterTargetMachine<eld::x86_64TargetMachine> X(eld::Thex86_64Target);
}

using namespace eld;

//===----------------------------------------------------------------------===//
// x86_64TargetMachine
//===----------------------------------------------------------------------===//
x86_64TargetMachine::x86_64TargetMachine(const llvm::Target &pLLVMTarget,
                                         const eld::Target &pELDTarget,
                                         const std::string &pTriple)
    : ELDTargetMachine() {}
