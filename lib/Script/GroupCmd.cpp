//===- GroupCmd.cpp--------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Script/GroupCmd.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Core/Module.h"
#include "eld/Input/InputBuilder.h"
#include "eld/Input/InputTree.h"
#include "eld/Input/LinkerScriptFile.h"
#include "eld/Script/InputToken.h"
#include "eld/Script/StringList.h"
#include "eld/Support/MsgHandling.h"
#include "eld/Support/Path.h"
#include "llvm/Support/Casting.h"
#include <vector>

using namespace eld;

//===----------------------------------------------------------------------===//
// GroupCmd
//===----------------------------------------------------------------------===//
GroupCmd::GroupCmd(const LinkerConfig &config, StringList &pStringList,
                   const Attribute &attribute, ScriptFile &scriptFile)
    : ScriptCommand(ScriptCommand::GROUP), m_StringList(pStringList),
      m_InputBuilder(config, attribute), m_ScriptFile(scriptFile) {}

GroupCmd::~GroupCmd() {}

void GroupCmd::dump(llvm::raw_ostream &outs) const {
  outs << "GROUP(";
  bool prev = false, cur = false;
  bool hasInput = false;
  for (auto &S : m_StringList) {
    InputToken *input = llvm::cast<InputToken>(S);
    cur = input->asNeeded();
    if (!prev && cur)
      outs << "AS_NEEDED(";
    else if (prev && !cur)
      outs << ")";
    if (hasInput)
      outs << " ";
    if (input->type() == InputToken::NameSpec)
      outs << "-l";
    outs << input->name();
    hasInput = true;
    prev = cur;
  }

  if (!m_StringList.empty() && prev)
    outs << ")";

  outs << ")\n";
}

eld::Expected<void> GroupCmd::activate(Module &pModule) {
  // --start-group
  m_InputBuilder.enterGroup();

  for (auto &S : m_StringList) {
    InputToken *token = llvm::cast<InputToken>(S);
    if (token->asNeeded())
      m_InputBuilder.getAttributes().setAsNeeded();
    else
      m_InputBuilder.getAttributes().unsetAsNeeded();
    switch (token->type()) {
    case InputToken::File:
      m_InputBuilder.createDeferredInput(token->name(), Input::Script);
      break;
    case InputToken::NameSpec: {
      m_InputBuilder.createDeferredInput(token->name(), Input::Namespec);
      break;
    }
    default:
      break;
    }
  }

  // --end-group
  m_InputBuilder.exitGroup();

  // Add all inputs that were read from the script to the input itself.
  for (auto &N : m_InputBuilder.getTree())
    m_ScriptFile.getLinkerScriptFile().addNode(N);

  m_InputBuilder.getTree().clear();
  return eld::Expected<void>();
}
