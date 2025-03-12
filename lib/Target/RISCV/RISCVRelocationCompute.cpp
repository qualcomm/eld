//===- RISCVRelocationCompute.cpp------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
// This  provides functionality to the linker and tools that need to process
// relocations and apply them.
//===----------------------------------------------------------------------===//
#include "RISCVHelper.h"
#include "RISCVLLVMExtern.h"
#include "RISCVRelocationInternal.h"
#include "llvm/Support/LEB128.h"
#include "llvm/Support/MathExtras.h"

namespace eld {

namespace {
bool checkRange(uint64_t Value, bool IsSigned, EncodingType Type) {
  switch (Type) {
  case EncTy_I:
  case EncTy_S:
  case EncTy_QC_EAI:
  case EncTy_QC_EJ:
    return llvm::isInt<32>(static_cast<int64_t>(Value));
  case EncTy_U_HI20:
  case EncTy_U_ABS20:
  case EncTy_UJ:
    return llvm::isInt<20>(static_cast<int64_t>(Value));
  case EncTy_CB:
  case EncTy_8: {
    if (IsSigned)
      return llvm::isInt<8>(static_cast<int64_t>(Value));
    else
      return llvm::isUInt<8>(Value);
  }
  case EncTy_SB:
  case EncTy_CJ:
  case EncTy_QC_EB: {
    if (IsSigned)
      return llvm::isInt<12>(static_cast<int64_t>(Value));
    else
      return llvm::isUInt<12>(Value);
  }
  case EncTy_CI:
  case EncTy_6: {
    if (IsSigned)
      return llvm::isInt<6>(static_cast<int64_t>(Value));
    else
      return llvm::isUInt<6>(Value);
  }
  case EncTy_16: {
    if (IsSigned)
      return llvm::isInt<16>(static_cast<int64_t>(Value));
    else
      return llvm::isUInt<16>(Value);
  }
  case EncTy_None:
  case EncTy_32:
  case EncTy_64:
  case EncTy_LEB128:
    return true;
  }
}

unsigned getNumberOfBits(EncodingType Type) {
  switch (Type) {
  case EncTy_I:
  case EncTy_SB:
  case EncTy_S:
  case EncTy_QC_EB:
    return 12;
  case EncTy_UJ:
  case EncTy_U_HI20:
  case EncTy_U_ABS20:
    return 20;
  case EncTy_CJ:
    return 11;
  case EncTy_CI:
  case EncTy_6:
    return 6;
  case EncTy_CB:
  case EncTy_8:
    return 8;
  case EncTy_16:
    return 16;
  case EncTy_64:
    return 64;
  case EncTy_QC_EAI:
  case EncTy_QC_EJ:
  case EncTy_32:
    return 32;
  case EncTy_LEB128:
  case EncTy_None:
    return 0;
  }
}

uint64_t clearImmediateBits(uint64_t Instr, EncodingType Type) {
  // This only has to clear bits in the bytes that are covered by
  // the relocation's Size.
  switch (Type) {
  case EncTy_I:
    return Instr & 0x000FFFFF;
  case EncTy_SB:
  case EncTy_S:
    return Instr & 0x01FFF07F;
  case EncTy_UJ:
  case EncTy_U_HI20:
  case EncTy_U_ABS20:
    return Instr & 0x00000FFF;
  case EncTy_CB:
    return Instr & 0xFFFFE383;
  case EncTy_CJ:
    return Instr & 0xFFFFE003;
  case EncTy_6:
    return Instr & 0xC0;
  case EncTy_8:
    return Instr & 0x00;
  case EncTy_16:
    return Instr & 0x0000;
  case EncTy_32:
    return Instr & 0x00000000;
  case EncTy_64:
    return Instr & 0x00000000'00000000;
  case EncTy_QC_EB:
    return Instr & 0xFFFF01FFF07F;
  case EncTy_QC_EAI:
    return Instr & 0x00000000FFFF;
  case EncTy_QC_EJ:
    return Instr & 0x000001F1F07F;
  /* C.LUI/C.LI clearing handled in doRelocHelper */
  case EncTy_CI:
  /* No overwriting being performed */
  case EncTy_None:
  case EncTy_LEB128:
    return Instr;
  }
}

// The Relocation helper function that computes the Instruction bits with the
// relocation applied.
template <typename T>
uint64_t doRelocHelper(const RelocationInfo &RelocInfo, uint64_t Instruction,
                       T Value) {
  Instruction = clearImmediateBits(Instruction, RelocInfo.EncType);
  switch (RelocInfo.EncType) {
  case EncTy_I:
    Value = encodeI(Value);
    break;
  case EncTy_S:
    Value = encodeS(Value);
    break;
  case EncTy_SB:
    Value = encodeSB(Value);
    break;
  case EncTy_UJ:
    Value = encodeUJ(Value);
    break;
  case EncTy_U_HI20:
    Value = encodeU(Value);
    break;
  case EncTy_U_ABS20:
    Value = encodeU_ABS20(Value);
    break;
  case EncTy_CB:
    Value = encodeCB(Value);
    break;
  case EncTy_CJ:
    Value = encodeCJ(Value);
    break;
  case EncTy_CI: {
    if (Value >> 12 == 0) {
      // `c.lui rd, 0` is illegal, convert to `c.li rd, 0`
      return (Instruction & 0x0F83) | 0x4000;
    } else {
      Instruction &= 0xEF83;
      Value = encodeCI(Value);
    }
    break;
  }
  case EncTy_QC_EB:
    Value = encodeQCEB(Value);
    break;
  case EncTy_QC_EAI:
    Value = encodeQCEAI(Value);
    break;
  case EncTy_QC_EJ:
    Value = encodeQCEJ(Value);
    break;
  case EncTy_32:
    Value = encode32(Value);
    break;
  case EncTy_64:
    Value = encode64(Value);
    break;
  case EncTy_16:
    Value = encode16(Value);
    break;
  case EncTy_6:
    Value = encode6(Value);
    break;
  case EncTy_8:
    Value = encode8(Value);
    break;
  case EncTy_LEB128:
    /* Handled seperately by backend */
    LLVM_FALLTHROUGH;
  case EncTy_None:
    break;
  }
  return Value | Instruction;
}
} // anonymous namespace

// This function finds the Mask for the Instruction and applies the Mask.
uint64_t doRISCVReloc(const RelocationInfo &RelocInfo, uint64_t Instruction,
                      uint64_t Value) {
  if (getRISCVReloc(RelocInfo.Type).IsSigned)
    return doRelocHelper<int64_t>(RelocInfo, Instruction, Value);
  else
    return doRelocHelper<uint64_t>(RelocInfo, Instruction, Value);
}

/* Verify the Range specified by the ABI */
bool verifyRISCVRange(const RelocationInfo &RelocInfo, uint64_t Value,
                      bool is32Bits) {
  if (RelocInfo.IsSigned) {
    int64_t signExtendedValue = llvm::SignExtend64(Value, (is32Bits ? 32 : 64));
    return checkRange(signExtendedValue >> RelocInfo.Shift, RelocInfo.IsSigned,
                      RelocInfo.EncType);
  } else
    return checkRange(static_cast<uint64_t>(Value) >> RelocInfo.Shift,
                      RelocInfo.IsSigned, RelocInfo.EncType);
}

/* Helper function to Verify alignment */

bool verifyRISCVAlignment(const RelocationInfo &RelocInfo, uint64_t Value) {
  if (RelocInfo.VerifyAlignment)
    return (Value & 1) == 0;
  return false;
}

/* Check if the result will be Truncated */
bool isTruncatedRISCV(const RelocationInfo &RelocInfo, uint64_t Value) {
  if (RelocInfo.IsSigned || RelocInfo.EncType == EncTy_None)
    return false;
  unsigned N = getNumberOfBits(RelocInfo.EncType);
  return Value & (~0U >> (32 - N));
}

// Overwrite a value encoded in LEB128
bool overwriteLEB128(uint8_t *buf, uint64_t val) {
  while (*buf & 0x80) {
    *buf++ = 0x80 | (val & 0x7f);
    val >>= 7;
  }
  // if the buffer is not big enough, we may end up
  // losing some bits, we need to raise an error
  if (val >> 7)
    return false;
  *buf = val;
  return true;
}

} // namespace eld
