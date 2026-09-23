//===- X86_32RelocationTable.h--------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
// The i386 relocation descriptor table.
//===----------------------------------------------------------------------===//

#ifndef ELD_TARGET_X86_32_RELOCATION_TABLE_H
#define ELD_TARGET_X86_32_RELOCATION_TABLE_H

#include "X86_32RelocationFunctions.h"
#include "X86_32RelocationInfo.h"
#include "llvm/BinaryFormat/ELF.h"

#include <array>

namespace eld {
namespace x86_32 {

struct RelocationDescriptor {
  const char *Name = nullptr;
  EncodingWidth Width = EncodingWidth::None;
  RangeCheck Range = RangeCheck::None;
  RelocationHandler Handler = &unsupported;
};

// This list contains ELD-specific behavior. Relocation names and numeric
// values come from LLVM's i386.def and are populated separately below.
// clang-format off
#define X86_32_RELOC_OVERRIDES(Add)                                             \
  Add(llvm::ELF::R_386_NONE, none, EncodingWidth::None, RangeCheck::None)       \
  Add(llvm::ELF::R_386_32, relocAbs, EncodingWidth::Bits32, RangeCheck::None)   \
  Add(llvm::ELF::R_386_16, relocAbs, EncodingWidth::Bits16,                    \
      RangeCheck::SignedOrUnsigned)                                             \
  Add(llvm::ELF::R_386_8, relocAbs, EncodingWidth::Bits8,                      \
      RangeCheck::SignedOrUnsigned)
// clang-format on

inline std::array<RelocationDescriptor, RelocationTableSize>
createRelocationDescriptors() {
  std::array<RelocationDescriptor, RelocationTableSize> Entries{};

  // LLVM's i386.def is the source of truth for public relocation names and
  // values. Entries not present in that definition remain unnamed and
  // unsupported.
#define ELF_RELOC(RelocName, Value) Entries[Value].Name = #RelocName;
#include "llvm/BinaryFormat/ELFRelocs/i386.def"
#undef ELF_RELOC

  // Add the ELD handler and encoding policy for each supported relocation.
#define ADD_X86_32_RELOC_OVERRIDE(Type, Function, FieldWidth, RangePolicy)     \
  Entries[Type].Handler = &Function;                                           \
  Entries[Type].Width = FieldWidth;                                            \
  Entries[Type].Range = RangePolicy;
  X86_32_RELOC_OVERRIDES(ADD_X86_32_RELOC_OVERRIDE)
#undef ADD_X86_32_RELOC_OVERRIDE

  return Entries;
}

inline const std::array<RelocationDescriptor, RelocationTableSize> Relocs =
    createRelocationDescriptors();

#undef X86_32_RELOC_OVERRIDES

} // namespace x86_32
} // namespace eld

#endif
