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
void ScriptCommand::dumpMap(llvm::raw_ostream &ostream, bool color,
                            bool useNewLine, bool withValues,
                            bool addIndent) const {
  if (color)
    ostream.changeColor(llvm::raw_ostream::CYAN);
  dump(ostream);
  if (useNewLine)
    ostream << "\n";
  if (color)
    ostream.resetColor();
}

uint32_t ScriptCommand::getDepth() const {
  uint32_t Depth = 0;
  eld::ScriptCommand *Parent = m_Parent;
  while (Parent != nullptr) {
    Parent = Parent->getParent();
    ++Depth;
  }
  return Depth;
}

void ScriptCommand::doIndent(llvm::raw_ostream &outs) const {
  for (uint32_t i = 0; i < getDepth(); ++i)
    outs << "  ";
}

std::string ScriptCommand::getContext() const {
  return m_ScriptFile->getInput()->decoratedPath() +
         (hasLineNumberInContext()
              ? ":" + std::to_string(getLineNumberInContext())
              : "");
}
