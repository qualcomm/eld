//===- x86_64RelocationInfo.h----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
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
  Add(llvm::ELF::R_X86_64_NONE, none, false, false, 64)                             \
  Add(llvm::ELF::R_X86_64_64, relocAbs, false, false, 64)                           \
  Add(llvm::ELF::R_X86_64_PC32, relocPCRel, true, true, 32)                         \
  Add(llvm::ELF::R_X86_64_PLT32, relocPLT32, true, true, 32)                        \
  Add(llvm::ELF::R_X86_64_GOTPCREL, relocGOTRelative, true, true, 32)               \
  Add(llvm::ELF::R_X86_64_32, relocAbs, false, false, 32)                           \
  Add(llvm::ELF::R_X86_64_32S, relocAbs, false, true, 32)                           \
  Add(llvm::ELF::R_X86_64_16, relocAbs, false, false, 16)                           \
  Add(llvm::ELF::R_X86_64_PC16, relocPCRel, true, true, 16)                         \
  Add(llvm::ELF::R_X86_64_8, relocAbs, false, false, 8)                             \
  Add(llvm::ELF::R_X86_64_PC8, relocPCRel, true, true, 8)                           \
  Add(llvm::ELF::R_X86_64_DTPOFF64, relocDTPOFF, false, true, 64)                   \
  Add(llvm::ELF::R_X86_64_TPOFF64, relocTPOFF, false, true, 64)                     \
  Add(llvm::ELF::R_X86_64_TLSGD, relocGOTRelative, true, true, 32)                  \
  Add(llvm::ELF::R_X86_64_TLSLD, relocGOTRelative, true, true, 32)                  \
  Add(llvm::ELF::R_X86_64_DTPOFF32, relocDTPOFF, true, true, 32)                    \
  Add(llvm::ELF::R_X86_64_GOTTPOFF, relocGOTRelative, true, true, 32)               \
  Add(llvm::ELF::R_X86_64_TPOFF32, relocTPOFF, true, true, 32)                      \
  Add(llvm::ELF::R_X86_64_PC64, relocPCRel, false, false, 64)                       \
  Add(llvm::ELF::R_X86_64_GOTPCRELX, relocGOTRelative, true, true, 32)              \
  Add(llvm::ELF::R_X86_64_REX_GOTPCRELX, relocGOTRelative, true, true, 32)
// clang-format on

constexpr std::array<RelocationDescriptor, RelocationTableSize>
createRelocationDescriptors() {
  std::array<RelocationDescriptor, RelocationTableSize> Entries{};

#define ELF_RELOC(RelocName, Value) Entries[Value].Name = #RelocName;
#include "llvm/BinaryFormat/ELFRelocs/x86_64.def"
#undef ELF_RELOC

#define ADD_X86_64_RELOC_OVERRIDE(Type, Function, VerifyValue, SignedValue,    \
                                  SizeValue)                                   \
  Entries[Type].Handler = &Function;                                           \
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
