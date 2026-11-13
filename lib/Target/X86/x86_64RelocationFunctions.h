//===- x86_64RelocationFunctions.h-----------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//===----------------------------------------------------------------------===//
#ifndef X86_64_RELOCATION_FUNCTIONS_H
#define X86_64_RELOCATION_FUNCTIONS_H

#include "x86_64LLVMExtern.h"
#include "x86_64Relocator.h"

namespace eld {
namespace x86_64 {

// Signature shared by every x86-64 relocation handler.
using RelocationHandler = Relocator::Result (*)(Relocation &pReloc,
                                                x86_64Relocator &pParent);

Relocator::Result none(Relocation &pReloc, x86_64Relocator &pParent);
Relocator::Result relocAbs(Relocation &pReloc, x86_64Relocator &pParent);
Relocator::Result relocPCRel(Relocation &pReloc, x86_64Relocator &pParent);
Relocator::Result relocPLT32(Relocation &pReloc, x86_64Relocator &pParent);
Relocator::Result relocGOTRelative(Relocation &pReloc,
                                   x86_64Relocator &pParent);
Relocator::Result relocTPOFF(Relocation &pReloc, x86_64Relocator &pParent);
Relocator::Result relocDTPOFF(Relocation &pReloc, x86_64Relocator &pParent);
Relocator::Result unsupported(Relocation &pReloc, x86_64Relocator &pParent);

struct RelocationDescriptor {
  const char *Name = "";
  EncodingType EncType = EncTy_None;
  uint32_t Shift = 0;
  bool VerifyRange = false;
  bool IsSigned = false;
  uint32_t Size = 0;
  RelocationHandler Handler = &unsupported;
  bool forceVerify = false;
};

} // namespace x86_64
} // namespace eld

#endif // X86_64_RELOCATION_FUNCTIONS_H
