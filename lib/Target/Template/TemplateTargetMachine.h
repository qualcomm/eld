//===- TemplateTargetMachine.h---------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
#ifndef ELD_TEMPLATE_TARGET_MACHINE_H
#define ELD_TEMPLATE_TARGET_MACHINE_H
#include "Template.h"
#include "eld/Target/TargetMachine.h"

namespace eld {

class TemplateTargetMachine : public ELDTargetMachine {
public:
  TemplateTargetMachine(const llvm::Target &pLLVMTarget,
                        const eld::Target &pELDTarget,
                        const std::string &pTriple);
};

} // namespace eld

#endif
