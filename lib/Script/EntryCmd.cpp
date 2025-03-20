//===- EntryCmd.cpp--------------------------------------------------------===//
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

#include "eld/Script/EntryCmd.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Core/Module.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// EntryCmd
//===----------------------------------------------------------------------===//
EntryCmd::EntryCmd(const std::string &PEntry)
    : ScriptCommand(ScriptCommand::ENTRY), EntrySymbol(PEntry) {}

EntryCmd::~EntryCmd() {}

void EntryCmd::dump(llvm::raw_ostream &Outs) const {
  Outs << "ENTRY(" << EntrySymbol << ")";
  Outs << " # " << getContext();
  Outs << "\n";
}

eld::Expected<void> EntryCmd::activate(Module &CurModule) {
  if ((!EntrySymbol.empty()) && (!(CurModule.getConfig().options().hasEntry())))
    CurModule.getConfig().options().setEntry(EntrySymbol);
  return eld::Expected<void>();
}
