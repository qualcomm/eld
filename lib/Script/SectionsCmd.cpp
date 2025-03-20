//===- SectionsCmd.cpp-----------------------------------------------------===//
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
#include "eld/Script/SectionsCmd.h"
#include "eld/Script/Assignment.h"
#include "eld/Support/MsgHandling.h"
#include "llvm/Support/Casting.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// SectionsCmd
//===----------------------------------------------------------------------===//
SectionsCmd::SectionsCmd() : ScriptCommand(ScriptCommand::SECTIONS) {}

SectionsCmd::~SectionsCmd() {}

void SectionsCmd::dump(llvm::raw_ostream &Outs) const {
  Outs << "SECTIONS\n{\n";

  for (const auto &Elem : *this) {
    switch ((Elem)->getKind()) {
    case ScriptCommand::ENTRY:
    case ScriptCommand::ASSIGNMENT:
    case ScriptCommand::OUTPUT_SECT_DESC:
    case ScriptCommand::ASSERT:
      Outs << "\t";
      (Elem)->dump(Outs);
      break;
    case ScriptCommand::INCLUDE:
    case ScriptCommand::ENTER_SCOPE:
    case ScriptCommand::EXIT_SCOPE:
      break;
    default:
      assert(0);
      break;
    }
  }

  Outs << "}\n";
}

void SectionsCmd::dumpOnlyThis(llvm::raw_ostream &Outs) const {
  Outs << "SECTIONS";
}

void SectionsCmd::pushBack(ScriptCommand *PCommand) {
  switch (PCommand->getKind()) {
  case ScriptCommand::ENTRY:
  case ScriptCommand::ENTER_SCOPE:
  case ScriptCommand::EXIT_SCOPE:
  case ScriptCommand::ASSIGNMENT:
  case ScriptCommand::OUTPUT_SECT_DESC:
  case ScriptCommand::ASSERT:
  case ScriptCommand::INCLUDE:
    ThisSectionCommands.push_back(PCommand);
    break;
  default:
    break;
  }
}

eld::Expected<void> SectionsCmd::activate(Module &CurModule) {
  // Assignment between output sections
  SectionCommands Assignments;

  for (const_iterator It = begin(), Ie = end(); It != Ie; ++It) {
    switch ((*It)->getKind()) {
    case ScriptCommand::ENTRY:
      (*It)->activate(CurModule);
      break;
    case ScriptCommand::ASSIGNMENT:
      Assignments.push_back(*It);
      break;
    case ScriptCommand::ASSERT:
      (*It)->activate(CurModule);
      break;
    case ScriptCommand::OUTPUT_SECT_DESC: {
      if (!Assignments.empty()) {
        iterator Assign, AssignEnd = Assignments.end();
        for (Assign = Assignments.begin(); Assign != AssignEnd; ++Assign) {
          llvm::cast<Assignment>(*Assign)->setLevel(Assignment::SECTIONS_END);
          (*Assign)->activate(CurModule);
        }
        Assignments.clear();
      }
      eld::Expected<void> E = (*It)->activate(CurModule);
      ELDEXP_RETURN_DIAGENTRY_IF_ERROR(E);
      break;
    }
    case ScriptCommand::INCLUDE:
    case ScriptCommand::ENTER_SCOPE:
    case ScriptCommand::EXIT_SCOPE:
      break;
    default:
      assert(0);
      break;
    }
  }
  // The assignment may be the last set too.
  if (!Assignments.empty()) {
    iterator Assign, AssignEnd = Assignments.end();
    for (Assign = Assignments.begin(); Assign != AssignEnd; ++Assign) {
      llvm::cast<Assignment>(*Assign)->setLevel(Assignment::SECTIONS_END);
      eld::Expected<void> E = (*Assign)->activate(CurModule);
      ELDEXP_RETURN_DIAGENTRY_IF_ERROR(E);
    }
    Assignments.clear();
  }
  return eld::Expected<void>();
}
