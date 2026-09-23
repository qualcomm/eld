//===- X86_32Info.h------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
// Refer to eld/Target/TargetInfo.h for additional hooks.
//===----------------------------------------------------------------------===//

#ifndef ELD_TARGET_X86_32_INFO_H
#define ELD_TARGET_X86_32_INFO_H

#include "eld/Target/TargetInfo.h"
#include "llvm/BinaryFormat/ELF.h"

namespace eld {

class X86_32Info : public TargetInfo {
public:
  explicit X86_32Info(LinkerConfig &pConfig) : TargetInfo(pConfig) {}

  uint32_t machine() const override { return llvm::ELF::EM_386; }
  std::string getMachineStr() const override { return "i386"; }
  uint64_t flags() const override { return 0; }
};

} // namespace eld

#endif
