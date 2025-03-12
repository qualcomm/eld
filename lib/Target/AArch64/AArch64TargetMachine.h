//===- AArch64TargetMachine.h----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef TARGET_AARCH64_AARCH64TARGETMACHINE_H
#define TARGET_AARCH64_AARCH64TARGETMACHINE_H

#include "AArch64.h"
#include "eld/Target/TargetMachine.h"

namespace eld {

class AArch64BaseTargetMachine : public ELDTargetMachine {
public:
  AArch64BaseTargetMachine(const llvm::Target &pLLVMTarget,
                           const eld::Target &pELDTarget,
                           const std::string &pTriple);
};

} // namespace eld

#endif
