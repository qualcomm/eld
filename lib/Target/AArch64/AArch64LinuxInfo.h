//===- AArch64LinuxInfo.h--------------------------------------------------===//
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
#ifndef ELD_TARGET_AARCH64_LINUX_INFO_H
#define ELD_TARGET_AARCH64_LINUX_INFO_H
#include "AArch64Info.h"
#include "eld/Config/TargetOptions.h"
#include "llvm/BinaryFormat/ELF.h"

namespace eld {

class AArch64LinuxInfo : public AArch64Info {
public:
  AArch64LinuxInfo(LinkerConfig &config) : AArch64Info(config) {}

  uint64_t startAddr(bool linkerScriptHasSectionsCommand, bool isDynExec,
                     bool loadPhdr) const override {
    // When linker script present, the start address is 0x0
    if (linkerScriptHasSectionsCommand ||
        (m_Config.codeGenType() == LinkerConfig::DynObj))
      return 0x0;

    // Handle non-shared library executables & Linux.
    if (m_Config.codeGenType() == LinkerConfig::Exec &&
        m_Config.targets().triple().isOSLinux())
      return 0x400000;

    // Everything that loads the program headers, returns 4MB as the initial
    // start address. (This is only on Hexagon Linux to trap on accessing NULL).
    if (m_Config.options().isPIE() || loadPhdr || isDynExec)
      return 0;
    return 0;
  }

  bool needEhdr(Module &pModule, bool linkerScriptHasSectionsCommand,
                bool isPhdr) override {
    if (m_Config.targets().triple().isOSLinux()) {
      if (linkerScriptHasSectionsCommand)
        return false;
      return true;
    }
    // When linker script is present, unless SIZEOF_HEADERS is used in the
    // linker script, the linker really doesnot know whether to load the program
    // headers or not.
    if (linkerScriptHasSectionsCommand)
      return false;
    if (isPhdr)
      return true;
    return false;
  }
};

} // namespace eld

#endif
