//===- HexagonStandaloneInfo.h---------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
#ifndef ELD_TARGET_HEXAGON_STANDALONE_INFO_H
#define ELD_TARGET_HEXAGON_STANDALONE_INFO_H
#include "HexagonInfo.h"
#include "eld/Config/GeneralOptions.h"
#include "eld/Config/TargetOptions.h"
#include "llvm/BinaryFormat/ELF.h"

namespace eld {

class HexagonStandaloneInfo : public HexagonInfo {
public:
  HexagonStandaloneInfo(LinkerConfig &pConfig) : HexagonInfo(pConfig) {}

  uint64_t startAddr(bool linkerScriptHasSectionsCommand, bool isDynExec,
                     bool loadPhdr) const override {
    return 0;
  }

  void initializeAttributes(InputBuilder &pBuilder) override {
    pBuilder.makeBStatic();
    // Warn on mismatch.
    if (!m_Config.options().hasOptionWarnNoWarnMismatch())
      m_Config.options().setWarnMismatch(true);
  }
};

} // namespace eld

#endif
