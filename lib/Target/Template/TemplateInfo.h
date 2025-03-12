//===- TemplateInfo.h------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_TARGET_TEMPLATE_GNU_INFO_H
#define ELD_TARGET_TEMPLATE_GNU_INFO_H
#include "eld/Config/TargetOptions.h"
#include "eld/Target/TargetInfo.h"
#include "llvm/BinaryFormat/ELF.h"

namespace eld {

class TemplateInfo : public TargetInfo {
public:
  TemplateInfo(LinkerConfig &m_Config);

  uint32_t machine() const override {
    // return the llvm::ELF target triplet here
    return 1;
  }

  /// flags - the value of ElfXX_Ehdr::e_flags
  uint64_t flags() const override;

  uint8_t OSABI() const override;

  bool checkFlags(uint64_t flag, const std::string &name) override;

  const char *flagString(uint64_t pFlag) const override;

  int32_t cmdLineFlag() const override { return m_CmdLineFlag; }

  int32_t outputFlag() const override { return m_OutputFlag; }

  bool needEhdr(Module &pModule, bool linkerScriptHasSectionsCmd,
                bool isPhdr) override {
    return false & isPhdr;
  }

  bool processNoteGNUSTACK() override { return false; }

  llvm::StringRef getOutputMCPU() const override;

private:
  uint64_t translateFlag(uint64_t pFlag) const;
  int32_t m_CmdLineFlag;
  int32_t m_OutputFlag;
};

} // namespace eld

#endif
