//===- PointerReloc.h -----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef TEST_CLAIM_USE_POINTER_RELOC_H
#define TEST_CLAIM_USE_POINTER_RELOC_H

#include "LinkerWrapper.h"

inline uint32_t pointerReloc(eld::plugin::LinkerWrapper &LW) {
  auto None = LW.getRelocationHandler().getRelocationName(0);
  // ELF relocation numbers; name lookup may visit unsupported table holes.
  return None == "R_HEX_NONE"           ? 6
         : None.find("R_AARCH64_") == 0 ? 257
         : None == "R_ARM_NONE"         ? 2
         : LW.is64Bits()                ? 2
                                        : 1;
}

#endif
