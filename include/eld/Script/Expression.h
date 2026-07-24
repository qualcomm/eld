//===- Expression.h--------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SCRIPT_EXPRESSION_H
#define ELD_SCRIPT_EXPRESSION_H

#include "eld/Object/SectionMap.h"
#include "eld/PluginAPI/DiagnosticEntry.h"
#include "eld/PluginAPI/Expected.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Support/Memory.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/DataTypes.h"
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace eld {
class InputFile;
class LinkerScript;
class Module;
class NamePool;
class ScriptFile;

//===----------------------------------------------------------------------===//
/** \class Expression
 *  \brief This class defines the interfaces to an expression.
 */
class Expression {
public:
  enum Type {
    /* Operands */
    STRING,
    SYMBOL,
    INTEGER,
    /* Operators */
    ADD,
    SUBTRACT,
    MODULO,
    MULTIPLY,
    DIVIDE,
    SIZEOF,
    SIZEOF_HEADERS,
    ADDR,
    LOADADDR,
    TERNARY,
    ALIGN,
    ALIGNOF,
    ABSOLUTE,
    DATA_SEGMENT_ALIGN,
    DATA_SEGMENT_RELRO_END,
    DATA_SEGMENT_END,
    OFFSETOF,
    GT,
    LT,
    EQ,
    GTE,
    LTE,
    NEQ,
    COM,
    UNARYPLUS,
    UNARYMINUS,
    UNARYNOT,
    MAXPAGESIZE,
    COMMONPAGESIZE,
    SEGMENT_START,
    ASSERT,
    PRINT,
    DEFINED,
    BITWISE_RS,
    BITWISE_LS,
    BITWISE_OR,
    BITWISE_AND,
    BITWISE_XOR,
    MAX,
    MIN,
    FILL,
    LOG2CEIL,
    LOGICAL_AND,
    LOGICAL_OR,
    ORIGIN,
    LENGTH,
    NULLEXPR
  };

  Expression(std::string Name, Type Type, Module &Module, uint64_t Value = 0);
  virtual ~Expression() {}

public:
  /// Inspection functions used for Querying expressions and casting
  bool isString() const { return ThisType == STRING; }

  bool isSymbol() const { return (ThisType == SYMBOL); }

  bool isInteger() const { return (ThisType == INTEGER); }

  bool isAdd() const { return (ThisType == ADD); }

  bool isSubtract() const { return (ThisType == SUBTRACT); }

  bool isModulo() const { return (ThisType == MODULO); }

  bool isMultiply() const { return (ThisType == MULTIPLY); }

  bool isDivide() const { return (ThisType == DIVIDE); }

  bool isSizeOf() const { return (ThisType == SIZEOF); }

  bool isSizeOfHeaders() const { return (ThisType == SIZEOF_HEADERS); }

  bool isAddr() const { return (ThisType == ADDR); }

  bool isLoadAddr() const { return (ThisType == LOADADDR); }

  bool isTernary() const { return (ThisType == TERNARY); }

  bool isAlign() const { return (ThisType == ALIGN); }

  bool isAlignOf() const { return (ThisType == ALIGNOF); }

  bool isAbsolute() const { return (ThisType == ABSOLUTE); }

  bool isDataSegmentAlign() const { return (ThisType == DATA_SEGMENT_ALIGN); }

  bool isDataSegmentRelRoEnd() const {
    return (ThisType == DATA_SEGMENT_RELRO_END);
  }

  bool isDataSegmentEnd() const { return (ThisType == DATA_SEGMENT_END); }

  bool isOffsetOf() const { return (ThisType == OFFSETOF); }

  bool isGreater() const { return (ThisType == GT); }

  bool isLessThan() const { return (ThisType == LT); }

  bool isEqual() const { return (ThisType == EQ); }

  bool isGreaterThanOrEqual() const { return (ThisType == GTE); }

  bool isLesserThanOrEqual() const { return (ThisType == LTE); }

  bool isNotEqual() const { return (ThisType == NEQ); }

  bool isComplement() const { return (ThisType == COM); }

  bool isUnaryPlus() const { return (ThisType == UNARYPLUS); }

  bool isUnaryMinus() const { return (ThisType == UNARYMINUS); }

  bool isUnaryNot() const { return (ThisType == UNARYNOT); }

  bool isMaxPageSize() const { return (ThisType == MAXPAGESIZE); }

  bool isCommonPageSize() const { return (ThisType == COMMONPAGESIZE); }

  bool isSegmentStart() const { return (ThisType == SEGMENT_START); }

  bool isAssert() const { return (ThisType == ASSERT); }

  bool isPrint() const { return (ThisType == PRINT); }

  bool isDefined() const { return (ThisType == DEFINED); }

  bool isBitWiseRightShift() const { return (ThisType == BITWISE_RS); }

  bool isBitWiseLeftShift() const { return (ThisType == BITWISE_LS); }

  bool isBitWiseOR() const { return (ThisType == BITWISE_OR); }

  bool isBitWiseAND() const { return (ThisType == BITWISE_AND); }

  bool isBitWiseXOR() const { return (ThisType == BITWISE_XOR); }

  bool isMax() const { return (ThisType == MAX); }

  bool isMin() const { return (ThisType == MIN); }

  bool isFill() const { return (ThisType == FILL); }

  bool isLog2Ceil() const { return (ThisType == LOG2CEIL); }

  bool isLogicalAnd() const { return (ThisType == LOGICAL_AND); }

  bool isLogicalOR() const { return (ThisType == LOGICAL_OR); }

  bool isOrigin() const { return (ThisType == ORIGIN); }

  bool isLength() const { return (ThisType == LENGTH); }

  bool isNullExpr() const { return ThisType == Expression::Type::NULLEXPR; }

  /// evaluateAndReturnError
  /// \brief Evaluate the expression, commit and return the value when
  ///        evaluation is successful. Returns an error if evaluation fails.
  ///        This method is intended to be called by Expression users.
  /// The result is set to 0 if the evaluation fails.
  eld::Expected<uint64_t> evaluateAndReturnError();

  /// evaluateAndRaiseError
  /// \brief Evaluate the expression and return the value when
  ///        evaluation is successful. Raise an error if evaluation fails and
  ///        return nullopt. The commit is called and the result value is set in
  ///        any case.
  /// This method is intended to be called by Expression
  ///        users.
  /// The result is set to 0 if the evaluation fails.
  std::optional<uint64_t> evaluateAndRaiseError();

  /// eval
  /// \brief Evaluate the expression, and return the value when
  ///        evaluation is successful or an error when failed. Commit is
  ///        not called. This method is intended to be recursively called by
  ///        parent expression nodes.
  eld::Expected<uint64_t> eval();

  ///
  /// getBackend
  /// \brief Helper function to return the Target backend
  GNULDBackend &getTargetBackend() const;

private:
  /// eval
  /// \brief eval will be implemented by each derived class. The purpose of eval
  ///        should be to verify and evaluate the expression. eval() should
  ///        return the value if evaluation is successful or raise an error
  ///        and return an empty object otherwise.
  virtual eld::Expected<uint64_t> evalImpl() = 0;

  static std::unique_ptr<plugin::DiagnosticEntry>
  addContextToDiagEntry(std::unique_ptr<plugin::DiagnosticEntry>,
                        const std::string &Context);

public:
  const std::string &getContext() const { return MContext; }

  void setContext(const std::string &Context);

  void setContextRecursively(const std::string &Context);

  /// getSymbols
  /// \brief The symbols that the expression refers to will be returned from
  ///        function.
  virtual void getSymbols(std::vector<ResolveInfo *> &Symbols) = 0;
  virtual void
  getSymbolNames(std::unordered_set<std::string> &SymbolTokens) = 0;

  /// commit
  /// \brief commit should commit the m_Eval to m_Result and also invoke any
  ///        commits for sub expressions that may be present.
  virtual void commit() { MResult = EvaluatedValue; }

  /// dump
  /// \brief print a formatted string for each expression.
  virtual void dump(llvm::raw_ostream &Outs, bool ShowValues = true) const = 0;

  uint64_t result() const;

  bool hasResult() const { return MResult.has_value(); }

  uint64_t resultOrZero() const;

  const std::string &name() const { return Name; }
  Type type() const { return ThisType; }
  Type getType() const { return ThisType; }

  void setParen() { ExpressionHasParenthesis = true; }

  void setAssign() { ExpressionIsAssignment = true; }

  bool hasAssign() const { return ExpressionIsAssignment; }

  // Get left side expression to get name and result.
  // Return nullptr when left and right expressions are not available.
  virtual Expression *getLeftExpression() const = 0;

  // Get right side expression to get name and result.
  // Return nullptr when left and right expressions are not available.
  // In case of Unary operator, returns the only expression needed.
  virtual Expression *getRightExpression() const = 0;

  // Casting support
  static bool classof(const Expression *Exp) { return true; }

  // Does the expression contain a Dot ?
  virtual bool hasDot() const = 0;

  // Get the assignment sign
  virtual std::string getAssignStr() const {
    if (hasAssign())
      return Name + "=";
    return "=";
  }

  /// Add all symbols referenced by this expression as undefined symbols
  /// to the NamePool.
  void addRefSymbolsAsUndefSymbolToNP(InputFile *IF, NamePool &NP);

protected:
  std::string Name;   /// string representation of the expression
  Type ThisType;      /// type of expression which is being evaluated
  Module &ThisModule; /// pointer to Module to be used for evaluation purposes.
  bool ExpressionHasParenthesis = false;
  bool ExpressionIsAssignment =
      false; /// Is this expression an assignment like +=/-=/*= etc..
  // TODO: remove this and return the value from commit?
  std::string MContext; // context is only set in the outermost expression
  std::optional<uint64_t> MResult; /// committed result from the evaluation

private:
  uint64_t EvaluatedValue; /// temporary assignment to hold evaluation result
};

/** \class Symbol
 *  \brief This class extends an Expression to a Symbol operand.
 */
class Symbol : public Expression {
public:
  Symbol(Module &Module, std::string Name);

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isSymbol(); }

private:
  bool hasDot() const override;
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  /// Returns a set of all the symbol names contained in the expression.
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return nullptr; }
  Expression *getRightExpression() const override { return nullptr; }

  mutable LDSymbol *ThisSymbol = nullptr;
  const Assignment *SourceAssignment = nullptr;
};

