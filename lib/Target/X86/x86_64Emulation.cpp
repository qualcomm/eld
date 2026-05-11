//===- x86_64Emulation.cpp-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//===----------------------------------------------------------------------===//
#include "eld/Config/LinkerConfig.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Input/ELDDirectory.h"
#include "eld/Support/TargetRegistry.h"
#include "eld/Target/ELFEmulation.h"
#include "x86_64.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/FileSystem.h"

using namespace llvm;

namespace eld {

static bool ELDEmulatex86_64ELF(LinkerScript &pScript, LinkerConfig &pConfig) {
  // Set Endianness and BitClass(32/64).
  pConfig.targets().setEndian(TargetOptions::Little);
  pConfig.targets().setBitClass(64);

  if (!ELDEmulateELF(pScript, pConfig))
    return false;

  if (pConfig.options().nostdlib())
    return true;

  auto Sysroot = pConfig.directories().hasSysRoot()
                     ? pConfig.directories().sysroot().native()
                     : "";

  static const char *X86_64Dirs[] = {
      "=/usr/local/lib/x86_64-linux-gnu",
      "=/lib/x86_64-linux-gnu",
      "=/usr/lib/x86_64-linux-gnu",
      "=/usr/lib/x86_64-linux-gnu64",
      "=/usr/local/lib64",
      "=/lib64",
      "=/usr/lib64",
      "=/usr/x86_64-linux-gnu/lib64",
      "=/usr/x86_64-linux-gnu/lib",
  };
  for (const char *Dir : X86_64Dirs) {
    auto EldDir = ELDDirectory(Dir, Sysroot);
    if (llvm::sys::fs::is_directory(EldDir.name()))
      pConfig.directories().insert(Dir);
  }
  return true;
}

//===----------------------------------------------------------------------===//
// emulatex86_64LD - the help function to emulate x86_64 ld
//===----------------------------------------------------------------------===//
bool emulatex86_64LD(LinkerScript &pScript, LinkerConfig &pConfig) {
  return ELDEmulatex86_64ELF(pScript, pConfig);
}

} // namespace eld

//===----------------------------------------------------------------------===//
// x86_64Emulation
//===----------------------------------------------------------------------===//
extern "C" void ELDInitializeX86Emulation() {
  // Register the emulation
  eld::TargetRegistry::RegisterEmulation(eld::Thex86_64Target,
                                         eld::emulatex86_64LD);
}