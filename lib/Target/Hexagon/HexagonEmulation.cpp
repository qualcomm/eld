//===- HexagonEmulation.cpp------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "Hexagon.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Support/MsgHandling.h"
#include "eld/Support/TargetRegistry.h"
#include "eld/Target/ELFEmulation.h"
#include "llvm/ADT/StringSwitch.h"

using namespace llvm;

namespace eld {

static bool ELDEmulateHexagonELF(LinkerScript &pScript, LinkerConfig &pConfig) {
  // set up bitclass and endian
  pConfig.targets().setEndian(TargetOptions::Little);
  pConfig.targets().setBitClass(32);
  if (!pConfig.options().getEmulation().empty()) {
    StringRef flag =
        llvm::StringSwitch<StringRef>(pConfig.options().getEmulation())
            .Case("v68", "hexagonv68")
            .Case("v69", "hexagonv69")
            .Case("v71", "hexagonv71")
            .Case("v71t", "hexagonv71t")
            .Case("v73", "hexagonv73")
            .Case("v75", "hexagonv75")
            .Case("v77", "hexagonv77")
            .Case("v79", "hexagonv79")
            .Case("v81", "hexagonv81")
            .Case("v83", "hexagonv83")
            .Case("v85", "hexagonv85")
            .Default("invalid");
    if (flag == "deprecated") {
      pConfig.raise(diag::deprecated_emulation)
          << pConfig.options().getEmulation();
      return false;
    }
    if (flag == "invalid") {
      pConfig.raise(diag::fatal_unsupported_emulation)
          << pConfig.options().getEmulation();
      return false;
    }
    pConfig.targets().setTargetCPU(flag.str());
  }
  if (LinkerConfig::DynObj == pConfig.codeGenType())
    pConfig.options().setGPSize(0);

  // Always show warnings on Hexagon unless user
  // used -Wno-linker-script
  if (!pConfig.hasShowLinkerScriptWarnings())
    pConfig.setShowLinkerScriptWarning(true);

  if (!ELDEmulateELF(pScript, pConfig))
    return false;

  return true;
}

//===----------------------------------------------------------------------===//
// emulateHexagonLD - the help function to emulate Hexagon ld
//===----------------------------------------------------------------------===//
bool emulateHexagonLD(LinkerScript &pScript, LinkerConfig &pConfig) {
  return ELDEmulateHexagonELF(pScript, pConfig);
}

} // namespace eld

//===----------------------------------------------------------------------===//
// HexagonEmulation
//===----------------------------------------------------------------------===//
extern "C" void ELDInitializeHexagonEmulation() {
  // Register the emulation
  eld::TargetRegistry::RegisterEmulation(eld::TheHexagonTarget,
                                         eld::emulateHexagonLD);
}
