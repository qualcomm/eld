//===- x86_64StandaloneInfo.h----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_TARGET_X86_64_STANDALONE_INFO_H
#define ELD_TARGET_X86_64_STANDALONE_INFO_H
#include "eld/Config/GeneralOptions.h"
#include "eld/Config/TargetOptions.h"
#include "x86_64Info.h"
#include "llvm/BinaryFormat/ELF.h"

namespace eld {

class x86_64StandaloneInfo : public x86_64Info {
public:
  x86_64StandaloneInfo(LinkerConfig &pConfig) : x86_64Info(pConfig) {}

  uint64_t startAddr(bool linkerScriptHasSectionsCmd, bool isDynExec,
                     bool loadPhdr) const override {
    if (m_Config.codeGenType() == LinkerConfig::Exec) {
      return 0x400000;
    }
    return 0;
  }
  void initializeAttributes(InputBuilder &pBuilder) override {
    if (m_Config.codeGenType() == LinkerConfig::Object ||
        m_Config.isCodeStatic())
      pBuilder.makeBStatic();
  }
};

} // namespace eld

#endif
