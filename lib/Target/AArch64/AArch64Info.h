//===- AArch64Info.h-------------------------------------------------------===//
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

#ifndef TARGET_AARCH64_AARCH64INFO_H
#define TARGET_AARCH64_AARCH64INFO_H
#include "eld/Core/Module.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Target/TargetInfo.h"
#include "llvm/BinaryFormat/ELF.h"

namespace eld {

class GNULDBackend;

class AArch64Info : public TargetInfo {
public:
  AArch64Info(LinkerConfig &pConfig);

  uint32_t machine() const override { return llvm::ELF::EM_AARCH64; }

  std::string getMachineStr() const override { return "AArch64"; }

  // AArch64's max page size is 64K.
  uint64_t abiPageSize(bool linkerScriptHasSectionsCmd) const override {
    return 0x1000;
  }

  uint64_t defaultTextSegmentAddr() const { return 0x400000; }

  // There are no processor-specific flags so this field shall contain zero.
  uint64_t flags() const override { return 0x0; }

  uint64_t startAddr(bool linkerScriptHasSectionsCmd, bool isDynExec,
                     bool loadPhdr) const override {
    // When linker script presents, the start address is 0x0
    if (linkerScriptHasSectionsCmd)
      return 0x0;

    // Handle non-shared library executables & Linux.
    if (m_Config.codeGenType() == LinkerConfig::Exec &&
        m_Config.targets().triple().isOSLinux())
      return 0x400000;

    return 0;
  }

  bool isAndroid() const { return (m_Config.targets().triple().isAndroid()); }

  bool needEhdr(Module &pModule, bool linkerScriptHasSectionsCmd,
                bool isPhdr) override;

  bool InitializeDefaultMappings(Module &pModule) override;

  std::string flagString(uint64_t flag) const override;

  void initializeAttributes(InputBuilder &pBuilder) override {
    // Warn on mismatch.
    if (!m_Config.options().hasOptionWarnNoWarnMismatch())
      m_Config.options().setWarnMismatch(false);
  }
};

} // namespace eld

#endif
