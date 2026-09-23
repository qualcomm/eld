//===- X86_32Emulation.cpp-----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "X86.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Target/ELFEmulation.h"

namespace eld {

bool emulateX86_32LD(LinkerScript &pScript, LinkerConfig &pConfig) {
  pConfig.targets().setEndian(TargetOptions::Little);
  pConfig.targets().setBitClass(32);
  return ELDEmulateELF(pScript, pConfig);
}

} // namespace eld
