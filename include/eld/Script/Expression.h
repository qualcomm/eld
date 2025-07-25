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
#include "llvm/Support/DataTypes.h"
#include <optional>
#include <string>
#include <unordered_set>

namespace eld {
class LinkerScript;
class Module;
class GNULDBackend;
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

  Expression(std::string Name, Type Type, Module &Module, GNULDBackend &Backend,
             uint64_t Value = 0);
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
  eld::Expected<uint64_t> evaluateAndReturnError();

  /// evaluateAndRaiseError
  /// \brief Evaluate the expression and return the value when
  ///        evaluation is successful. Raise an error if evaluation fails and
  ///        return nullopt. The commit is called and the result value is set in
  ///        any case.
  /// This method is intended to be called by Expression
  ///        users.
  std::optional<uint64_t> evaluateAndRaiseError();

  /// eval
  /// \brief Evaluate the expression, and return the value when
  ///        evaluation is successful or an error when failed. Commit is
  ///        not called. This method is intended to be recursively called by
  ///        parent expression nodes.
  eld::Expected<uint64_t> eval();

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

  bool hasResult() const {
    return MResult.has_value();
  }

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

protected:
  std::string Name;   /// string representation of the expression
  Type ThisType;      /// type of expression which is being evaluated
  Module &ThisModule; /// pointer to Module to be used for evaluation purposes.
  GNULDBackend &ThisBackend; /// pointer to Target Backend for target data
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
  Symbol(Module &Module, GNULDBackend &Backend, std::string Name);

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
};

//===----------------------------------------------------------------------===//
/** \class Integer
 *  \brief This class extends an Expression to an Integer operand.
 */
class Integer : public Expression {
public:
  Integer(Module &Module, GNULDBackend &Backend, std::string Name,
          uint64_t Value)
      : Expression(Name, Expression::INTEGER, Module, Backend, Value),
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
/** \class Add
 *  \brief This class extends an Expression to an Add operator.
 */
class Add : public Expression {
public:
  Add(Module &Module, GNULDBackend &Backend, Expression &Left,
      Expression &Right)
      : Expression("+", Expression::ADD, Module, Backend), LeftExpression(Left),
        RightExpression(Right) {}

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isAdd(); }

private:
  bool hasDot() const override;
  void commit() override;
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return &LeftExpression; }
  Expression *getRightExpression() const override { return &RightExpression; }

  Expression &LeftExpression;  /// represents the left hand operand.
  Expression &RightExpression; /// represents the right hand operand.
};

//===----------------------------------------------------------------------===//
/** \class Subtract
 *  \brief This class extends an Expression to a Subtraction operator.
 */
class Subtract : public Expression {
public:
  Subtract(Module &Module, GNULDBackend &Backend, Expression &Left,
           Expression &Right)
      : Expression("-", Expression::SUBTRACT, Module, Backend),
        LeftExpression(Left), RightExpression(Right) {}

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isSubtract(); }

private:
  bool hasDot() const override;
  void commit() override;
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return &LeftExpression; }
  Expression *getRightExpression() const override { return &RightExpression; }

  Expression &LeftExpression;  /// represents the left hand operand.
  Expression &RightExpression; /// represents the right hand operand.
};

//===----------------------------------------------------------------------===//
/** \class Modulo
 *  \brief This class extends an Expression to a Modulus operator.
 */
class Modulo : public Expression {
public:
  Modulo(Module &Module, GNULDBackend &Backend, Expression &Left,
         Expression &Right)
      : Expression("%", Expression::MODULO, Module, Backend),
        LeftExpression(Left), RightExpression(Right) {}

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isModulo(); }

private:
  bool hasDot() const override;
  void commit() override;
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return &LeftExpression; }
  Expression *getRightExpression() const override { return &RightExpression; }

  Expression &LeftExpression;  /// represents the left hand operand.
  Expression &RightExpression; /// represents the right hand operand.
};

//===----------------------------------------------------------------------===//
/** \class Multiply
 *  \brief This class extends an Expression to a Multiply operator.
 */
class Multiply : public Expression {
public:
  Multiply(Module &Module, GNULDBackend &Backend, Expression &Left,
           Expression &Right)
      : Expression("*", Expression::MULTIPLY, Module, Backend),
        LeftExpression(Left), RightExpression(Right) {}

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isMultiply(); }