//===----------------------------------------------------------------------===//
/** \class Integer
 *  \brief This class extends an Expression to an Integer operand.
 */
class Integer : public Expression {
public:
  Integer(Module &Module, std::string Name, uint64_t Value)
      : Expression(Name, Expression::INTEGER, Module, Value),
        ExpressionValue(Value) {}

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isInteger(); }

private:
  bool hasDot() const override { return false; }
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return nullptr; }
  Expression *getRightExpression() const override { return nullptr; }

  const uint64_t ExpressionValue;
};

//===----------------------------------------------------------------------===//
/** \class BinaryOp
 *  \brief Unified binary operator expression (arithmetic, bitwise, relational,
 *         logical, and min/max). The operator semantics are determined by the
 *         expression Type.
 */
class BinaryOp : public Expression {
public:
  BinaryOp(llvm::StringRef OpName, Expression::Type T, Module &Module,
           Expression &Left, Expression &Right)
      : Expression(OpName.str(), T, Module), LeftExpression(Left),
        RightExpression(Right) {}

  // Casting support
  static bool classof(const Expression *Exp) {
    switch (Exp->type()) {
    case ADD:
    case SUBTRACT:
    case MODULO:
    case MULTIPLY:
    case DIVIDE:
    case BITWISE_RS:
    case BITWISE_LS:
    case GT:
    case LT:
    case GTE:
    case LTE:
    case EQ:
    case NEQ:
    case LOGICAL_OR:
    case LOGICAL_AND:
    case BITWISE_AND:
    case BITWISE_XOR:
    case BITWISE_OR:
    case MAX:
    case MIN:
      return true;
    default:
      return false;
    }
  }

private:
  bool hasDot() const override;
  void commit() override;
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return &LeftExpression; }
  Expression *getRightExpression() const override { return &RightExpression; }

  Expression &LeftExpression;
  Expression &RightExpression;
};

