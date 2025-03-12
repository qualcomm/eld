//===- TemplateEmulation.cpp-----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "Template.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Support/TargetRegistry.h"
#include "eld/Target/ELFEmulation.h"
#include "llvm/ADT/StringSwitch.h"

using namespace llvm;

namespace eld {

static bool ELDEmulateTemplateELF(LinkerScript &pScript,
                                  LinkerConfig &pConfig) {
  // Set Endianess and BitClass(32/64).
  pConfig.targets().setEndian(TargetOptions::Little);
  pConfig.targets().setBitClass(32);

  if (!ELDEmulateELF(pScript, pConfig))
    return false;

  return true;
}

//===----------------------------------------------------------------------===//
// emulateTemplateLD - the help function to emulate Template ld
//===----------------------------------------------------------------------===//
bool emulateTemplateLD(LinkerScript &pScript, LinkerConfig &pConfig) {
  return ELDEmulateTemplateELF(pScript, pConfig);
}

} // namespace eld

//===----------------------------------------------------------------------===//
// TemplateEmulation
//===----------------------------------------------------------------------===//
extern "C" void ELDInitializeTemplateEmulation() {
  // Register the emulation
  eld::TargetRegistry::RegisterEmulation(eld::TheTemplateTarget,
                                         eld::emulateTemplateLD);
}
