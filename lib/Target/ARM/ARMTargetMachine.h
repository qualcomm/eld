//===- ARMTargetMachine.h--------------------------------------------------===//
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

//===- ARMTargetMachine.h -------------------------------------------------===//
//===----------------------------------------------------------------------===//
#ifndef TARGET_ARM_ARMTARGETMACHINE_H
#define TARGET_ARM_ARMTARGETMACHINE_H

#include "ARM.h"
#include "eld/Target/TargetMachine.h"

namespace eld {

class ARMBaseTargetMachine : public ELDTargetMachine {
public:
  ARMBaseTargetMachine(const llvm::Target &pLLVMTarget,
                       const eld::Target &pELDTarget,
                       const std::string &pTriple);
};

} // namespace eld

#endif