// Type aliases kept for call-site compatibility.
using Add = BinaryOp;
using Subtract = BinaryOp;
using Modulo = BinaryOp;
using Multiply = BinaryOp;
using Divide = BinaryOp;

//===----------------------------------------------------------------------===//
/** \class SizeOf
 *  \brief This class extends an Expression to a SizeOf operator.
 */
class SizeOf : public Expression {
public:
  SizeOf(Module &Module, std::string Name);

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isSizeOf(); }

private:
  bool hasDot() const override { return false; }
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return nullptr; }
  Expression *getRightExpression() const override { return nullptr; }

  ELFSection *ThisSection; /// the section size which should be evaluated.
};

//===----------------------------------------------------------------------===//
/** \class SizeOfHeaders
 *  \brief This class extends an Expression to a SIZEOF_HEADERS keyword.
 */
class SizeOfHeaders : public Expression {
public:
  SizeOfHeaders(Module &Module, ScriptFile *S);

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isSizeOfHeaders(); }

private:
  bool hasDot() const override { return false; }
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return nullptr; }
  Expression *getRightExpression() const override { return nullptr; }
};
//===----------------------------------------------------------------------===//
/** \class OffsetOf
 *  \brief This class extends an Expression to an OffsetOf operator.
 */
class OffsetOf : public Expression {
public:
  OffsetOf(Module &Module, std::string Name)
      : Expression(Name, Expression::OFFSETOF, Module), ThisSection(nullptr) {}
  OffsetOf(Module &Module, ELFSection *Sect)
      : Expression(Sect->name().str(), Expression::OFFSETOF, Module),
        ThisSection(Sect) {}

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isOffsetOf(); }

