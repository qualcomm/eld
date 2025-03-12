//===- MemoryCmd.cpp-------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Script/MemoryCmd.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Core/Module.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// MemoryCmd
//===----------------------------------------------------------------------===//
MemoryCmd::MemoryCmd() : ScriptCommand(ScriptCommand::MEMORY) {}

MemoryCmd::~MemoryCmd() {}

void MemoryCmd::dump(llvm::raw_ostream &outs) const {
  outs << "MEMORY\n{\n";
  for (const auto &elem : m_MemoryDescriptors) {
    outs << "\t";
    elem->dump(outs);
  }
  outs << "}\n";
}

void MemoryCmd::push_back(ScriptCommand *Desc) {
  m_MemoryDescriptors.push_back(llvm::cast<MemoryDesc>(Desc));
}

eld::Expected<void> MemoryCmd::activate(Module &pModule) {
  for (auto &elem : m_MemoryDescriptors) {
    eld::Expected<void> E = elem->activate(pModule);
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(E);
  }
  pModule.getScript().setMemoryCommand(this);
  return eld::Expected<void>();
}

void MemoryCmd::dumpOnlyThis(llvm::raw_ostream &outs) const {
  outs << "MEMORY";
}
