//===- TargetRegistry.cpp--------------------------------------------------===//
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

//===- TargetRegistry.cpp -------------------------------------------------===//
//===----------------------------------------------------------------------===//
#include "eld/Support/TargetRegistry.h"

using namespace eld;

std::vector<Target *> eld::TargetRegistry::TargetList;

void TargetRegistry::RegisterTarget(Target &Target, llvm::StringRef Name,
                                    uint16_t Machine, bool is64bit) {
  Target.Name = Name;
  Target.Machine = Machine;
  Target.Is64bit = is64bit;
  TargetList.push_back(&Target);
}

const Target *TargetRegistry::lookupTarget(llvm::StringRef ArchName,
                                           llvm::Triple &Triple,
                                           std::string &Error) {
  const Target *Result = nullptr;
  for (auto &target : targets()) {
    if (ArchName == target->name()) {
      Result = target;
      break;
    }
  }

  if (!Result) {
    Error = std::string("invalid target '") + ArchName.str() + "'.\n";
    return nullptr;
  }

  // Adjust the triple to match (if known), otherwise stick with the
  // module/host triple.
  llvm::Triple::ArchType Type = llvm::Triple::getArchTypeForLLVMName(ArchName);
  if (llvm::Triple::UnknownArch != Type)
    Triple.setArch(Type);
  return Result;
}

Target *TargetRegistry::lookupMachine(uint16_t Machine, bool is64bit) const {
  for (auto &target : targets()) {
    if ((target->Machine == Machine) && (target->Is64bit == is64bit))
      return target;
  }
  return nullptr;
}
