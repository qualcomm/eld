//===- ExitScopeCmd.cpp----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Script/ExitScopeCmd.h"
#include "eld/Core/Module.h"
#include "eld/Script/OutputSectDesc.h"

using namespace eld;

ExitScopeCmd::ExitScopeCmd() : ScriptCommand(ScriptCommand::EXIT_SCOPE) {}

eld::Expected<void> ExitScopeCmd::activate(Module &CurModule) {
  return eld::Expected<void>();
}

void ExitScopeCmd::dump(llvm::raw_ostream &Outs) const {
  Outs << "}";
  Outs << "\n";
}

void ExitScopeCmd::dumpOnlyThis(llvm::raw_ostream &Outs) const {
  doIndent(Outs);
  Outs << "}";
  if (getParent() && getParent()->isOutputSectionDescription()) {
    auto *Cmd = llvm::dyn_cast<eld::OutputSectDesc>(getParent());
    Outs << " ";
    Cmd->dumpEpilogue(Outs);
  }
  Outs << "\n";
}

uint32_t ExitScopeCmd::getDepth() const {
  return ScriptCommand::getDepth() - 1;
}
