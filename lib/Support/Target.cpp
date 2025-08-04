//===- Target.cpp----------------------------------------------------------===//
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

#include "eld/Support/Target.h"
#include "eld/Config/LinkerConfig.h"
#include "llvm/TargetParser/Triple.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// Target
//===----------------------------------------------------------------------===//
Target::Target() {}

/// emulate - given MCLinker default values for the other aspects of the
/// target system.
bool Target::emulate(LinkerScript &Script, LinkerConfig &Config) const {
  if (nullptr == EmulationFn)
    return false;
  if (!Config.targets().hasArch())
    Config.targets().setArch(std::string(Name));
  return EmulationFn(Script, Config);
}

/// createLDBackend - create target-specific LDBackend
GNULDBackend *Target::createLDBackend(Module &Module) const {
  if (nullptr == GNULDBackendCtorFn)
    return nullptr;
  return GNULDBackendCtorFn(Module);
}
