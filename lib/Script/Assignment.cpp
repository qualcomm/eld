//===- Assignment.cpp------------------------------------------------------===//
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

#include "eld/Script/Assignment.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticPrinter.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Readers/Section.h"
#include "eld/Script/Expression.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "llvm/Support/Casting.h"
#include <cassert>

using namespace eld;

//===----------------------------------------------------------------------===//
// Assignment
//===----------------------------------------------------------------------===//
Assignment::Assignment(Level AssignmentLevel, Type AssignmentType,
                       std::string Symbol, Expression *ScriptExpression)
    : ScriptCommand(ScriptCommand::ASSIGNMENT),
      AssignmentLevel(AssignmentLevel), ThisType(AssignmentType),
      ExpressionValue(0), Name(Symbol), ExpressionToEvaluate(ScriptExpression),
      ThisSymbol(nullptr) {}

Assignment::~Assignment() {}

void Assignment::dump(llvm::raw_ostream &Outs) const {
  bool CloseParen = true;
  switch (type()) {
  case HIDDEN:
    Outs << "HIDDEN(";
    break;
  case PROVIDE:
    Outs << "PROVIDE(";
    break;
  case PROVIDE_HIDDEN:
    Outs << "PROVIDE_HIDDEN(";
    break;
  default:
    CloseParen = false;
    break;
  }
  switch (type()) {
  case DEFAULT:
  case HIDDEN:
  case PROVIDE:
  case PROVIDE_HIDDEN: {
    Outs << Name;
    Outs << " " << ExpressionToEvaluate->getAssignStr() << " ";
    break;
  }
  case FILL:
  case ASSERT:
    break;
  }
  ExpressionToEvaluate->dump(Outs, false);
  if (CloseParen)
    Outs << ")";
  Outs << ";";
  Outs << "\n";
}

void Assignment::trace(llvm::raw_ostream &Outs) const {
  switch (AssignmentLevel) {
  case OUTSIDE_SECTIONS:
    Outs << "OUTSIDE_SECTIONS   >>  ";
    break;
  case OUTPUT_SECTION:
    Outs << "  OUTPUT_SECTION(PROLOGUE)   >>  ";
    break;
  case INPUT_SECTION:
    Outs << "    INSIDE_OUTPUT_SECTION    >>  ";
    break;
  case SECTIONS_END:
    Outs << "  OUTPUT_SECTION(EPILOGUE)   >>  ";
    break;
  }
  Outs << Name << "(" << ExpressionToEvaluate->result() << ") = ";

  ExpressionToEvaluate->dump(llvm::outs());

  Outs << ";\n";
}

void Assignment::dumpMap(llvm::raw_ostream &Ostream, bool Color,
                         bool UseNewLine, bool WithValues,
                         bool AddIndent) const {
  switch (AssignmentLevel) {
  case OUTSIDE_SECTIONS: {
    if (Color)
      Ostream.changeColor(llvm::raw_ostream::GREEN);
    break;
  }
  case OUTPUT_SECTION: {
    if (Color)
      Ostream.changeColor(llvm::raw_ostream::CYAN);
    break;
  }
  case INPUT_SECTION: {
    if (Color)
      Ostream.changeColor(llvm::raw_ostream::YELLOW);
    if (AddIndent)
      Ostream << "\t";
    break;
  }

  case SECTIONS_END: {
    if (Color)
      Ostream.changeColor(llvm::raw_ostream::MAGENTA);
    break;
  }
  }
  bool CloseParen = false;
  switch (type()) {
  case PROVIDE:
    Ostream << "PROVIDE(";
    CloseParen = true;
    break;
  case PROVIDE_HIDDEN:
    Ostream << "PROVIDE_HIDDEN(";
    CloseParen = true;
    break;
  default:
    break;
  }
  switch (type()) {
  case DEFAULT:
  case HIDDEN:
  case PROVIDE:
  case PROVIDE_HIDDEN: {
    Ostream << Name;
    if (WithValues) {
      Ostream << "(0x";
      Ostream.write_hex(ExpressionToEvaluate->resultOrZero());
      Ostream << ")";
    }
    Ostream << " " << ExpressionToEvaluate->getAssignStr() << " ";
    break;
  }
  case FILL:
  case ASSERT:
    break;
  }
  ExpressionToEvaluate->dump(Ostream, WithValues);
  if (CloseParen)
    Ostream << ")";
  Ostream << ";";

  if (!WithValues && hasInputFileInContext())
    Ostream << " " << getContext();

  if (UseNewLine)
    Ostream << "\n";
  if (Color)
    Ostream.resetColor();
}

