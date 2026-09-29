//===- X86_32RelocationInfo.h--------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
// Basic ELD policy types for i386 ELF32 relocations.
//
// The descriptor table is defined in X86_32RelocationTable.h and is indexed by
// the ELF relocation number.
//===----------------------------------------------------------------------===//

#ifndef ELD_TARGET_X86_32_RELOCATION_INFO_H
#define ELD_TARGET_X86_32_RELOCATION_INFO_H

#include <cstdint>

namespace eld {
namespace x86_32 {

// The width, in bits, of the field an i386 relocation writes into.
enum class EncodingWidth : uint8_t { None, Bits8, Bits16, Bits32 };

// No range check is required for a full-width field. Narrow fields accept
// values representable as either signed or unsigned.
//
// Signed checks require the result to fit the signed field range. R_386_PC16
// is a special case: it accepts a signed 17-bit result before truncating it
// to the 16-bit field.
enum class RangeCheck { None, SignedOrUnsigned, Signed, SignedPC16 };

// Keep the table size tied to LLVM's canonical relocation list. The array may
// contain holes because ELF relocation values are not required to be dense.
inline constexpr uint32_t getRelocationTableSize() {
  uint32_t Max = 0;
#define ELF_RELOC(RelocName, Value)                                            \
  if (Value > Max)                                                             \
    Max = Value;
#include "llvm/BinaryFormat/ELFRelocs/i386.def"
#undef ELF_RELOC
  return Max + 1;
}

inline constexpr uint32_t RelocationTableSize = getRelocationTableSize();

// Number of bits in the field an i386 relocation writes.
inline constexpr unsigned getFieldBits(EncodingWidth Width) {
  switch (Width) {
  case EncodingWidth::Bits8:
    return 8;
  case EncodingWidth::Bits16:
    return 16;
  case EncodingWidth::Bits32:
    return 32;
  case EncodingWidth::None:
    return 0;
  }
  return 0;
}

} // namespace x86_32
} // namespace eld

#endif
