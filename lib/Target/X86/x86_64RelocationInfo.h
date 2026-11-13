//===- x86_64RelocationInfo.h----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
// The x86-64 relocation table, indexed by the ELF relocation number. Names come
// from LLVM's x86_64.def. Handler and encoding come from the override list. A
// number that is not in the list has no name, no encoding, size 0, and the
// unsupported handler.
//===----------------------------------------------------------------------===//
#ifndef X86_64_RELOCATION_INFO_H
#define X86_64_RELOCATION_INFO_H

#include "x86_64RelocationFunctions.h"
#include "llvm/BinaryFormat/ELF.h"

#include <array>

namespace eld {
namespace x86_64 {

// clang-format off
#define X86_64_RELOC_OVERRIDES(Add)                                                 \
  Add(llvm::ELF::R_X86_64_NONE, none, EncTy_None, 0, false, false, 64)             \
  Add(llvm::ELF::R_X86_64_64, relocAbs, EncTy_64, 0, false, false, 64)             \
  Add(llvm::ELF::R_X86_64_PC32, relocPCRel, EncTy_32, 0, true, true, 32)           \
  Add(llvm::ELF::R_X86_64_GOT32, unsupported, EncTy_32, 0, false, false, 32)       \
  Add(llvm::ELF::R_X86_64_PLT32, relocPLT32, EncTy_32, 0, true, true, 32)          \
  Add(llvm::ELF::R_X86_64_COPY, none, EncTy_None, 0, false, false, 64)             \
  Add(llvm::ELF::R_X86_64_GLOB_DAT, unsupported, EncTy_64, 0, false, false, 64)    \
  Add(llvm::ELF::R_X86_64_JUMP_SLOT, unsupported, EncTy_64, 0, false, false, 64)   \
  Add(llvm::ELF::R_X86_64_RELATIVE, unsupported, EncTy_64, 0, false, false, 64)    \
  Add(llvm::ELF::R_X86_64_GOTPCREL, relocGOTRelative, EncTy_32, 0, true, true, 32) \
  Add(llvm::ELF::R_X86_64_32, relocAbs, EncTy_32, 0, false, false, 32)             \
  Add(llvm::ELF::R_X86_64_32S, relocAbs, EncTy_32, 0, false, true, 32)             \
  Add(llvm::ELF::R_X86_64_16, relocAbs, EncTy_16, 0, false, false, 16)             \
  Add(llvm::ELF::R_X86_64_PC16, relocPCRel, EncTy_16, 0, true, true, 16)           \
  Add(llvm::ELF::R_X86_64_8, relocAbs, EncTy_8, 0, false, false, 8)                \
  Add(llvm::ELF::R_X86_64_PC8, relocPCRel, EncTy_8, 0, true, true, 8)              \
  Add(llvm::ELF::R_X86_64_DTPMOD64, unsupported, EncTy_64, 0, false, false, 8)     \
  Add(llvm::ELF::R_X86_64_DTPOFF64, relocDTPOFF, EncTy_64, 0, false, true, 64)     \
  Add(llvm::ELF::R_X86_64_TPOFF64, relocTPOFF, EncTy_64, 0, false, true, 64)       \
  Add(llvm::ELF::R_X86_64_TLSGD, relocGOTRelative, EncTy_32, 0, true, true, 32)    \
  Add(llvm::ELF::R_X86_64_TLSLD, relocGOTRelative, EncTy_32, 0, true, true, 32)    \
  Add(llvm::ELF::R_X86_64_DTPOFF32, relocDTPOFF, EncTy_32, 0, true, true, 32)      \
  Add(llvm::ELF::R_X86_64_GOTTPOFF, relocGOTRelative, EncTy_32, 0, true, true, 32) \
  Add(llvm::ELF::R_X86_64_TPOFF32, relocTPOFF, EncTy_32, 0, true, true, 32)        \
  Add(llvm::ELF::R_X86_64_PC64, relocPCRel, EncTy_64, 0, false, false, 64)         \
  Add(llvm::ELF::R_X86_64_GOTPCRELX, relocGOTRelative, EncTy_32, 0, true, true, 32) \
  Add(llvm::ELF::R_X86_64_REX_GOTPCRELX, relocGOTRelative, EncTy_32, 0, true, true, 32)
// clang-format on

constexpr std::array<RelocationDescriptor, RelocationTableSize>
createRelocationDescriptors() {
  std::array<RelocationDescriptor, RelocationTableSize> Entries{};

#define ELF_RELOC(RelocName, Value) Entries[Value].Name = #RelocName;
#include "llvm/BinaryFormat/ELFRelocs/x86_64.def"
#undef ELF_RELOC

#define ADD_X86_64_RELOC_OVERRIDE(Type, Function, Encoding, ShiftValue,        \
                                  VerifyValue, SignedValue, SizeValue)         \
  Entries[Type].Handler = &Function;                                           \
  Entries[Type].EncType = Encoding;                                            \
  Entries[Type].Shift = ShiftValue;                                            \
  Entries[Type].VerifyRange = VerifyValue;                                     \
  Entries[Type].IsSigned = SignedValue;                                        \
  Entries[Type].Size = SizeValue;
  X86_64_RELOC_OVERRIDES(ADD_X86_64_RELOC_OVERRIDE)
#undef ADD_X86_64_RELOC_OVERRIDE

  return Entries;
}

inline std::array<RelocationDescriptor, RelocationTableSize> Relocs =
    createRelocationDescriptors();

#undef X86_64_RELOC_OVERRIDES

} // namespace x86_64
} // namespace eld

#endif // X86_64_RELOCATION_INFO_H
