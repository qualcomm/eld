//===- HexagonTargetMachine.cpp--------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "HexagonTargetMachine.h"
#include "Hexagon.h"
#include "eld/Support/TargetRegistry.h"

extern "C" void ELDInitializeHexagonLDTarget() {
  // Register createTargetMachine function pointer to eld::Target
  eld::RegisterTargetMachine<eld::HexagonTargetMachine> X(
      eld::TheHexagonTarget);
}

using namespace eld;

//===----------------------------------------------------------------------===//
// HexagonTargetMachine
//===----------------------------------------------------------------------===//
HexagonTargetMachine::HexagonTargetMachine(const llvm::Target &pLLVMTarget,
                                           const eld::Target &pELDTarget,
                                           const std::string &pTriple)
    : ELDTargetMachine() {}
