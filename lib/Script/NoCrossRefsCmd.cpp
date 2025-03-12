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
NoCrossRefsCmd::NoCrossRefsCmd(StringList &pSections, size_t ID)
    : ScriptCommand(ScriptCommand::NOCROSSREFS), m_Sections(pSections),
      m_ID(ID) {}

NoCrossRefsCmd::~NoCrossRefsCmd() {}

void NoCrossRefsCmd::dump(llvm::raw_ostream &outs) const {
  outs << "NOCROSSREFS(";
  bool isFirst = true;
  for (auto &I : m_Sections) {
    if (isFirst)
      isFirst = false;
    else
      outs << ", ";
    outs << I->name();
  }
  outs << ");\n";
}

eld::Expected<void> NoCrossRefsCmd::activate(Module &pModule) {
  StringList::iterator it, ite = m_Sections.end();
  for (it = m_Sections.begin(); it != ite; it++) {
    std::string name = (*it)->name();
    pModule.getNonRefSections().emplace(std::make_pair(name, m_ID));
  }
  return eld::Expected<void>();
}