eld::Expected<void> Assignment::activate(Module &CurModule) {
  LinkerScript &Script = CurModule.getScript();

  ExpressionToEvaluate->setContext(getContext());

  switch (AssignmentLevel) {
  case OUTSIDE_SECTIONS:
    break;

  case OUTPUT_SECTION: {
    SectionMap::reference Out = Script.sectionMap().back();
    Out->symAssignments().push_back(this);
    break;
  }

  case INPUT_SECTION: {
    OutputSectionEntry::reference In = Script.sectionMap().back()->back();
    In->symAssignments().push_back(this);
    break;
  }

  case SECTIONS_END: {
    SectionMap::reference Out = Script.sectionMap().back();
    Out->sectionEndAssignments().push_back(this);
    break;
  }
  } // end of switch
  return eld::Expected<void>();
}

void Assignment::getSymbols(std::vector<ResolveInfo *> &Symbols) const {
  ExpressionToEvaluate->getSymbols(Symbols);
}

bool Assignment::assign(Module &CurModule, const ELFSection *Section) {

  if (Section && !Section->isAlloc() && isDot()) {
    CurModule.getConfig().raise(Diag::error_dot_lhs_in_non_alloc)
        << std::string(Section->name())
        << ELFSection::getELFPermissionsStr(Section->getFlags());
    return false;
  }

  // evaluate, commit, then get the result of the expression
  auto Result = ExpressionToEvaluate->evaluateAndRaiseError();
  if (!Result)
    return false;
  ExpressionValue = *Result;

  if (!checkLinkerScript(CurModule))
    return false;

  LDSymbol *Sym = CurModule.getNamePool().findSymbol(Name);
  if (Sym != nullptr) {
    ThisSymbol = Sym;
    ThisSymbol->setValue(ExpressionValue);
    ThisSymbol->setScriptValueDefined();
  }

  if (CurModule.getPrinter()->traceAssignments())
    trace(llvm::outs());
  return true;
}

bool Assignment::checkLinkerScript(Module &CurModule) {
  const bool ShowLinkerScriptWarnings =
      CurModule.getConfig().showLinkerScriptWarnings();
  if (!ShowLinkerScriptWarnings)
    return true;
  std::vector<ResolveInfo *> Symbols;
  ExpressionToEvaluate->getSymbols(Symbols);
  for (auto &Sym : Symbols) {
    if (Sym->outSymbol()->scriptDefined() &&
        !CurModule.isVisitedAssignment(std::string(Sym->name()))) {
      CurModule.addVisitedAssignment(Name);
      CurModule.getConfig().raise(Diag::linker_script_var_used_before_define)
          << Sym->name();
      return false;
    }
  }
  CurModule.addVisitedAssignment(Name);
  return true;
}

bool Assignment::isDot() const { return (Name.size() == 1 && Name[0] == '.'); }

bool Assignment::hasDot() const {
  return isDot() || ExpressionToEvaluate->hasDot();
}

// Add undefined symbols for symbols referred by the assignment
void Assignment::processAssignment(Module &CurModule, InputFile &I) {
  CurModule.getScript().assignments().push_back(this);
  if (isProvideOrProvideHidden())
    return;
  for (auto &Sym : getSymbolNames()) {
    CurModule.getNamePool().addUndefinedELFSymbol(&I, Sym);
  }
}

std::unordered_set<std::string> Assignment::getSymbolNames() const {
  std::unordered_set<std::string> SymbolNames;
  ExpressionToEvaluate->getSymbolNames(SymbolNames);
  return SymbolNames;
}
