//===- Assignment.h--------------------------------------------------------===//
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

#ifndef ELD_SCRIPT_ASSIGNMENT_H
#define ELD_SCRIPT_ASSIGNMENT_H

#include "eld/Script/ScriptCommand.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/DataTypes.h"
#include <string>
#include <unordered_set>

namespace eld {

class Module;
class Expression;
class ELFSection;

/** \class Assignment
 *  \brief This class defines the interfaces to assignment command.
 */

class Assignment : public ScriptCommand {
public:
  enum Level {
    OUTSIDE_SECTIONS, // outside SECTIONS command
    OUTPUT_SECTION,   // related to an output section
    INPUT_SECTION,    // related to an input section
    SECTIONS_END
  };

  enum Type { DEFAULT, HIDDEN, PROVIDE, PROVIDE_HIDDEN, FILL, ASSERT };

public:
  Assignment(Level pLevel, Type pType, std::string pSymbol,
             Expression *pExpression);

  ~Assignment();

  Level level() const { return m_Level; }

  void setLevel(enum Level pLevel) { m_Level = pLevel; }

  Type type() const { return m_Type; }

  Expression *getExpression() const { return m_Expr; }

  llvm::StringRef name() const { return m_Name; }

  uint64_t value() const { return m_Value; }

  void dump(llvm::raw_ostream &outs) const override;

  void trace(llvm::raw_ostream &outs) const;

  void dumpMap(llvm::raw_ostream &out = llvm::outs(), bool color = false,
               bool endWithNewLine = true, bool withValues = false,
               bool addIndent = true) const override;

  bool isDot() const;

  // Does the assignment have any dot usage ?
  bool hasDot() const;

  static bool classof(const ScriptCommand *pCmd) {
    return pCmd->getKind() == ScriptCommand::ASSIGNMENT;
  }

  eld::Expected<void> activate(Module &pModule) override;

  /// assign - evaluate the rhs and assign the result to lhs.
  bool assign(Module &pModule, const ELFSection *Section);

  LDSymbol *symbol() const { return m_Symbol; }

  void getSymbols(std::vector<ResolveInfo *> &Symbols) const;

  /// Returns a set of all the symbol names contained in the assignment
  /// expression.
  void getSymbolNames(std::unordered_set<std::string> &symbolTokens) const;

  /// Query functions on Assignment Kinds.
  bool isOutsideSections() const {
    return m_Level == OUTSIDE_SECTIONS || m_Level == SECTIONS_END;
  }

  bool isOutsideOutputSection() const { return m_Level == OUTPUT_SECTION; }

  bool isInsideOutputSection() const { return m_Level == INPUT_SECTION; }

  bool isHidden() const { return m_Type == HIDDEN; }

  bool isProvide() const { return m_Type == PROVIDE; }

  bool isProvideHidden() const { return m_Type == PROVIDE_HIDDEN; }

  bool isProvideOrProvideHidden() const {
    return isProvide() || isProvideHidden();
  }

  bool isFill() const { return m_Type == FILL; }

  bool isAssert() const { return m_Type == ASSERT; }

private:
  bool checkLinkerScript(Module &pModule);

private:
  Level m_Level;
  Type m_Type;
  uint64_t m_Value;
  std::string m_Name;
  Expression *m_Expr;
  LDSymbol *m_Symbol;
};

} // namespace eld

#endif
