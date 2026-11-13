//===- x86_64LLVMExtern.h--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#ifndef X86_64_LLVM_EXTERN_H
#define X86_64_LLVM_EXTERN_H

#include "llvm/BinaryFormat/ELF.h"

namespace eld {
namespace x86_64 {

enum EncodingType { EncTy_None, EncTy_8, EncTy_16, EncTy_32, EncTy_64 };

struct RelocationDescriptor;

constexpr uint32_t getRelocationTableSize() {
  uint32_t Max = 0;
#define ELF_RELOC(RelocName, Value)                                            \
  if (Value > Max)                                                             \
    Max = Value;
#include "llvm/BinaryFormat/ELFRelocs/x86_64.def"
#undef ELF_RELOC
  return Max + 1;
}

inline constexpr uint32_t RelocationTableSize = getRelocationTableSize();

uint64_t doRelocX86_64(const RelocationDescriptor &Desc, uint64_t Instruction,
                       uint64_t Value);

bool verifyRangeX86_64(const RelocationDescriptor &Desc, uint64_t Value);

unsigned getNumberOfBits(EncodingType Type);

bool isTruncatedX86_64(const RelocationDescriptor &Desc, uint64_t Value);

} // namespace x86_64
} // namespace eld

#endif