private:
  bool hasDot() const override;
  void commit() override;
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return &LeftExpression; }
  Expression *getRightExpression() const override { return &RightExpression; }

  Expression &LeftExpression;  /// represents the left hand operand.
  Expression &RightExpression; /// represents the right hand operand.
};

//===----------------------------------------------------------------------===//
/** \class Divide
 *  \brief This class extends an Expression to a Divide operator.
 */
class Divide : public Expression {
public:
  Divide(Module &Module, GNULDBackend &Backend, Expression &Left,
         Expression &Right)
      : Expression("/", Expression::DIVIDE, Module, Backend),
        LeftExpression(Left), RightExpression(Right) {}

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isDivide(); }

private:
  bool hasDot() const override;
  void commit() override;
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return &LeftExpression; }
  Expression *getRightExpression() const override { return &RightExpression; }

  Expression &LeftExpression;  /// represents the left hand operand.
  Expression &RightExpression; /// represents the right hand operand.
};

//===----------------------------------------------------------------------===//
/** \class SizeOf
 *  \brief This class extends an Expression to a SizeOf operator.
 */
class SizeOf : public Expression {
public:
  SizeOf(Module &Module, GNULDBackend &Backend, std::string Name);

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
  SizeOfHeaders(Module &Module, GNULDBackend &Backend, ScriptFile *S);

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
  OffsetOf(Module &Module, GNULDBackend &Backend, std::string Name)
      : Expression(Name, Expression::OFFSETOF, Module, Backend),
        ThisSection(nullptr) {}
  OffsetOf(Module &Module, GNULDBackend &Backend, ELFSection *Sect)
      : Expression(Sect->name().str(), Expression::OFFSETOF, Module, Backend),
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
  Addr(Module &Module, GNULDBackend &Backend, std::string Name)
      : Expression(Name, Expression::ADDR, Module, Backend),
        ThisSection(nullptr) {}

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
  LoadAddr(Module &Module, GNULDBackend &Backend, std::string Name);

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
  AlignExpr(Module &Module, GNULDBackend &Backend, const std::string &Context,
            Expression &Align, Expression &Expr)
      : Expression("ALIGN", Expression::ALIGN, Module, Backend),
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
  AlignOf(Module &Module, GNULDBackend &Backend, std::string Name)
      : Expression(Name, Expression::ALIGNOF, Module, Backend),
        ThisSection(nullptr) {}
  AlignOf(Module &Module, GNULDBackend &Backend, ELFSection *Sect)
      : Expression(Sect->name().str(), Expression::ALIGNOF, Module, Backend),
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
  Absolute(Module &Module, GNULDBackend &Backend, Expression &Expr)
      : Expression("ABSOLUTE", Expression::ABSOLUTE, Module, Backend),
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
  Ternary(Module &Module, GNULDBackend &Backend, Expression &Cond,
          Expression &Left, Expression &Right)
      : Expression("?", Expression::TERNARY, Module, Backend),
        ConditionExpression(Cond), LeftExpression(Left),
        RightExpression(Right) {}

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

//===----------------------------------------------------------------------===//
/** \class ConditionGT
 *  \brief This class extends an Expression to a GreaterThan operator.
 */
class ConditionGT : public Expression {
public:
  ConditionGT(Module &Module, GNULDBackend &Backend, Expression &Left,
              Expression &Right)
      : Expression(">", Expression::GT, Module, Backend), LeftExpression(Left),
        RightExpression(Right) {}

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isGreater(); }

private:
  bool hasDot() const override;
  void commit() override;
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return &LeftExpression; }
  Expression *getRightExpression() const override { return &RightExpression; }

  Expression &LeftExpression;  /// represents the left hand expression.
  Expression &RightExpression; /// represents the right hand expression.
};

//===----------------------------------------------------------------------===//
/** \class ConditionLT
 *  \brief This class extends an Expression to a LessThan operator.
 */
class ConditionLT : public Expression {
public:
  ConditionLT(Module &Module, GNULDBackend &Backend, Expression &Left,
              Expression &Right)
      : Expression("<", Expression::LT, Module, Backend), LeftExpression(Left),
        RightExpression(Right) {}

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isLessThan(); }

private:
  bool hasDot() const override;
  void commit() override;
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return &LeftExpression; }
  Expression *getRightExpression() const override { return &RightExpression; }

  Expression &LeftExpression;  /// represents the left hand expression.
  Expression &RightExpression; /// represents the right hand expression.
};

