//===- PhdrsCmd.cpp--------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Script/PhdrsCmd.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Core/Module.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// PhdrsCmd
//===----------------------------------------------------------------------===//
PhdrsCmd::PhdrsCmd() : ScriptCommand(ScriptCommand::PHDRS) {}

PhdrsCmd::~PhdrsCmd() {}

void PhdrsCmd::dump(llvm::raw_ostream &Outs) const {
  Outs << "PHDRS\n{\n";

  for (const auto &Elem : MPhdrs) {
    Outs << "\t";
    Elem->dump(Outs);
  }
  Outs << "}\n";
}

void PhdrsCmd::pushBack(ScriptCommand *Cmd) { MPhdrs.push_back(Cmd); }

eld::Expected<void> PhdrsCmd::activate(Module &CurModule) {
  for (auto &Elem : MPhdrs) {
    eld::Expected<void> E = Elem->activate(CurModule);
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(E);
  }
  return eld::Expected<void>();
}

void PhdrsCmd::dumpOnlyThis(llvm::raw_ostream &Outs) const { Outs << "PHDRS"; }
