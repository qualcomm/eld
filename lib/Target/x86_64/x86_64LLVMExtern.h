//===- x86_64LLVMExtern.h--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#ifndef X86_64_LLVM_EXTERN_H
#define X86_64_LLVM_EXTERN_H

#include "llvm/BinaryFormat/ELF.h"

enum EncodingType { EncTy_None, EncTy_8, EncTy_16, EncTy_32, EncTy_64 };
typedef struct {
  const char *Name;
  const uint32_t Type;
  const enum EncodingType EncType;
  const uint32_t Shift;
  const bool VerifyRange;
  const bool IsSigned;
  uint32_t Size;
} RelocationInfo;

extern "C" {
uint64_t doRelocX86_64(const RelocationInfo &RelocInfo, uint64_t Instruction,
                       uint64_t Value);

bool verifyRangeX86_64(const RelocationInfo &RelocInfo, uint64_t Value);

bool isTruncatedX86_64(const RelocationInfo &RelocInfo, uint64_t Value);

extern const RelocationInfo x86_64Relocs[];
}

#endif
