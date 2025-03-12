//===- x86_64TargetMachine.h-----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
#ifndef ELD_X86_64_TARGET_MACHINE_H
#define ELD_X86_64_TARGET_MACHINE_H
#include "eld/Target/TargetMachine.h"
#include "x86_64.h"

namespace eld {

class x86_64TargetMachine : public ELDTargetMachine {
public:
  x86_64TargetMachine(const llvm::Target &pLLVMTarget,
                      const eld::Target &pELDTarget,
                      const std::string &pTriple);
};

} // namespace eld

#endif
