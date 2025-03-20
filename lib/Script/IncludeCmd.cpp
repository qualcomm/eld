//===- IncludeCmd.cpp------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Script/IncludeCmd.h"
#include "llvm/Support/Casting.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// IncludeCmd
//===----------------------------------------------------------------------===//
IncludeCmd::IncludeCmd(const std::string FileName, bool IsOptional)
    : ScriptCommand(ScriptCommand::INCLUDE), FileName(FileName),
      IsOptional(IsOptional) {}

void IncludeCmd::dump(llvm::raw_ostream &Outs) const {
  if (IsOptional)
    Outs << "INCLUDE_OPTIONAL";
  else
    Outs << "INCLUDE";
  Outs << " ";
  Outs << FileName;
  Outs << "\n";
}

void IncludeCmd::dumpOnlyThis(llvm::raw_ostream &Outs) const {
  if (IsOptional)
    Outs << "INCLUDE_OPTIONAL";
  else
    Outs << "INCLUDE";
  Outs << " ";
  Outs << FileName;
}

eld::Expected<void> IncludeCmd::activate(Module &CurModule) {
  return eld::Expected<void>();
}
