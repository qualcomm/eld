//===- NoCrossRefsCmd.cpp--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Script/NoCrossRefsCmd.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Core/Module.h"
#include "eld/Support/MsgHandling.h"
#include "eld/SymbolResolver/NamePool.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// NoCrossRefsCmd
//===----------------------------------------------------------------------===//
NoCrossRefsCmd::NoCrossRefsCmd(StringList &PSections, size_t ID)
    : ScriptCommand(ScriptCommand::NOCROSSREFS), ThisSectionions(PSections),
      CurID(ID) {}

NoCrossRefsCmd::~NoCrossRefsCmd() {}

void NoCrossRefsCmd::dump(llvm::raw_ostream &Outs) const {
  Outs << "NOCROSSREFS(";
  bool IsFirst = true;
  for (auto &I : ThisSectionions) {
    if (IsFirst)
      IsFirst = false;
    else
      Outs << ", ";
    Outs << I->name();
  }
  Outs << ");\n";
}

eld::Expected<void> NoCrossRefsCmd::activate(Module &CurModule) {
  StringList::iterator It, Ite = ThisSectionions.end();
  for (It = ThisSectionions.begin(); It != Ite; It++) {
    std::string Name = (*It)->name();
    CurModule.getNonRefSections().emplace(std::make_pair(Name, CurID));
  }
  return eld::Expected<void>();
}