private:
  bool hasDot() const override { return false; }
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return nullptr; }
  Expression *getRightExpression() const override { return nullptr; }

  ELFSection *ThisSection; /// the section size which should be evaluated.
};

//===----------------------------------------------------------------------===//
/** \class Addr
 *  \brief This class extends an Expression to an Address operator.
 */
class Addr : public Expression {
public:
  Addr(Module &Module, std::string Name)
      : Expression(Name, Expression::ADDR, Module), ThisSection(nullptr) {}

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isAddr(); }

private:
  bool hasDot() const override { return false; }
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return nullptr; }
  Expression *getRightExpression() const override { return nullptr; }

  ELFSection *ThisSection; /// the section addr which should be evaluated.
};

//===----------------------------------------------------------------------===//
/** \class LoadAddr
 *  \brief This class extends an Expression to an LoadAddress operator.
 */
class LoadAddr : public Expression {
public:
  LoadAddr(Module &Module, std::string Name);

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isLoadAddr(); }

private:
  bool hasDot() const override { return false; }
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return nullptr; }
  Expression *getRightExpression() const override { return nullptr; }

  ELFSection *ThisSection; /// the section addr which should be evaluated.
  bool ForwardReference;
};
//===----------------------------------------------------------------------===//
/** \class AlignExpr
 *  \brief This class extends an Expression to an AlignmentOf operator.
 */
class AlignExpr : public Expression {
public:
  AlignExpr(Module &Module, const std::string &Context, Expression &Align,
            Expression &Expr)
      : Expression("ALIGN", Expression::ALIGN, Module),
        AlignmentExpression(Align), ExpressionToEvaluate(Expr) {
    setContext(Context);
  }

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isAlign(); }

private:
  bool hasDot() const override;
  void commit() override;
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override {
    return &ExpressionToEvaluate;
  }
  Expression *getRightExpression() const override {
    return &AlignmentExpression;
  }

  Expression &AlignmentExpression; /// represents the alignment value
  Expression
      &ExpressionToEvaluate; /// represents the dot or expression to be aligned
};

//===----------------------------------------------------------------------===//
/** \class AlignOf
 *  \brief This class extends an Expression to an Alignment operator.
 */
