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
Assignment::Assignment(Level pLevel, Type pType, std::string pSymbol,
                       Expression *pExpression)
    : ScriptCommand(ScriptCommand::ASSIGNMENT), m_Level(pLevel), m_Type(pType),
      m_Value(0), m_Name(pSymbol), m_Expr(pExpression), m_Symbol(nullptr) {}

Assignment::~Assignment() {}

void Assignment::dump(llvm::raw_ostream &outs) const {
  bool CloseParen = true;
  switch (type()) {
  case HIDDEN:
    outs << "HIDDEN(";
    break;
  case PROVIDE:
    outs << "PROVIDE(";
    break;
  case PROVIDE_HIDDEN:
    outs << "PROVIDE_HIDDEN(";
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
    outs << m_Name;
    outs << " " << m_Expr->getAssignStr() << " ";
    break;
  }
  case FILL:
  case ASSERT:
    break;
  }
  m_Expr->dump(outs, false);
  if (CloseParen)
    outs << ")";
  outs << ";";
  outs << "\n";
}

void Assignment::trace(llvm::raw_ostream &outs) const {
  switch (m_Level) {
  case OUTSIDE_SECTIONS:
    outs << "OUTSIDE_SECTIONS   >>  ";
    break;
  case OUTPUT_SECTION:
    outs << "  OUTPUT_SECTION(PROLOGUE)   >>  ";
    break;
  case INPUT_SECTION:
    outs << "    INSIDE_OUTPUT_SECTION    >>  ";
    break;
  case SECTIONS_END:
    outs << "  OUTPUT_SECTION(EPILOGUE)   >>  ";
    break;
  }
  outs << m_Name << "(" << m_Expr->result() << ") = ";

  m_Expr->dump(llvm::outs());

  outs << ";\n";
}

void Assignment::dumpMap(llvm::raw_ostream &ostream, bool color,
                         bool useNewLine, bool withValues,
                         bool addIndent) const {
  switch (m_Level) {
  case OUTSIDE_SECTIONS: {
    if (color)
      ostream.changeColor(llvm::raw_ostream::GREEN);
    break;
  }
  case OUTPUT_SECTION: {
    if (color)
      ostream.changeColor(llvm::raw_ostream::CYAN);
    break;
  }
  case INPUT_SECTION: {
    if (color)
      ostream.changeColor(llvm::raw_ostream::YELLOW);
    if (addIndent)
      ostream << "\t";
    break;
  }

  case SECTIONS_END: {
    if (color)
      ostream.changeColor(llvm::raw_ostream::MAGENTA);
    break;
  }
  }
  bool CloseParen = false;
  switch (type()) {
  case PROVIDE:
    ostream << "PROVIDE(";
    CloseParen = true;
    break;
  case PROVIDE_HIDDEN:
    ostream << "PROVIDE_HIDDEN(";
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
    ostream << m_Name;
    if (withValues) {
      ostream << "(0x";
      ostream.write_hex(m_Expr->result());
      ostream << ")";
    }
    ostream << " " << m_Expr->getAssignStr() << " ";
    break;
  }
  case FILL:
  case ASSERT:
    break;
  }
  m_Expr->dump(ostream, withValues);
  if (CloseParen)
    ostream << ")";
  ostream << ";";

  if (!withValues && hasInputFileInContext())
    ostream << " " << getContext();

  if (useNewLine)
    ostream << "\n";
  if (color)
    ostream.resetColor();
}

eld::Expected<void> Assignment::activate(Module &pModule) {
  LinkerScript &script = pModule.getScript();

  m_Expr->setContext(getContext());

  if (!isDot())
    script.assignments().push_back(std::make_pair((LDSymbol *)nullptr, this));

  switch (m_Level) {
  case OUTSIDE_SECTIONS:
    break;

  case OUTPUT_SECTION: {
    SectionMap::reference out = script.sectionMap().back();
    out->symAssignments().push_back(this);
    break;
  }

  case INPUT_SECTION: {
    OutputSectionEntry::reference in = script.sectionMap().back()->back();
    in->symAssignments().push_back(this);
    break;
  }

  case SECTIONS_END: {
    SectionMap::reference out = script.sectionMap().back();
    out->sectionEndAssignments().push_back(this);
    break;
  }
  } // end of switch
  return eld::Expected<void>();
}

void Assignment::getSymbols(std::vector<ResolveInfo *> &Symbols) const {
  m_Expr->getSymbols(Symbols);
}

void Assignment::getSymbolNames(
    std::unordered_set<std::string> &symbolTokens) const {
  m_Expr->getSymbolNames(symbolTokens);
}

bool Assignment::assign(Module &pModule, const ELFSection *Section) {

  if (Section && !Section->isAlloc() && isDot()) {
    pModule.getConfig().raise(diag::error_dot_lhs_in_non_alloc)
        << std::string(Section->name())
        << ELFSection::getELFPermissionsStr(Section->getFlags());
    return false;
  }

  // evaluate, commit, then get the result of the expression
  auto Result = m_Expr->evaluateAndRaiseError();
  if (!Result)
    return false;
  m_Value = *Result;

  if (!checkLinkerScript(pModule))
    return false;

  LDSymbol *sym = pModule.getNamePool().findSymbol(m_Name);
  if (sym != nullptr) {
    m_Symbol = sym;
    m_Symbol->setValue(m_Value);
    m_Symbol->setScriptValueDefined();
  }

  if (pModule.getPrinter()->traceAssignments())
    trace(llvm::outs());
  return true;
}

bool Assignment::checkLinkerScript(Module &pModule) {
  const bool showLinkerScriptWarnings =
      pModule.getConfig().showLinkerScriptWarnings();
  if (!showLinkerScriptWarnings)
    return true;
  std::vector<ResolveInfo *> Symbols;
  m_Expr->getSymbols(Symbols);
  for (auto &Sym : Symbols) {
    if (Sym->outSymbol()->scriptDefined() &&
        !pModule.isVisitedAssignment(std::string(Sym->name()))) {
      pModule.addVisitedAssignment(m_Name);
      pModule.getConfig().raise(diag::linker_script_var_used_before_define)
          << Sym->name();
      return false;
    }
  }
  pModule.addVisitedAssignment(m_Name);
  return true;
}

bool Assignment::isDot() const {
  return (m_Name.size() == 1 && m_Name[0] == '.');
}

bool Assignment::hasDot() const { return isDot() || m_Expr->hasDot(); }
