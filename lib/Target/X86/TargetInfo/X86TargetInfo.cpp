//===- X86TargetInfo.cpp---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Support/Target.h"
#include "eld/Support/TargetRegistry.h"
#include "llvm/Object/ELF.h"

namespace eld {

eld::Target Thex86_64Target;
eld::Target TheX86_32Target;

extern "C" void ELDInitializeX86LDTargetInfo() {
  // register into eld::TargetRegistry
  eld::RegisterTarget X86_32(TheX86_32Target, "i386", llvm::ELF::EM_386, false);
  eld::RegisterTarget X(Thex86_64Target, "x86_64", llvm::ELF::EM_X86_64, true);
}

} // namespace eld
