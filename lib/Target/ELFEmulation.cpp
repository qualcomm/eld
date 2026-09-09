//===- ELFEmulation.cpp----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#include "eld/Target/ELFEmulation.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Input/ELDDirectory.h"
#include "llvm/Support/FileSystem.h"

using namespace eld;

bool eld::ELDEmulateELF(LinkerScript &pScript, LinkerConfig &pConfig) {
  if (pConfig.options().nostdlib())
    return true;
  auto Sysroot = pConfig.directories().hasSysRoot()
                     ? pConfig.directories().sysroot().native()
                     : "";
  static const char *StandardDirs[] = {
      "=/lib",
      "=/usr/lib",
      "=/usr/local/lib",
  };
  for (const char *Dir : StandardDirs) {
    auto EldDir = ELDDirectory(Dir, Sysroot);
    if (llvm::sys::fs::is_directory(EldDir.name()))
      pConfig.directories().insert(Dir);
  }
  return true;
}