//===----------------------------------------------------------------------===//
/** \class ConditionEQ
 *  \brief This class extends an Expression to an EqualTo operator.
 */
class ConditionEQ : public Expression {
public:
  ConditionEQ(Module &Module, GNULDBackend &Backend, Expression &Left,
              Expression &Right)
      : Expression("==", Expression::EQ, Module, Backend), LeftExpression(Left),
        RightExpression(Right) {}

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isEqual(); }

private:
  bool hasDot() const override;
  void commit() override;
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return &LeftExpression; }
  Expression *getRightExpression() const override { return &RightExpression; }

  Expression &LeftExpression;  /// represents the left hand expression.
  Expression &RightExpression; /// represents the right hand expression.
};

//===----------------------------------------------------------------------===//
/** \class ConditionGTE
 *  \brief This class extends an Expression to a GreaterThanEqual operator.
 */
class ConditionGTE : public Expression {
public:
  ConditionGTE(Module &Module, GNULDBackend &Backend, Expression &Left,
               Expression &Right)
      : Expression(">=", Expression::GTE, Module, Backend),
        LeftExpression(Left), RightExpression(Right) {}

  // Casting support
  static bool classof(const Expression *Exp) {
    return Exp->isGreaterThanOrEqual();
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

  Expression &LeftExpression;  /// represents the left hand expression.
  Expression &RightExpression; /// represents the right hand expression.
};

//===----------------------------------------------------------------------===//
/** \class ConditionLTE
 *  \brief This class extends an Expression to a LessThanEqual operator.
 */
class ConditionLTE : public Expression {
public:
  ConditionLTE(Module &Module, GNULDBackend &Backend, Expression &Left,
               Expression &Right)
      : Expression("<=", Expression::LTE, Module, Backend),
        LeftExpression(Left), RightExpression(Right) {}

  // Casting support
  static bool classof(const Expression *Exp) {
    return Exp->isLesserThanOrEqual();
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

  Expression &LeftExpression;  /// represents the left hand expression.
  Expression &RightExpression; /// represents the right hand expression.
};

//===----------------------------------------------------------------------===//
/** \class ConditionNEQ
 *  \brief This class extends an Expression to an NotEqualTo operator.
 */
class ConditionNEQ : public Expression {
public:
  ConditionNEQ(Module &Module, GNULDBackend &Backend, Expression &Left,
               Expression &Right)
      : Expression("!=", Expression::NEQ, Module, Backend),
        LeftExpression(Left), RightExpression(Right) {}

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isNotEqual(); }

private:
  bool hasDot() const override;
  void commit() override;
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return &LeftExpression; }
  Expression *getRightExpression() const override { return &RightExpression; }

  Expression &LeftExpression;  /// represents the left hand expression.
  Expression &RightExpression; /// represents the right hand expression.
};

//===----------------------------------------------------------------------===//
/** \class Complement
 *  \brief This class extends an Expression to a Complement operator.
 */
class Complement : public Expression {
public:
  Complement(Module &Module, GNULDBackend &Backend, Expression &Expr)
      : Expression("~", Expression::COM, Module, Backend),
        ExpressionToEvaluate(Expr) {}

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isComplement(); }

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

//===----------------------------------------------------------------------===//
/** \class UnaryPlus
 *  \brief This class extends an Expression to a UnaryPlus operator.
 */
class UnaryPlus : public Expression {
public:
  UnaryPlus(Module &Module, GNULDBackend &Backend, Expression &Expr)
      : Expression("+", Expression::UNARYMINUS, Module, Backend),
        ExpressionToEvaluate(Expr) {}

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isUnaryPlus(); }

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

//===----------------------------------------------------------------------===//
/** \class UnaryMinus
 *  \brief This class extends an Expression to a UnaryMinus operator.
 */
class UnaryMinus : public Expression {
public:
  UnaryMinus(Module &Module, GNULDBackend &Backend, Expression &Expr)
      : Expression("-", Expression::UNARYMINUS, Module, Backend),
        ExpressionToEvaluate(Expr) {}

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isUnaryMinus(); }

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

//===----------------------------------------------------------------------===//
/** \class UnaryNot
 *  \brief This class extends an Expression to a UnaryNot operator.
 */
class UnaryNot : public Expression {
public:
  UnaryNot(Module &Module, GNULDBackend &Backend, Expression &Expr)
      : Expression("~", Expression::UNARYNOT, Module, Backend),
        ExpressionToEvaluate(Expr) {}

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isUnaryNot(); }

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
//===----------------------------------------------------------------------===//
/** \class Constant
 *  \brief This class extends an Expression to a Constant operator.
 */
