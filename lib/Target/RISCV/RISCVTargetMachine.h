//===- RISCVTargetMachine.h------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_RISCV_TARGET_MACHINE_H
#define ELD_RISCV_TARGET_MACHINE_H
#include "RISCV.h"
#include "eld/Target/TargetMachine.h"

namespace eld {

class RISCVBaseTargetMachine : public ELDTargetMachine {
public:
  RISCVBaseTargetMachine(const llvm::Target &pLLVMTarget,
                         const eld::Target &pELDTarget,
                         const std::string &pTriple);
};

} // namespace eld

#endif
