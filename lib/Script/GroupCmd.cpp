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
GroupCmd::GroupCmd(const LinkerConfig &Config, StringList &PStringList,
                   const Attribute &Attribute, ScriptFile &ScriptFile)
    : ScriptCommand(ScriptCommand::GROUP), ThisStringList(PStringList),
      ThisBuilder(Config, Attribute), ThisScriptFile(ScriptFile) {}

GroupCmd::~GroupCmd() {}

void GroupCmd::dump(llvm::raw_ostream &Outs) const {
  Outs << "GROUP(";
  bool Prev = false, Cur = false;
  bool HasInput = false;
  for (auto &S : ThisStringList) {
    InputToken *Input = llvm::cast<InputToken>(S);
    Cur = Input->asNeeded();
    if (!Prev && Cur)
      Outs << "AS_NEEDED(";
    else if (Prev && !Cur)
      Outs << ")";
    if (HasInput)
      Outs << " ";
    if (Input->type() == InputToken::NameSpec)
      Outs << "-l";
    Outs << Input->name();
    HasInput = true;
    Prev = Cur;
  }

  if (!ThisStringList.empty() && Prev)
    Outs << ")";

  Outs << ")\n";
}

eld::Expected<void> GroupCmd::activate(Module &CurModule) {
  // --start-group
  ThisBuilder.enterGroup();

  for (auto &S : ThisStringList) {
    InputToken *Token = llvm::cast<InputToken>(S);
    Token = ThisScriptFile.findResolvedFilename(Token);
    if (Token->asNeeded())
      ThisBuilder.getAttributes().setAsNeeded();
    else
      ThisBuilder.getAttributes().unsetAsNeeded();
    switch (Token->type()) {
    case InputToken::File:
      ThisBuilder.createDeferredInput(Token->name(), Input::Script);
      break;
    case InputToken::NameSpec: {
      ThisBuilder.createDeferredInput(Token->name(), Input::Namespec);
      break;
    }
    default:
      break;
    }
  }

  // --end-group
  ThisBuilder.exitGroup();

  // Add all inputs that were read from the script to the input itself.
  for (auto &N : ThisBuilder.getTree())
    ThisScriptFile.getLinkerScriptFile().addNode(N);

  ThisBuilder.getTree().clear();
  return eld::Expected<void>();
}
