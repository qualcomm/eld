//===- SearchDirCmd.cpp----------------------------------------------------===//
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

#include "eld/Script/SearchDirCmd.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Core/Module.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// SearchDirCmd
//===----------------------------------------------------------------------===//
SearchDirCmd::SearchDirCmd(const std::string &PPath)
    : ScriptCommand(ScriptCommand::SEARCH_DIR), MPath(PPath) {}

SearchDirCmd::~SearchDirCmd() {}

void SearchDirCmd::dump(llvm::raw_ostream &Outs) const {
  Outs << "SEARCH_DIR(\"" << MPath << "\");\n";
}

eld::Expected<void> SearchDirCmd::activate(Module &CurModule) {
  CurModule.getConfig().directories().insert(MPath);
  return eld::Expected<void>();
}
