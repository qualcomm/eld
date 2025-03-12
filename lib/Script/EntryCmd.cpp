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
EntryCmd::EntryCmd(const std::string &pEntry)
    : ScriptCommand(ScriptCommand::ENTRY), m_Entry(pEntry) {}

EntryCmd::~EntryCmd() {}

void EntryCmd::dump(llvm::raw_ostream &outs) const {
  outs << "ENTRY(" << m_Entry << ")";
  outs << " # " << getContext();
  outs << "\n";
}

eld::Expected<void> EntryCmd::activate(Module &pModule) {
  if ((!m_Entry.empty()) && (!(pModule.getConfig().options().hasEntry())))
    pModule.getConfig().options().setEntry(m_Entry);
  return eld::Expected<void>();
}
