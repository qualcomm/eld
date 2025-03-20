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

void MemoryCmd::dump(llvm::raw_ostream &Outs) const {
  Outs << "MEMORY\n{\n";
  for (const auto &Elem : MemoryDescriptors) {
    Outs << "\t";
    Elem->dump(Outs);
  }
  Outs << "}\n";
}

void MemoryCmd::pushBack(ScriptCommand *Desc) {
  MemoryDescriptors.push_back(llvm::cast<MemoryDesc>(Desc));
}

eld::Expected<void> MemoryCmd::activate(Module &CurModule) {
  for (auto &Elem : MemoryDescriptors) {
    eld::Expected<void> E = Elem->activate(CurModule);
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(E);
  }
  CurModule.getScript().setMemoryCommand(this);
  return eld::Expected<void>();
}

void MemoryCmd::dumpOnlyThis(llvm::raw_ostream &Outs) const {
  Outs << "MEMORY";
}
