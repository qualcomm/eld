//===- ScriptCommand.cpp---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

//===- ScriptCommand.cpp --------------------------------------------------===//
//===----------------------------------------------------------------------===//
#include "eld/Script/ScriptCommand.h"
#include "eld/Input/Input.h"
#include "eld/Input/InputFile.h"
#include <string>

using namespace eld;

//===----------------------------------------------------------------------===//
// ScriptCommand
//===----------------------------------------------------------------------===//
ScriptCommand::~ScriptCommand() {}
void ScriptCommand::dumpMap(llvm::raw_ostream &Ostream, bool Color,
                            bool UseNewLine, bool WithValues,
                            bool AddIndent) const {
  if (Color)
    Ostream.changeColor(llvm::raw_ostream::CYAN);
  dump(Ostream);
  if (UseNewLine)
    Ostream << "\n";
  if (Color)
    Ostream.resetColor();
}

uint32_t ScriptCommand::getDepth() const {
  uint32_t Depth = 0;
  eld::ScriptCommand *Parent = ParentScriptCommand;
  while (Parent != nullptr) {
    Parent = Parent->getParent();
    ++Depth;
  }
  return Depth;
}

void ScriptCommand::doIndent(llvm::raw_ostream &Outs) const {
  for (uint32_t I = 0; I < getDepth(); ++I)
    Outs << "  ";
}

std::string ScriptCommand::getContext() const {
  return ThisScriptFile->getInput()->decoratedPath() +
         (hasLineNumberInContext()
              ? ":" + std::to_string(getLineNumberInContext())
              : "");
}
