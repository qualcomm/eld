//===- TemplateTargetMachine.cpp-------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "TemplateTargetMachine.h"
#include "Template.h"
#include "eld/Support/TargetRegistry.h"

extern "C" void ELDInitializeTemplateLDTarget() {
  // Register createTargetMachine function pointer to eld::Target
  eld::RegisterTargetMachine<eld::TemplateTargetMachine> X(
      eld::TheRTemplateTarget);
}

using namespace eld;

//===----------------------------------------------------------------------===//
// TemplateTargetMachine
//===----------------------------------------------------------------------===//
TemplateTargetMachine::TemplateTargetMachine(const llvm::Target &pLLVMTarget,
                                             const eld::Target &pELDTarget,
                                             const std::string &pTriple)
    : ELDTargetMachine() {}
