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
#include "x86_64RelocationFunctions.h"
#include "llvm/Support/MathExtras.h"

namespace eld {
namespace x86_64 {

namespace {
bool checkRange(uint64_t Value, bool IsSigned, uint32_t Bits) {
  bool Reachable;
  switch (Bits) {
  case 8: {
    if (IsSigned)
      Reachable = llvm::isInt<8>(static_cast<int64_t>(Value));
    else
      Reachable = llvm::isUInt<8>(Value);
  } break;
  case 16: {
    if (IsSigned)
      Reachable = llvm::isInt<16>(static_cast<int64_t>(Value));
    else
      Reachable = llvm::isUInt<16>(Value);
  } break;
  case 32: {
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

uint64_t clearImmediateBits(uint64_t Instr, uint32_t Bits) {
  switch (Bits) {
  case 8:
    return Instr & 0xFFFFFFFFFFFFFF00ULL;
  case 16:
    return Instr & 0xFFFFFFFFFFFF0000ULL;
  case 32:
    return Instr & 0xFFFFFFFF00000000ULL;
  case 64:
    return 0ULL;
  default:
    return Instr;
  }
}

// The Relocation helper function that computes the Instruction bits with the
// relocation applied.
template <typename T>
uint64_t doRelocHelper(const RelocationDescriptor &Desc, uint64_t Instruction,
                       T Value) {
  Instruction = clearImmediateBits(Instruction, Desc.Size);
  switch (Desc.Size) {
  case 64:
    Value = encode64(Value);
    break;
  case 32:
    Value = encode32(Value);
    break;
  case 16:
    Value = encode16(Value);
    break;
  case 8:
    Value = encode8(Value);
    break;
  default:
    break;
  }
  return Value | Instruction;
}
} // anonymous namespace

uint64_t doRelocX86_64(const RelocationDescriptor &Desc, uint64_t Instruction,
                       uint64_t Value) {
  if (Desc.IsSigned)
    return doRelocHelper<int64_t>(Desc, Instruction, Value);
  return doRelocHelper<uint64_t>(Desc, Instruction, Value);
}

bool verifyRangeX86_64(const RelocationDescriptor &Desc, uint64_t Value) {
  return checkRange(Value, Desc.IsSigned, Desc.Size);
}

bool isTruncatedX86_64(const RelocationDescriptor &Desc, uint64_t Value) {
  if (Desc.IsSigned)
    return false;
  unsigned N = Desc.Size;
  return Value & (~0U >> (64 - N));
}

} // namespace x86_64
} // namespace eld
