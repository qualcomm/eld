//===- TemplateInfo.h------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
// Refer to eld/Target/TargetInfo.h for additional hooks.
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

  std::string getMachineStr() const override { return ""; }

  /// flags - the value of ElfXX_Ehdr::e_flags
  uint64_t flags() const override;

  bool checkFlags(uint64_t flags, const InputFile *pInput, bool) override;

  std::string flagString(uint64_t pFlag) const override { return "template"; }

private:
  std::optional<uint64_t> m_OutputFlag;
};

} // namespace eld

#endif
