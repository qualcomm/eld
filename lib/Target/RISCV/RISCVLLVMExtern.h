//===- RISCVLLVMExtern.h---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef RISCV_LLVM_EXTERN_H
#define RISCV_LLVM_EXTERN_H

#include <cstdint>
#include <string>

namespace eld {

enum EncodingType {
  EncTy_None,
  EncTy_I,
  EncTy_S,
  /* U-type, High 20 bits of Result */
  EncTy_U_HI20,
  /* U-type, Low 20 bits of Result */
  EncTy_U_ABS20,
  /* Denotes J-Type */
  EncTy_UJ,
  /* Denotes B-Type */
  EncTy_SB,
  EncTy_CB,
  EncTy_CJ,
  EncTy_CI,
  EncTy_QC_EB,
  EncTy_QC_EAI,
  EncTy_QC_EJ,
  EncTy_6,
  EncTy_8,
  EncTy_16,
  EncTy_32,
  EncTy_64,
  EncTy_LEB128,
};

typedef struct {
  const char *Name;
  const uint32_t Type;
  const enum EncodingType EncType;
  const uint32_t Alignment;
  const uint32_t Shift;
  const bool VerifyRange;
  const bool VerifyAlignment;
  const bool IsSigned;
  uint32_t Size;
} RelocationInfo;

uint64_t doRISCVReloc(const RelocationInfo &RelocInfo, uint64_t Instruction,
                      uint64_t Value);

bool verifyRISCVRange(const RelocationInfo &RelocInfo, uint64_t Value,
                      bool is32Bits);

bool verifyRISCVAlignment(const RelocationInfo &RelocInfo, uint64_t Value);

bool isTruncatedRISCV(const RelocationInfo &RelocInfo, uint64_t Value);

const RelocationInfo &getRISCVReloc(uint32_t Type);

std::string getRISCVRelocName(uint32_t pType);

bool overwriteLEB128(uint8_t *buf, uint64_t val);

} // namespace eld

#endif