class AlignOf : public Expression {
public:
  AlignOf(Module &Module, std::string Name)
      : Expression(Name, Expression::ALIGNOF, Module), ThisSection(nullptr) {}
  AlignOf(Module &Module, ELFSection *Sect)
      : Expression(Sect->name().str(), Expression::ALIGNOF, Module),
        ThisSection(Sect) {}

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isAlignOf(); }

private:
  bool hasDot() const override { return false; }
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return nullptr; }
  Expression *getRightExpression() const override { return nullptr; }

  ELFSection *ThisSection; /// represents the section alignment to return
};

//===----------------------------------------------------------------------===//
/** \class Absolute
 *  \brief This class extends an Expression to an Absolute operator.
 */
class Absolute : public Expression {
public:
  Absolute(Module &Module, Expression &Expr)
      : Expression("ABSOLUTE", Expression::ABSOLUTE, Module),
        ExpressionToEvaluate(Expr) {}

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isAbsolute(); }

private:
  bool hasDot() const override;
  void commit() override;
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return nullptr; }
  Expression *getRightExpression() const override {
    return &ExpressionToEvaluate;
  }

  Expression &ExpressionToEvaluate;
};

//===----------------------------------------------------------------------===//
/** \class Ternary
 *  \brief This class extends an Expression to a Ternary operator.
 */
class Ternary : public Expression {
public:
  Ternary(Module &Module, Expression &Cond, Expression &Left, Expression &Right)
      : Expression("?", Expression::TERNARY, Module), ConditionExpression(Cond),
        LeftExpression(Left), RightExpression(Right) {}

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isTernary(); }

  Expression *getConditionalExpression() const { return &ConditionExpression; }

private:
  bool hasDot() const override;
  void commit() override;
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return &LeftExpression; }
  Expression *getRightExpression() const override { return &RightExpression; }

  Expression &ConditionExpression; /// represents the conditional expression to
                                   /// be evaluated.
  Expression &LeftExpression;      /// represents the left hand expression.
  Expression &RightExpression;     /// represents the right hand expression.
};

// Condition operator aliases — all implemented by BinaryOp.
using ConditionGT = BinaryOp;
using ConditionLT = BinaryOp;
using ConditionEQ = BinaryOp;
using ConditionGTE = BinaryOp;
using ConditionLTE = BinaryOp;
using ConditionNEQ = BinaryOp;

//===----------------------------------------------------------------------===//
/** \class UnaryOp
 *  \brief Unified unary operator expression (complement, unary plus/minus/not).
 *         The operator semantics are determined by the expression Type.
 */
class UnaryOp : public Expression {
public:
  UnaryOp(llvm::StringRef OpName, Expression::Type T, Module &Module,
          Expression &Expr)
      : Expression(OpName.str(), T, Module), ExpressionToEvaluate(Expr) {}

  // Casting support
  static bool classof(const Expression *Exp) {
    switch (Exp->type()) {
    case COM:
    case UNARYPLUS:
    case UNARYMINUS:
    case UNARYNOT:
      return true;
    default:
      return false;
    }
  }

private:
  bool hasDot() const override;
  void commit() override;
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return nullptr; }
  Expression *getRightExpression() const override {
    return &ExpressionToEvaluate;
  }

  Expression &ExpressionToEvaluate;
};

// Unary operator aliases — all implemented by UnaryOp.
using Complement = UnaryOp;
using UnaryPlus = UnaryOp;
using UnaryMinus = UnaryOp;
using UnaryNot = UnaryOp;
//===----------------------------------------------------------------------===//
/** \class Constant
 *  \brief This class extends an Expression to a Constant operator.
 */
class Constant : public Expression {
public:
  Constant(Module &Module, std::string Name, Type Type)
      : Expression(Name, Type, Module) {}

  // Casting support
  static bool classof(const Expression *Exp) {
    return Exp->isMaxPageSize() || Exp->isCommonPageSize();
  }

private:
  bool hasDot() const override { return false; }
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return nullptr; }
  Expression *getRightExpression() const override { return nullptr; }
};

//===----------------------------------------------------------------------===//
/** \class SegmentStart
 *  \brief This class extends an Expression to a SegmentStart operator.
 */
