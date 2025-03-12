//===- TemplateTargetInfo.cpp----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
#include "eld/Support/Target.h"
#include "eld/Support/TargetRegistry.h"

namespace eld {

eld::Target TheTemplateTarget;

extern "C" void ELDInitializeTemplateLDTargetInfo() {
  // register into eld::TargetRegistry
  eld::RegisterTarget<llvm::Triple::iu> X(TheTemplateTarget, "template");
}

} // namespace eld
