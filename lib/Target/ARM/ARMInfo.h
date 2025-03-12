//===- ARMInfo.h-----------------------------------------------------------===//
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
#ifndef TARGET_ARM_ARMINFO_H
#define TARGET_ARM_ARMINFO_H
#include "eld/Core/Module.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Target/TargetInfo.h"
#include "llvm/BinaryFormat/ELF.h"

namespace eld {

class ARMInfo : public TargetInfo {
public:
  ARMInfo(LinkerConfig &pConfig) : TargetInfo(pConfig) {}

  uint32_t machine() const override { return llvm::ELF::EM_ARM; }

  std::string getMachineStr() const override { return "ARM"; }

  uint64_t defaultTextSegmentAddr() const { return 0x8000; }

  // ARM's max page size is 64KB
  // GCC uses 0x4 as segment alignment when linker script is used.
  uint64_t abiPageSize(bool linkerScriptHasSectionsCommand) const override {
    return !linkerScriptHasSectionsCommand
               ? TargetInfo::abiPageSize(linkerScriptHasSectionsCommand)
               : 0x4;
  }

  uint64_t flags() const override { return llvm::ELF::EF_ARM_EABI_VER5; }

  uint64_t startAddr(bool linkerScriptHasSectionsCommand, bool isDynExec,
                     bool loadPhdr) const override {
    // When linker script presents, the start address is 0x0
    if (linkerScriptHasSectionsCommand)
      return 0;

    // Handle non-shared library executables & Linux.
    if (m_Config.codeGenType() == LinkerConfig::Exec &&
        m_Config.targets().triple().isOSLinux())
      return 0x08048000;

    // Everything that loads the program headers, returns 0 as the initial
    // start address.
    return 0x0;
  }

  bool needEhdr(Module &pModule, bool linkerScriptHasSectionsCmd,
                bool isPhdr) override {
    if (m_Config.targets().triple().isOSLinux()) {
      if (linkerScriptHasSectionsCmd)
        return false;
      return true;
    }
    ELFSection *eh_frame_sect =
        pModule.getScript().sectionMap().find(".eh_frame");
    if (eh_frame_sect && eh_frame_sect->size())
      return true;
    return false;
  }

  bool InitializeDefaultMappings(Module &pModule) override;

  bool isAndroid() const { return (m_Config.targets().triple().isAndroid()); }

  std::string flagString(uint64_t flag) const override;

  void initializeAttributes(InputBuilder &pBuilder) override {
    // Do not warn on mismatch by default
    if (!m_Config.options().hasOptionWarnNoWarnMismatch()) {
      m_Config.options().setWarnMismatch(false);
    }
  }
};

} // namespace eld

#endif
