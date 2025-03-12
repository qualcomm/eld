//===- RISCVStandaloneInfo.h-----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_TARGET_RISCV_STANDALONE_INFO_H
#define ELD_TARGET_RISCV_STANDALONE_INFO_H
#include "RISCVInfo.h"
#include "eld/Config/GeneralOptions.h"
#include "eld/Config/TargetOptions.h"
#include "llvm/BinaryFormat/ELF.h"

namespace eld {

class RISCVStandaloneInfo : public RISCVInfo {
public:
  RISCVStandaloneInfo(LinkerConfig &pConfig) : RISCVInfo(pConfig) {}

  uint64_t startAddr(bool linkerScriptHasSectionsCmd, bool isDynExec,
                     bool loadPhdr) const override {
    // When the linker script is present, the start address is 0x0
    if (linkerScriptHasSectionsCmd)
      return 0x0;

    // Shared libraries have a load address that begins at 0.
    if (m_Config.codeGenType() == LinkerConfig::DynObj)
      return 0;

    if (m_Config.targets().triple().isOSLinux()) {
      // 64-bit dynamic executables also needs starting address
      // as non zero
      if (m_Config.targets().is64Bits())
        return 0x400000;
      // 32-bit
      if (!isDynExec)
        return 0x8048000;
    }

    return 0x0;
  }

  void initializeAttributes(InputBuilder &pBuilder) override {
    // Warn on mismatch is by default false.
    if (!m_Config.options().hasOptionWarnNoWarnMismatch())
      m_Config.options().setWarnMismatch(false);
  }
};

} // namespace eld

#endif
