//===- X86_32StandaloneInfo.h--------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_TARGET_X86_32_STANDALONE_INFO_H
#define ELD_TARGET_X86_32_STANDALONE_INFO_H

#include "X86_32Info.h"

namespace eld {

class X86_32StandaloneInfo : public X86_32Info {
public:
  explicit X86_32StandaloneInfo(LinkerConfig &pConfig) : X86_32Info(pConfig) {}

  uint64_t startAddr(bool, bool, bool) const override { return 0; }

  void initializeAttributes(InputBuilder &pBuilder) override {
    if (m_Config.codeGenType() == LinkerConfig::Object ||
        m_Config.isCodeStatic())
      pBuilder.makeBStatic();
  }
};

} // namespace eld

#endif
