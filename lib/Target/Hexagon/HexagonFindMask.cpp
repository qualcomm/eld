//===- HexagonFindMask.cpp-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// This  provides functionality to the linker and tools that need to process
// relocations and apply them.
//===----------------------------------------------------------------------===//
#include "HexagonDepDefines.h"
#include "HexagonDepMask.h"
#include "HexagonRelocationInfo.h"
#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/MathExtras.h>

// Helper function to find the BitMask from the traversing the Encoding file
// created from ISet.
static uint32_t findBitMaskHelper(uint32_t Insn, HexagonInstruction *Encodings,
                                  int32_t NumInsns) {
  for (int32_t I = 0; I < NumInsns; I++) {
    if (((Insn & 0xc000) == 0) && !(Encodings[I].isDuplex))
      continue;

    if (((Insn & 0xc000) != 0) && (Encodings[I].isDuplex))
      continue;

    if (((Encodings[I].insnMask) & Insn) == Encodings[I].insnCmpMask) {
      return Encodings[I].insnBitMask;
    }
  }
  llvm_unreachable("Cannot find encoding for Instruction!");
}

// Find the BitMask by calling the Helper function. In future this function may
// take the architecture and traverse different encodings per architecture.
static uint32_t findMask(uint32_t I) {
  return findBitMaskHelper((uint32_t)I, InstructionEncodings,
                           sizeof(InstructionEncodings) /
                               sizeof(HexagonInstruction));
}

// Scatters the Value pData over the Mask.
static uint32_t applyMask(uint32_t Mask, uint32_t Data) {
  uint32_t Result = 0;
  size_t Off = 0;

  for (size_t Bit = 0; Bit != sizeof(uint32_t) * 8; ++Bit) {
    const bool ValBit = (Data >> Off) & 1;
    const bool MaskBit = (Mask >> Bit) & 1;
    if (MaskBit) {
      Result |= (ValBit << Bit);
      ++Off;
    }
  }
  return Result;
}

// The Relocation helper function that computes the Instruction bits with the
// relocation applied.
template <typename T>
static uint32_t doRelocHelper(uint32_t RelocType, uint32_t Instruction,
                              T Value) {
  // If the relocation needs the value to be shifted, then lets shift.
  T ValueAfterShift = Value >> llvm::Hexagon::Relocs[RelocType].Shift;
  // Truncate the value to the effective bits specified by the relocation.
  uint32_t EffectiveValue =
      ValueAfterShift &
      (~0U >> (32 - llvm::Hexagon::Relocs[RelocType].EffectiveBits));
  // Get the BitMask.
  uint32_t BitMask = llvm::Hexagon::Relocs[RelocType].BitMask;
  if (BitMask == 0)
    BitMask = findMask(Instruction);
  Instruction &= ~BitMask;
  EffectiveValue = applyMask(BitMask, EffectiveValue);
  return EffectiveValue | Instruction;
}

namespace llvm {
namespace Hexagon {

extern "C" {

// This function finds the Mask for the Instruction and applies the Mask.
uint32_t doReloc(uint32_t RelocType, uint32_t Instruction, uint32_t Value) {
  if (Relocs[RelocType].IsSigned)
    return doRelocHelper<int32_t>(RelocType, Instruction, Value);
  else
    return doRelocHelper<uint32_t>(RelocType, Instruction, Value);
}

/* Verify the Range specified by the ABI */
bool verifyRange(uint32_t RelocType, uint32_t Value) {
  if (Relocs[RelocType].IsSigned)
    return llvm::isIntN(Relocs[RelocType].EffectiveBits, (int32_t)Value);
  return llvm::isUIntN(Relocs[RelocType].EffectiveBits, Value);
}

/* Helper function to Verify alignment */
bool verifyAlignment(uint32_t RelocType, uint32_t Value) {
  return Value % Relocs[RelocType].Alignment == 0;
}

/* Check if the result will be Truncated */
bool isTruncated(uint32_t RelocType, uint32_t Value) {
  if (Relocs[RelocType].IsSigned)
    return false;
  return Value & (~0U >> (32 - Relocs[RelocType].EffectiveBits));
}

} // extern "C"
} // namespace Hexagon
} // namespace llvm
