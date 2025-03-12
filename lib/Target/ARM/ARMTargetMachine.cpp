//===- ARMTargetMachine.cpp------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#include "ARMTargetMachine.h"
#include "ARM.h"
#include "eld/Support/TargetRegistry.h"

using namespace eld;

ARMBaseTargetMachine::ARMBaseTargetMachine(const llvm::Target &pLLVMTarget,
                                           const eld::Target &pELDTarget,
                                           const std::string &pTriple)
    : ELDTargetMachine() {}

//===----------------------------------------------------------------------===//
// Initialize ELDTargetMachine
//===----------------------------------------------------------------------===//
extern "C" void ELDInitializeARMLDTarget() {
  // Register createTargetMachine function pointer to eld::Target
  eld::RegisterTargetMachine<eld::ARMBaseTargetMachine> X(eld::TheARMTarget);
  eld::RegisterTargetMachine<eld::ARMBaseTargetMachine> Y(eld::TheThumbTarget);
}