class SegmentStart : public Expression {
public:
  SegmentStart(Module &Module, std::string Segment, Expression &Expr)
      : Expression("SEGMENT_START", Expression::SEGMENT_START, Module),
        SegmentName(Segment), ExpressionToEvaluate(Expr) {}

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isSegmentStart(); }

  const std::string &getSegmentString() const { return SegmentName; }

private:
  bool hasDot() const override;
  void commit() override;
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return nullptr; }
  Expression *getRightExpression() const override {
    return &ExpressionToEvaluate;
  }

  std::string SegmentName;
  Expression &ExpressionToEvaluate;
};

//===----------------------------------------------------------------------===//
/** \class AssertCmd
 *  \brief This class extends an Expression to an Assert command.
 */
class AssertCmd : public Expression {
public:
  AssertCmd(Module &Module, std::string Msg, Expression &Expr)
      : Expression("ASSERT", Expression::ASSERT, Module),
        ExpressionToEvaluate(Expr), AssertionMessage(Msg) {}

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isAssert(); }

  const std::string &getAssertString() const { return AssertionMessage; }

private:
  bool hasDot() const override;
  void commit() override;
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return nullptr; }
  Expression *getRightExpression() const override {
    return &ExpressionToEvaluate;
  }

  Expression &ExpressionToEvaluate; /// represents the expression to evaluate
  std::string AssertionMessage; /// represents the message to print if we assert
};

//===----------------------------------------------------------------------===//
/** \class PrintCmd
 *  \brief This class extends an Expression to a PRINT command.
 */
class PrintCmd : public Expression {
public:
  PrintCmd(Module &Module, std::string FormatString,
           std::vector<Expression *> Args)
      : Expression("PRINT", Expression::PRINT, Module),
        RawFormatString(std::move(FormatString)), Arguments(std::move(Args)) {}

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isPrint(); }

private:
  bool hasDot() const override;
  void commit() override;
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return nullptr; }
  Expression *getRightExpression() const override { return nullptr; }

  static std::string unescape(llvm::StringRef S);

  eld::Expected<std::string>
  formatString(llvm::StringRef Fmt, llvm::ArrayRef<Expression *> Args) const;

  std::string RawFormatString;
  std::vector<Expression *> Arguments;
  std::vector<uint64_t> ArgValues;
};

// Bitwise and logical operator aliases — all implemented by BinaryOp.
using RightShift = BinaryOp;
using LeftShift = BinaryOp;
using BitwiseOr = BinaryOp;
using BitwiseAnd = BinaryOp;
using BitwiseXor = BinaryOp;
using LogicalOp = BinaryOp;

//===----------------------------------------------------------------------===//
/** \class Defined
 *  \brief This class extends an Expression to a Defined operator.
 */
class Defined : public Expression {
public:
  Defined(Module &Module, std::string Name)
      : Expression(Name, Expression::DEFINED, Module) {}

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isDefined(); }

private:
  bool hasDot() const override;
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return nullptr; }
  Expression *getRightExpression() const override { return nullptr; }
};

//===----------------------------------------------------------------------===//
/** \class DataSegmentAlign
 *  \brief This class extends an Expression to a Data Segment Align operator.
 */
class DataSegmentAlign : public Expression {
public:
  DataSegmentAlign(Module &Module, Expression &MaxPageSize,
                   Expression &CommonPageSize)
      : Expression("DATA_SEGMENT_ALIGN", Expression::DATA_SEGMENT_ALIGN,
                   Module),
        MaxPageSize(MaxPageSize), CommonPageSize(CommonPageSize) {}

  // Casting support
  static bool classof(const Expression *Exp) {
    return Exp->isDataSegmentAlign();
  }

private:
  bool hasDot() const override { return false; }
  void commit() override;
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return &MaxPageSize; }
  Expression *getRightExpression() const override { return &CommonPageSize; }

  Expression &MaxPageSize;    /// represents the max page size
  Expression &CommonPageSize; /// represents the common page size
};

//===----------------------------------------------------------------------===//
/** \class DataSegmentRelRoEnd
 *  \brief This class extends an Expression to a Data Segment Relocation End
 * operator.
 */