class Constant : public Expression {
public:
  Constant(Module &Module, GNULDBackend &Backend, std::string Name, Type Type)
      : Expression(Name, Type, Module, Backend) {}

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
  SegmentStart(Module &Module, GNULDBackend &Backend, std::string Segment,
               Expression &Expr)
      : Expression("SEGMENT_START", Expression::SEGMENT_START, Module, Backend),
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
  AssertCmd(Module &Module, GNULDBackend &Backend, std::string Msg,
            Expression &Expr)
      : Expression("ASSERT", Expression::ASSERT, Module, Backend),
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

/** \class RightShift
 *  \brief This class extends an Expression to a Right Shift operator.
 */
class RightShift : public Expression {
public:
  RightShift(Module &Module, GNULDBackend &Backend, Expression &Left,
             Expression &Right)
      : Expression(">>", Expression::BITWISE_RS, Module, Backend),
        LeftExpression(Left), RightExpression(Right) {}

  // Casting support
  static bool classof(const Expression *Exp) {
    return Exp->isBitWiseRightShift();
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

  Expression &LeftExpression;  /// represents the left hand operand.
  Expression &RightExpression; /// represents the right hand operand.
};

//===----------------------------------------------------------------------===//
/** \class LeftShift
 *  \brief This class extends an Expression to a Left Shift operator.
 */
class LeftShift : public Expression {
public:
  LeftShift(Module &Module, GNULDBackend &Backend, Expression &Left,
            Expression &Right)
      : Expression("<<", Expression::BITWISE_LS, Module, Backend),
        LeftExpression(Left), RightExpression(Right) {}

  // Casting support
  static bool classof(const Expression *Exp) {
    return Exp->isBitWiseLeftShift();
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

  Expression &LeftExpression;  /// represents the left hand operand.
  Expression &RightExpression; /// represents the right hand operand.
};

//===----------------------------------------------------------------------===//
/** \class BitwiseOr
 *  \brief This class extends an Expression to a Bitwise Or operator.
 */
class BitwiseOr : public Expression {
public:
  BitwiseOr(Module &Module, GNULDBackend &Backend, Expression &Left,
            Expression &Right)
      : Expression("|", Expression::BITWISE_OR, Module, Backend),
        LeftExpression(Left), RightExpression(Right) {}

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isBitWiseOR(); }

private:
  bool hasDot() const override;
  void commit() override;
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return &LeftExpression; }
  Expression *getRightExpression() const override { return &RightExpression; }

  Expression &LeftExpression;  /// represents the left hand operand.
  Expression &RightExpression; /// represents the right hand operand.
};

//===----------------------------------------------------------------------===//
/** \class BitwiseAnd
 *  \brief This class extends an Expression to a Bitwise And operator.
 */
class BitwiseAnd : public Expression {
public:
  BitwiseAnd(Module &Module, GNULDBackend &Backend, Expression &Left,
             Expression &Right)
      : Expression("&", Expression::BITWISE_AND, Module, Backend),
        LeftExpression(Left), RightExpression(Right) {}

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isBitWiseAND(); }

private:
  bool hasDot() const override;
  void commit() override;
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return &LeftExpression; }
  Expression *getRightExpression() const override { return &RightExpression; }

  Expression &LeftExpression;  /// represents the left hand operand.
  Expression &RightExpression; /// represents the right hand operand.
};

//===----------------------------------------------------------------------===//
/** \class BitwiseXor
 *  \brief This class extends an Expression to a Bitwise Xor operator.
 */
class BitwiseXor : public Expression {
public:
  BitwiseXor(Module &Module, GNULDBackend &Backend, Expression &Left,
             Expression &Right)
      : Expression("^", Expression::BITWISE_XOR, Module, Backend),
        LeftExpression(Left), RightExpression(Right) {}

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isBitWiseXOR(); }

private:
  bool hasDot() const override;
  void commit() override;
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return &LeftExpression; }
  Expression *getRightExpression() const override { return &RightExpression; }

  Expression &LeftExpression;  /// represents the left hand operand.
  Expression &RightExpression; /// represents the right hand operand.
};

//===----------------------------------------------------------------------===//
/** \class Defined
 *  \brief This class extends an Expression to a Defined operator.
 */
