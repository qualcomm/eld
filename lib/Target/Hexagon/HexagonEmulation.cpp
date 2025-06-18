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
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"

using namespace llvm;

namespace eld {

static bool ELDEmulateHexagonELF(LinkerScript &pScript, LinkerConfig &pConfig) {
  // set up bitclass and endian
  pConfig.targets().setEndian(TargetOptions::Little);
  pConfig.targets().setBitClass(32);
  llvm::StringRef Emulation = pConfig.options().getEmulation();
  if (!Emulation.empty()) {
    llvm::StringRef flag = llvm::StringSwitch<StringRef>(Emulation)
                               .Cases("v68", "hexagonelf", "hexagonv68")
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
                               .Case("v87", "hexagonv87")
                               .Case("v89", "hexagonv89")
                               .Default("invalid");
    if (flag == "deprecated") {
      pConfig.raise(Diag::deprecated_emulation)
          << pConfig.options().getEmulation();
      return false;
    }
    if (flag == "invalid") {
      pConfig.raise(Diag::fatal_unsupported_emulation)
          << pConfig.options().getEmulation();
      return false;
    }
    TargetOptions &TargetOpts = pConfig.targets();
    const std::string &CurTargetCPU = TargetOpts.getTargetCPU();
    // -m hexagonelf -mcpu hexagonvx should be allowed and
    // -mcpu should be given priority over hexagonelf.
    if (Emulation != "hexagonelf" ||
        (Emulation == "hexagonelf" && CurTargetCPU.empty()))
      pConfig.targets().setTargetCPU(flag.str());
  }
  if (LinkerConfig::DynObj == pConfig.codeGenType())
    pConfig.options().setGPSize(0);

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
