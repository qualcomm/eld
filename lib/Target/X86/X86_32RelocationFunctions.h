//===- X86_32RelocationFunctions.h---------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#ifndef ELD_TARGET_X86_32_RELOCATION_FUNCTIONS_H
#define ELD_TARGET_X86_32_RELOCATION_FUNCTIONS_H

#include "X86_32RelocationInfo.h"
#include "X86_32Relocator.h"
#include "llvm/BinaryFormat/ELF.h"

namespace eld {
namespace x86_32 {

using ApplyFunctionType = Relocator::Result (*)(Relocation &pReloc,
                                                X86_32Relocator &pParent);

// R_386_NONE.
Relocator::Result none(Relocation &pReloc, X86_32Relocator &pParent);
// R_386_8, R_386_16, and R_386_32: S + A.
Relocator::Result relocAbs(Relocation &pReloc, X86_32Relocator &pParent);
// R_386_PC8, R_386_PC16, and R_386_PC32: S + A - P.
Relocator::Result relocPCRel(Relocation &pReloc, X86_32Relocator &pParent);
// Any relocation not implemented by the current i386 backend.
Relocator::Result unsupported(Relocation &pReloc, X86_32Relocator &pParent);

inline constexpr ApplyFunctionType RelocDesc[] = {
    &none,        // R_386_NONE
    &relocAbs,    // R_386_32
    &relocPCRel,  // R_386_PC32
    &unsupported, // R_386_GOT32
    &unsupported, // R_386_PLT32
    &unsupported, // R_386_COPY
    &unsupported, // R_386_GLOB_DAT
    &unsupported, // R_386_JUMP_SLOT
    &unsupported, // R_386_RELATIVE
    &unsupported, // R_386_GOTOFF
    &unsupported, // R_386_GOTPC
    &unsupported, // R_386_32PLT
    &unsupported, // R_386_RESERVED_12
    &unsupported, // R_386_RESERVED_13
    &unsupported, // R_386_TLS_TPOFF
    &unsupported, // R_386_TLS_IE
    &unsupported, // R_386_TLS_GOTIE
    &unsupported, // R_386_TLS_LE
    &unsupported, // R_386_TLS_GD
    &unsupported, // R_386_TLS_LDM
    &relocAbs,    // R_386_16
    &relocPCRel,  // R_386_PC16
    &relocAbs,    // R_386_8
    &relocPCRel,  // R_386_PC8
    &unsupported, // R_386_TLS_GD_32
    &unsupported, // R_386_TLS_GD_PUSH
    &unsupported, // R_386_TLS_GD_CALL
    &unsupported, // R_386_TLS_GD_POP
    &unsupported, // R_386_TLS_LDM_32
    &unsupported, // R_386_TLS_LDM_PUSH
    &unsupported, // R_386_TLS_LDM_CALL
    &unsupported, // R_386_TLS_LDM_POP
    &unsupported, // R_386_TLS_LDO_32
    &unsupported, // R_386_TLS_IE_32
    &unsupported, // R_386_TLS_LE_32
    &unsupported, // R_386_TLS_DTPMOD32
    &unsupported, // R_386_TLS_DTPOFF32
    &unsupported, // R_386_TLS_TPOFF32
    &unsupported, // R_386_RESERVED_38
    &unsupported, // R_386_TLS_GOTDESC
    &unsupported, // R_386_TLS_DESC_CALL
    &unsupported, // R_386_TLS_DESC
    &unsupported, // R_386_IRELATIVE
    &unsupported, // R_386_GOT32X
};

static_assert(sizeof(RelocDesc) / sizeof(RelocDesc[0]) == x86_32::MaxRelocs,
              "i386 relocation tables must have the same number of entries");

} // namespace x86_32
} // namespace eld

#endif
