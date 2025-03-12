//===- ARMTargetInfo.cpp---------------------------------------------------===//
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

//===- ARMTargetInfo.cpp --------------------------------------------------===//
//===----------------------------------------------------------------------===//
#include "eld/Support/Target.h"
#include "eld/Support/TargetRegistry.h"

namespace eld {

eld::Target TheARMTarget;
eld::Target TheThumbTarget;

extern "C" void ELDInitializeARMLDTargetInfo() {
  // register into eld::TargetRegistry
  eld::RegisterTarget<llvm::Triple::arm> X(TheARMTarget, "arm");
  eld::RegisterTarget<llvm::Triple::thumb> Y(TheThumbTarget, "thumb");
}

} // namespace eld
