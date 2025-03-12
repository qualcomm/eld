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

void PhdrsCmd::dump(llvm::raw_ostream &outs) const {
  outs << "PHDRS\n{\n";

  for (const auto &elem : m_Phdrs) {
    outs << "\t";
    elem->dump(outs);
  }
  outs << "}\n";
}

void PhdrsCmd::push_back(ScriptCommand *Cmd) { m_Phdrs.push_back(Cmd); }

eld::Expected<void> PhdrsCmd::activate(Module &pModule) {
  for (auto &elem : m_Phdrs) {
    eld::Expected<void> E = elem->activate(pModule);
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(E);
  }
  return eld::Expected<void>();
}

void PhdrsCmd::dumpOnlyThis(llvm::raw_ostream &outs) const { outs << "PHDRS"; }
