//===- HexagonTargetInfo.cpp-----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
#include "eld/Support/Target.h"
#include "eld/Support/TargetRegistry.h"
#include "llvm/Object/ELF.h"

namespace eld {

eld::Target TheHexagonTarget;

extern "C" void ELDInitializeHexagonLDTargetInfo() {
  // register into eld::TargetRegistry
  eld::RegisterTarget X(TheHexagonTarget, "hexagon", llvm::ELF::EM_HEXAGON);
}

} // namespace eld