class DataSegmentRelRoEnd : public Expression {
public:
  DataSegmentRelRoEnd(Module &Module, Expression &Expr1, Expression &Expr2)
      : Expression("DATA_SEGMENT_RELRO_END", Expression::DATA_SEGMENT_RELRO_END,
                   Module),
        LeftExpression(Expr1), RightExpression(Expr2),
        CommonPageSize(*make<Constant>(Module, "COMMONPAGESIZE",
                                       Expression::COMMONPAGESIZE)) {}

  // Casting support
  static bool classof(const Expression *Exp) {
    return Exp->isDataSegmentRelRoEnd();
  }

private:
  bool hasDot() const override;
  void commit() override;
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return &LeftExpression; }
  Expression *getRightExpression() const override { return &RightExpression; }
  Expression *getCommonPageSizeExpression() const { return &CommonPageSize; }

  Expression &LeftExpression;  /// represents the expression to be added
  Expression &RightExpression; /// represents the expression to be added
  Constant &CommonPageSize;    /// represents the common page size
};

//===----------------------------------------------------------------------===//
/** \class DataSegmentEnd
 *  \brief This class extends an Expression to a Data Segment End operator.
 */
class DataSegmentEnd : public Expression {
public:
  DataSegmentEnd(Module &Module, Expression &Expr)
      : Expression("DATA_SEGMENT_END", Expression::DATA_SEGMENT_END, Module),
        ExpressionToEvaluate(Expr) {}

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isDataSegmentEnd(); }

private:
  bool hasDot() const override;
  void commit() override;
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return nullptr; }
  Expression *getRightExpression() const override {
    return &ExpressionToEvaluate;
  }

  Expression
      &ExpressionToEvaluate; /// represents the expression to be evaluated
};

//===----------------------------------------------------------------------===//
/** \class QueryMemory Command support  (ORIGIN/LENGTH)
 *  \brief This class extends an Expression to support memory command query
 *  capabiliites
 */
class Fill : public Expression {
public:
  Fill(Module &Module, Expression &Expr)
      : Expression("FILL", Expression::FILL, Module),
        ExpressionToEvaluate(Expr) {}

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isFill(); }

private:
  bool hasDot() const override;
  void commit() override;
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return nullptr; }
  Expression *getRightExpression() const override {
    return &ExpressionToEvaluate;
  }

  Expression &ExpressionToEvaluate; /// represents the left hand operand.
};

//===----------------------------------------------------------------------===//
/** \class Log2Ceil
 *  \brief This class extends an Expression to a Log2Ceil operator.
 */
class Log2Ceil : public Expression {
public:
  Log2Ceil(Module &Module, Expression &Expr)
      : Expression("LOG2CEIL", Expression::LOG2CEIL, Module),
        ExpressionToEvaluate(Expr) {}

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isLog2Ceil(); }

private:
  bool hasDot() const override;
  void commit() override;
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return nullptr; }
  Expression *getRightExpression() const override {
    return &ExpressionToEvaluate;
  }

  Expression &ExpressionToEvaluate; /// represents the expression to complement
};

// Max and Min aliases — all implemented by BinaryOp.
using Max = BinaryOp;
using Min = BinaryOp;
class QueryMemory : public Expression {
public:
  QueryMemory(Expression::Type Type, Module &Module, const std::string &Name);

  // Casting support
  static bool classof(const Expression *Exp) {
    return Exp->isSizeOf() || Exp->isOrigin();
  }

private:
  bool hasDot() const override { return false; }
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return nullptr; }
  Expression *getRightExpression() const override { return nullptr; }
};

inline void alignAddress(uint64_t &PAddr, uint64_t AlignConstraint) {
  if (AlignConstraint != 0)
    PAddr = (PAddr + AlignConstraint - 1) & ~(AlignConstraint - 1);
}

/// NullExpression represents an invalid expression. It is used as a sentinel
/// expression when the linker script parser fails to parse an expression.
class NullExpression : public Expression {
public:
  NullExpression(Module &Module);

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isNullExpr(); }

private:
  bool hasDot() const override { return false; }
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return nullptr; }
  Expression *getRightExpression() const override { return nullptr; }
};

} // namespace eld

#endif