class Defined : public Expression {
public:
  Defined(Module &Module, GNULDBackend &Backend, std::string Name)
      : Expression(Name, Expression::DEFINED, Module, Backend) {}

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
  DataSegmentAlign(Module &Module, GNULDBackend &Backend,
                   Expression &MaxPageSize, Expression &CommonPageSize)
      : Expression("DATA_SEGMENT_ALIGN", Expression::DATA_SEGMENT_ALIGN, Module,
                   Backend),
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
  DataSegmentRelRoEnd(Module &Module, GNULDBackend &Backend, Expression &Expr1,
                      Expression &Expr2)
      : Expression("DATA_SEGMENT_RELRO_END", Expression::DATA_SEGMENT_RELRO_END,
                   Module, Backend),
        LeftExpression(Expr1), RightExpression(Expr2),
        CommonPageSize(*make<Constant>(Module, Backend, "COMMONPAGESIZE",
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
  DataSegmentEnd(Module &Module, GNULDBackend &Backend, Expression &Expr)
      : Expression("DATA_SEGMENT_END", Expression::DATA_SEGMENT_END, Module,
                   Backend),
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
/** \class Max
 *  \brief This class extends an Expression to a Max operator.
 */
class Max : public Expression {
public:
  Max(Module &Module, GNULDBackend &Backend, Expression &Left,
      Expression &Right)
      : Expression("MAX", Expression::MAX, Module, Backend),
        LeftExpression(Left), RightExpression(Right) {}

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isMax(); }

private:
  bool hasDot() const override;
  void commit() override;
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return &LeftExpression; }
  Expression *getRightExpression() const override { return &RightExpression; }

  Expression &LeftExpression;  /// represents the left hand operand.
  Expression &RightExpression; /// represents the right hand operand.
};

//===----------------------------------------------------------------------===//
/** \class Min
 *  \brief This class extends an Expression to a Min operator.
 */
class Min : public Expression {
public:
  Min(Module &Module, GNULDBackend &Backend, Expression &Left,
      Expression &Right)
      : Expression("MIN", Expression::MIN, Module, Backend),
        LeftExpression(Left), RightExpression(Right) {}

  // Casting support
  static bool classof(const Expression *Exp) { return Exp->isMin(); }

private:
  bool hasDot() const override;
  void commit() override;
  void dump(llvm::raw_ostream &Outs, bool WithValues = true) const override;
  eld::Expected<uint64_t> evalImpl() override;
  void getSymbols(std::vector<ResolveInfo *> &Symbols) override;
  void getSymbolNames(std::unordered_set<std::string> &SymbolTokens) override;
  Expression *getLeftExpression() const override { return &LeftExpression; }
  Expression *getRightExpression() const override { return &RightExpression; }

  Expression &LeftExpression;  /// represents the left hand operand.
  Expression &RightExpression; /// represents the right hand operand.
};

/** \class Fill
 *  \brief This class extends an Expression to a Fill operator.
 */
class Fill : public Expression {
public:
  Fill(Module &Module, GNULDBackend &Backend, Expression &Expr)
      : Expression("FILL", Expression::FILL, Module, Backend),
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
  Log2Ceil(Module &Module, GNULDBackend &Backend, Expression &Expr)
      : Expression("LOG2CEIL", Expression::LOG2CEIL, Module, Backend),
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

//===----------------------------------------------------------------------===//
/** \class LogicalOp (&& | ||)
 *  \brief This class extends an Expression to a logical operator.
 */
class LogicalOp : public Expression {
public:
  LogicalOp(Expression::Type Type, Module &Module, GNULDBackend &Backend,
            Expression &Left, Expression &Right)
      : Expression("LogicalOperator", Type, Module, Backend),
        LeftExpression(Left), RightExpression(Right) {}

  // Casting support
  static bool classof(const Expression *Exp) {
    return Exp->isLogicalAnd() || Exp->isLogicalOR();
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

  Expression &LeftExpression;  /// represents the left hand operand.
  Expression &RightExpression; /// represents the right hand operand.
};

//===----------------------------------------------------------------------===//
/** \class QueryMemory Command support  (ORIGIN/LENGTH)
 *  \brief This class extends an Expression to support memory command query
 *  capabiliites
 */
class QueryMemory : public Expression {
public:
  QueryMemory(Expression::Type Type, Module &Module, GNULDBackend &Backend,
              const std::string &Name);

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
  NullExpression(Module &Module, GNULDBackend &Backend);

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
