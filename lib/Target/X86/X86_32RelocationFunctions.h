//===- X86_32RelocationFunctions.h---------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#ifndef ELD_TARGET_X86_32_RELOCATION_FUNCTIONS_H
#define ELD_TARGET_X86_32_RELOCATION_FUNCTIONS_H

#include "X86_32Relocator.h"

namespace eld {
namespace x86_32 {

using RelocationHandler = Relocator::Result (*)(Relocation &pReloc,
                                                X86_32Relocator &pParent);

// R_386_NONE.
Relocator::Result none(Relocation &pReloc, X86_32Relocator &pParent);
// R_386_8, R_386_16, and R_386_32: S + A.
Relocator::Result relocAbs(Relocation &pReloc, X86_32Relocator &pParent);
// Any relocation not implemented by the current i386 backend.
Relocator::Result unsupported(Relocation &pReloc, X86_32Relocator &pParent);

} // namespace x86_32
} // namespace eld

#endif
