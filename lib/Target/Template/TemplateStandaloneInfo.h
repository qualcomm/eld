//===- TemplateStandaloneInfo.h--------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
#ifndef ELD_TARGET_TEMPLATE_STANDALONE_INFO_H
#define ELD_TARGET_TEMPLATE_STANDALONE_INFO_H
#include "TemplateInfo.h"
#include "eld/Config/GeneralOptions.h"
#include "eld/Config/TargetOptions.h"
#include "llvm/BinaryFormat/ELF.h"

namespace eld {

class TemplateStandaloneInfo : public TemplateInfo {
public:
  TemplateStandaloneInfo(LinkerConfig &pConfig) : TemplateInfo(pConfig) {}

  virtual uint64_t startAddr(bool linkerScriptHasSectionsCmd, bool isDynExec,
                             bool loadPhdr) const override {
    return 0;
  }

  void initializeAttributes(eld::IRBuilder &pBuilder) override {
    pBuilder.AgainstStatic();
  }
};

} // namespace eld

#endif
