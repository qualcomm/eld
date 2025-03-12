//===- TemplateLLVMExtern.h------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef TEMPLATE_LLVM_EXTERN_H
#define TEMPLATE_LLVM_EXTERN_H

#include "llvm/BinaryFormat/ELF.h"

typedef struct {
  const char *Name;
  const uint32_t Type;
  const uint32_t EffectiveBits;
  const uint32_t Alignment;
  const uint32_t Shift;
  const bool VerifyRange;
  const bool VerifyAlignment;
  const bool IsSigned;
  uint32_t Size;
} RelocationInfo;

namespace llvm {
namespace Template {
extern "C" {
uint32_t doReloc(uint32_t RelocType, uint32_t Instruction, uint32_t Value);

bool verifyRange(uint32_t RelocType, uint32_t Value);

bool verifyAlignment(uint32_t RelocType, uint32_t Value);

bool isTruncated(uint32_t pRelocType, uint32_t Value);

extern const RelocationInfo Relocs[];
}
} // namespace Template
} // namespace llvm

#endif
