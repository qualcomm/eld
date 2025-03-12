//===- RISCVTargetMachine.cpp----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "RISCVTargetMachine.h"
#include "RISCV.h"
#include "eld/Support/Target.h"
#include "eld/Support/TargetRegistry.h"

extern "C" void ELDInitializeRISCVLDTarget() {
  // Register createTargetMachine function pointer to eld::Target
  eld::RegisterTargetMachine<eld::RISCVBaseTargetMachine> X(
      eld::TheRISCV32Target);
  eld::RegisterTargetMachine<eld::RISCVBaseTargetMachine> Y(
      eld::TheRISCV64Target);
}

using namespace eld;

//===----------------------------------------------------------------------===//
// RISCVTargetMachine
//===----------------------------------------------------------------------===//
RISCVBaseTargetMachine::RISCVBaseTargetMachine(const llvm::Target &pLLVMTarget,
                                               const eld::Target &pELDTarget,
                                               const std::string &pTriple)
    : ELDTargetMachine() {}
