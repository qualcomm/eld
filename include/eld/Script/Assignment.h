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

class InputFile;
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
  Assignment(Level AssignmentLevel, Type AssignmentType, std::string Symbol,
             Expression *ScriptExpression);

  ~Assignment();

  Level level() const { return AssignmentLevel; }

  void setLevel(enum Level AssignmentLevel) {
    this->AssignmentLevel = AssignmentLevel;
  }

  Type type() const { return ThisType; }

  Expression *getExpression() const { return ExpressionToEvaluate; }

  llvm::StringRef name() const { return Name; }

  uint64_t value() const { return ExpressionValue; }

  void dump(llvm::raw_ostream &Outs) const override;

  void trace(llvm::raw_ostream &Outs) const;

  void dumpMap(llvm::raw_ostream &Out = llvm::outs(), bool Color = false,
               bool EndWithNewLine = true, bool WithValues = false,
               bool AddIndent = true) const override;

  bool isDot() const;

  // Does the assignment has any dot usage ?
  bool hasDot() const;

  static bool classof(const ScriptCommand *LinkerScriptCommand) {
    return LinkerScriptCommand->getKind() == ScriptCommand::ASSIGNMENT;
  }

  /// Sets the expression context (location in the linker script)
  /// and the assignment level.
  eld::Expected<void> activate(Module &CurModule) override;

  /// assign - evaluate the rhs and assign the result to lhs.
  bool assign(Module &CurModule, const ELFSection *Section,
              bool EvaluatePendingOnly);

  LDSymbol *symbol() const { return ThisSymbol; }

  /// Returns all the symbols that might be referenced by the rhs of this
  /// assignment No expression evaluation is performed. Hence, this may return
  /// more symbols than what is actually referenced at runtime.
  void getSymbols(std::vector<ResolveInfo *> &Symbols) const;

  /// Query functions on Assignment Kinds.
  bool isOutsideSections() const {
    return AssignmentLevel == OUTSIDE_SECTIONS ||
           AssignmentLevel == SECTIONS_END;
  }

  bool isOutsideOutputSection() const {
    return AssignmentLevel == OUTPUT_SECTION;
  }

  bool isInsideOutputSection() const {
    return AssignmentLevel == INPUT_SECTION;
  }

  bool isHidden() const { return ThisType == HIDDEN; }

  bool isProvide() const { return ThisType == PROVIDE; }

  bool isProvideHidden() const { return ThisType == PROVIDE_HIDDEN; }

  bool isProvideOrProvideHidden() const {
    return isProvide() || isProvideHidden();
  }

  bool isFill() const { return ThisType == FILL; }

  bool isAssert() const { return ThisType == ASSERT; }

  /// Returns the symbol names that might be referenced by the rhs of this
  /// assignment. No expression evaluation is performed. Hence, this may return
  /// names of more symbols than what is actually referenced at runtime.
  std::unordered_set<std::string> getSymbolNames() const;

  // Add all undefined references from assignmnent expressions
  void processAssignment(Module &M, InputFile &I);

  // set property to check if the assignment expression is used
  void setUsed(bool Used) { IsUsed = Used; }

  bool isUsed() const { return IsUsed; }

  /// Reset the assignment value and the expression node.
  void reset();

private:
  bool checkLinkerScript(Module &CurModule);

private:
  Level AssignmentLevel;
  Type ThisType;
  uint64_t ExpressionValue;
  std::string Name;
  Expression *ExpressionToEvaluate;
  LDSymbol *ThisSymbol;
  /* Signifies if the assignment expression is evaluated */
  bool IsUsed = false;
};

} // namespace eld

#endif
