//===- HexagonLinuxInfo.h--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_TARGET_HEXAGON_LINUX_INFO_H
#define ELD_TARGET_HEXAGON_LINUX_INFO_H
#include "HexagonInfo.h"
#include "eld/Config/TargetOptions.h"
#include "llvm/BinaryFormat/ELF.h"

namespace eld {

class HexagonLinuxInfo : public HexagonInfo {
public:
  HexagonLinuxInfo(LinkerConfig &config) : HexagonInfo(config) {}

  uint64_t startAddr(bool linkerScriptHasSectionsCommand, bool isDynExec,
                     bool loadPhdr) const override {
    // When linker script presents, the start address is 0x0
    if (linkerScriptHasSectionsCommand ||
        (m_Config.codeGenType() == LinkerConfig::DynObj))
      return 0x0;
    // Everything that loads the program headers, returns 4MB as the initial
    // start address. (This is only on Hexagon Linux to trap on accessing NULL).
    if (m_Config.options().isPIE() || loadPhdr || isDynExec)
      return 0x400000;
    return 0;
  }

  bool needEhdr(Module &pModule, bool linkerScriptHasSectionsCommand,
                bool isPhdr) override {
    // When linker script is present, unless SIZEOF_HEADERS is used in the
    // linker script, the linker really doesnot know whether to load the program
    // headers or not.
    if (linkerScriptHasSectionsCommand)
      return false;
    return true;
  }

  // Call out the name of the dynamic linker on hexagon linux. The only dynamic
  // linker supported is musl.
  const char *dyld() const override { return "/lib/ld-musl-hexagon.so.1"; }

  void initializeAttributes(InputBuilder &pBuilder) override {
    if (!m_Config.options().isEhFrameHdrSet())
      m_Config.options().setEhFrameHdr(true);
    // Warn on mismatch.
    if (!m_Config.options().hasOptionWarnNoWarnMismatch())
      m_Config.options().setWarnMismatch(true);
  }
};

} // namespace eld

#endif
