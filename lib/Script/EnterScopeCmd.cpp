//===- EnterScopeCmd.cpp---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Script/EnterScopeCmd.h"
#include "eld/Core/Module.h"

using namespace eld;

EnterScopeCmd::EnterScopeCmd() : ScriptCommand(ScriptCommand::ENTER_SCOPE) {}

eld::Expected<void> EnterScopeCmd::activate(Module &CurModule) {
  return eld::Expected<void>();
}

void EnterScopeCmd::dump(llvm::raw_ostream &Outs) const {
  Outs << "{"
       << "\n";
}

void EnterScopeCmd::dumpOnlyThis(llvm::raw_ostream &Outs) const {
  Outs << " ";
  Outs << "{"
       << "\n";
}
