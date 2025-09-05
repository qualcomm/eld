//===- x86_64RelocationCompute.cpp-----------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
// This  provides functionality to the linker and tools that need to process
// relocations and apply them.
//===----------------------------------------------------------------------===//
#include "x86_64Helper.h"
#include "x86_64LLVMExtern.h"
#include "x86_64RelocationInfo.h"
#include "llvm/Support/MathExtras.h"

namespace {
bool checkRange(uint64_t Value, bool IsSigned, EncodingType Type) {
  bool Reachable;
  switch (Type) {
  case EncTy_8: {
    if (IsSigned)
      Reachable = llvm::isInt<8>(static_cast<int64_t>(Value));
    else
      Reachable = llvm::isUInt<8>(Value);
  } break;
  case EncTy_16: {
    if (IsSigned)
      Reachable = llvm::isInt<16>(static_cast<int64_t>(Value));
    else
      Reachable = llvm::isUInt<16>(Value);
  } break;
  case EncTy_32: {
    if (IsSigned)
      Reachable = llvm::isInt<32>(static_cast<int64_t>(Value));
    else
      Reachable = llvm::isUInt<32>(Value);
  } break;
  default:
    Reachable = true;
  }
  return Reachable;
}

unsigned getNumberOfBits(EncodingType Type) {
  switch (Type) {
  case EncTy_8:
    return 8;
  case EncTy_16:
    return 16;
  case EncTy_32:
    return 32;
  case EncTy_64:
    return 64;
  default:
    break;
  }
  return 64;
}

uint64_t clearImmediateBits(uint64_t Instr, EncodingType Type) {
  switch (Type) {
  case EncTy_8:
    return Instr & 0xFFFFFFFFFFFFFF00ULL;
  case EncTy_16:
    return Instr & 0xFFFFFFFFFFFF0000ULL;
  case EncTy_32:
    return Instr & 0xFFFFFFFF00000000ULL;
  case EncTy_64:
    return 0ULL;
  default:
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
  case EncTy_64:
    Value = encode64(Value);
    break;
  case EncTy_32:
    Value = encode32(Value);
    break;
  case EncTy_16:
    Value = encode16(Value);
    break;
  case EncTy_8:
    Value = encode8(Value);
    break;
  default:
    break;
  }
  return Value | Instruction;
}
} // anonymous namespace

extern "C" {
// This function finds the Mask for the Instruction and applies the Mask.

uint64_t doRelocX86_64(const RelocationInfo &RelocInfo, uint64_t Instruction,
                       uint64_t Value) {
  if (x86_64Relocs[RelocInfo.Type].IsSigned) {
    return doRelocHelper<int64_t>(RelocInfo, Instruction, Value);
  } else {
    return doRelocHelper<uint64_t>(RelocInfo, Instruction, Value);
  }
}

/* Verify the Range specified by the ABI */
bool verifyRangeX86_64(const RelocationInfo &RelocInfo, uint64_t Value) {
  if (RelocInfo.IsSigned)
    return checkRange(static_cast<int64_t>(Value) >> RelocInfo.Shift,
                      RelocInfo.IsSigned, RelocInfo.EncType);
  else
    return checkRange(static_cast<uint64_t>(Value) >> RelocInfo.Shift,
                      RelocInfo.IsSigned, RelocInfo.EncType);
}

/* Check if the result will be Truncated */
bool isTruncatedX86_64(const RelocationInfo &RelocInfo, uint64_t Value) {
  if (x86_64Relocs[RelocInfo.Type].IsSigned)
    return false;
  unsigned N = getNumberOfBits(RelocInfo.EncType);
  return Value & (~0U >> (64 - N));
}

} // extern "C"
