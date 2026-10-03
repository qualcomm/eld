//===- X86_32RelocationInfo.h--------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
// Per-relocation properties for the i386 ELF32 relocations.
//
// The table is indexed by the ELF relocation number. Keep every slot from
// R_386_NONE through R_386_GOT32X so unsupported relocations are rejected
// explicitly instead of indexing unrelated data.
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
enum class RangeCheck { None, SignedOrUnsigned };

struct RelocationInfo {
  const char *Name;
  EncodingWidth Width;
  RangeCheck Range;
};

#define X86_32_UNSUPPORTED_RELOC(Name)                                         \
  {Name, EncodingWidth::None, RangeCheck::None}

inline constexpr RelocationInfo Relocs[] = {
    {"R_386_NONE", EncodingWidth::None, RangeCheck::None},
    {"R_386_32", EncodingWidth::Bits32, RangeCheck::None},
    X86_32_UNSUPPORTED_RELOC("R_386_PC32"),
    X86_32_UNSUPPORTED_RELOC("R_386_GOT32"),
    X86_32_UNSUPPORTED_RELOC("R_386_PLT32"),
    X86_32_UNSUPPORTED_RELOC("R_386_COPY"),
    X86_32_UNSUPPORTED_RELOC("R_386_GLOB_DAT"),
    X86_32_UNSUPPORTED_RELOC("R_386_JUMP_SLOT"),
    X86_32_UNSUPPORTED_RELOC("R_386_RELATIVE"),
    X86_32_UNSUPPORTED_RELOC("R_386_GOTOFF"),
    X86_32_UNSUPPORTED_RELOC("R_386_GOTPC"),
    X86_32_UNSUPPORTED_RELOC("R_386_32PLT"),
    X86_32_UNSUPPORTED_RELOC("R_386_RESERVED_12"),
    X86_32_UNSUPPORTED_RELOC("R_386_RESERVED_13"),
    X86_32_UNSUPPORTED_RELOC("R_386_TLS_TPOFF"),
    X86_32_UNSUPPORTED_RELOC("R_386_TLS_IE"),
    X86_32_UNSUPPORTED_RELOC("R_386_TLS_GOTIE"),
    X86_32_UNSUPPORTED_RELOC("R_386_TLS_LE"),
    X86_32_UNSUPPORTED_RELOC("R_386_TLS_GD"),
    X86_32_UNSUPPORTED_RELOC("R_386_TLS_LDM"),
    {"R_386_16", EncodingWidth::Bits16, RangeCheck::SignedOrUnsigned},
    X86_32_UNSUPPORTED_RELOC("R_386_PC16"),
    {"R_386_8", EncodingWidth::Bits8, RangeCheck::SignedOrUnsigned},
    X86_32_UNSUPPORTED_RELOC("R_386_PC8"),
    X86_32_UNSUPPORTED_RELOC("R_386_TLS_GD_32"),
    X86_32_UNSUPPORTED_RELOC("R_386_TLS_GD_PUSH"),
    X86_32_UNSUPPORTED_RELOC("R_386_TLS_GD_CALL"),
    X86_32_UNSUPPORTED_RELOC("R_386_TLS_GD_POP"),
    X86_32_UNSUPPORTED_RELOC("R_386_TLS_LDM_32"),
    X86_32_UNSUPPORTED_RELOC("R_386_TLS_LDM_PUSH"),
    X86_32_UNSUPPORTED_RELOC("R_386_TLS_LDM_CALL"),
    X86_32_UNSUPPORTED_RELOC("R_386_TLS_LDM_POP"),
    X86_32_UNSUPPORTED_RELOC("R_386_TLS_LDO_32"),
    X86_32_UNSUPPORTED_RELOC("R_386_TLS_IE_32"),
    X86_32_UNSUPPORTED_RELOC("R_386_TLS_LE_32"),
    X86_32_UNSUPPORTED_RELOC("R_386_TLS_DTPMOD32"),
    X86_32_UNSUPPORTED_RELOC("R_386_TLS_DTPOFF32"),
    X86_32_UNSUPPORTED_RELOC("R_386_TLS_TPOFF32"),
    X86_32_UNSUPPORTED_RELOC("R_386_RESERVED_38"),
    X86_32_UNSUPPORTED_RELOC("R_386_TLS_GOTDESC"),
    X86_32_UNSUPPORTED_RELOC("R_386_TLS_DESC_CALL"),
    X86_32_UNSUPPORTED_RELOC("R_386_TLS_DESC"),
    X86_32_UNSUPPORTED_RELOC("R_386_IRELATIVE"),
    X86_32_UNSUPPORTED_RELOC("R_386_GOT32X"),
};

#undef X86_32_UNSUPPORTED_RELOC

inline constexpr uint32_t MaxRelocs = sizeof(Relocs) / sizeof(Relocs[0]);

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
