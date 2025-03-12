//===- TemplateRelocationCompute.cpp---------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
// This  provides functionality to the linker and tools that need to process
// relocations and apply them.
//===----------------------------------------------------------------------===//
#include "TemplateLLVMExtern.h"
#include "TemplateRelocationInfo.h"

// The Relocation helper function that computes the Instruction bits with the
// relocation applied.
template <typename T>
static uint32_t doRelocHelper(uint32_t RelocType, uint32_t Instruction,
                              T Value) {
  // If the relocation needs the value to be shifted, then lets shift.
  T ValueAfterShift = Value >> llvm::Template::Relocs[RelocType].Shift;
  // Truncate the value to the effective bits specified by the relocation.
  uint32_t EffectiveValue =
      ValueAfterShift &
      (~0U >> (32 - llvm::Template::Relocs[RelocType].EffectiveBits));
  return EffectiveValue | Instruction;
}

namespace llvm {
namespace Template {

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
  if (Relocs[RelocType].IsSigned) {
    int32_t Result = Value;
    int32_t Range = 1LL << (Relocs[RelocType].EffectiveBits - 1);
    if (Result > (Range - 1) || Result < -Range)
      return false;
    return true;
  }
  uint32_t Result = Value;
  uint32_t Range = 1LL << Relocs[RelocType].EffectiveBits;
  if (Result > Range - 1)
    return false;
  return true;
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
} // namespace Template
} // namespace llvm
