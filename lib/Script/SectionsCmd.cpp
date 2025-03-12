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

void SectionsCmd::dump(llvm::raw_ostream &outs) const {
  outs << "SECTIONS\n{\n";

  for (const auto &elem : *this) {
    switch ((elem)->getKind()) {
    case ScriptCommand::ENTRY:
    case ScriptCommand::ASSIGNMENT:
    case ScriptCommand::OUTPUT_SECT_DESC:
    case ScriptCommand::ASSERT:
      outs << "\t";
      (elem)->dump(outs);
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

  outs << "}\n";
}

void SectionsCmd::dumpOnlyThis(llvm::raw_ostream &outs) const {
  outs << "SECTIONS";
}

void SectionsCmd::push_back(ScriptCommand *pCommand) {
  switch (pCommand->getKind()) {
  case ScriptCommand::ENTRY:
  case ScriptCommand::ENTER_SCOPE:
  case ScriptCommand::EXIT_SCOPE:
  case ScriptCommand::ASSIGNMENT:
  case ScriptCommand::OUTPUT_SECT_DESC:
  case ScriptCommand::ASSERT:
  case ScriptCommand::INCLUDE:
    m_SectionCommands.push_back(pCommand);
    break;
  default:
    break;
  }
}

eld::Expected<void> SectionsCmd::activate(Module &pModule) {
  // Assignment between output sections
  SectionCommands assignments;

  for (const_iterator it = begin(), ie = end(); it != ie; ++it) {
    switch ((*it)->getKind()) {
    case ScriptCommand::ENTRY:
      (*it)->activate(pModule);
      break;
    case ScriptCommand::ASSIGNMENT:
      assignments.push_back(*it);
      break;
    case ScriptCommand::ASSERT:
      (*it)->activate(pModule);
      break;
    case ScriptCommand::OUTPUT_SECT_DESC: {
      if (!assignments.empty()) {
        iterator assign, assignEnd = assignments.end();
        for (assign = assignments.begin(); assign != assignEnd; ++assign) {
          llvm::cast<Assignment>(*assign)->setLevel(Assignment::SECTIONS_END);
          (*assign)->activate(pModule);
        }
        assignments.clear();
      }
      eld::Expected<void> E = (*it)->activate(pModule);
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
  if (!assignments.empty()) {
    iterator assign, assignEnd = assignments.end();
    for (assign = assignments.begin(); assign != assignEnd; ++assign) {
      llvm::cast<Assignment>(*assign)->setLevel(Assignment::SECTIONS_END);
      eld::Expected<void> E = (*assign)->activate(pModule);
      ELDEXP_RETURN_DIAGENTRY_IF_ERROR(E);
    }
    assignments.clear();
  }
  return eld::Expected<void>();
}
