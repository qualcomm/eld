//===- Expression.cpp------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Script/Expression.h"
#include "DiagnosticEntry.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Core/Module.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Script/ScriptFile.h"
#include "eld/Support/MsgHandling.h"
#include "eld/Support/Utils.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/Target/ELFSegment.h"
#include "eld/Target/GNULDBackend.h"
#include "llvm/Support/MathExtras.h"
#include <optional>

using namespace eld;

Expression::Expression(std::string Name, Type Type, Module &Module,
                       GNULDBackend &Backend, uint64_t Value)
    : Name(Name), ThisType(Type), ThisModule(Module), ThisBackend(Backend),
      MResult(std::nullopt), EvaluatedValue(Value) {}

void Expression::setContext(const std::string &Context) {
  ASSERT(!Context.empty(), "Empty context for expression");
  MContext = Context;
}

uint64_t Expression::result() const {
  ASSERT(MResult, "Expression result is not yet committed");
  return *MResult;
}

uint64_t Expression::resultOrZero() const {
  if (hasResult())
    return MResult.value();
  return 0;
}

std::unique_ptr<plugin::DiagnosticEntry> Expression::addContextToDiagEntry(
    std::unique_ptr<plugin::DiagnosticEntry> DiagEntry,
    const std::string &Context) {
  // All messages raised from expressions must have %0 for the context, but
  // they are generated without the context.
  // I don't really like passing incomplete diag entries but this is how this
  // works now and these incomplete entries are contained inside this file. The
  // alternative would be to pass context to each and every expression during
  // creation time. I think that would require massive changes in the parser.
  std::vector<std::string> DiagArgs;
  DiagArgs.push_back(Context);
  DiagArgs.insert(DiagArgs.end(), DiagEntry->args().begin(),
                  DiagEntry->args().end());
  return std::make_unique<plugin::DiagnosticEntry>(DiagEntry->diagID(),
                                                   std::move(DiagArgs));
}

eld::Expected<uint64_t> Expression::evaluateAndReturnError() {
  // Each expression that may raise errors, should have its context set by
  // calling setContext(). A good place for this is ScriptCommand::activate().
  // This is unfortunate, but hopefully context for expressions will be set
  // during parsing.
  ASSERT(!MContext.empty(), "Context not set for expression");
  auto Result = eval();
  if (!Result)
    return addContextToDiagEntry(std::move(Result.error()), MContext);
  commit();
  return Result;
}

std::optional<uint64_t> Expression::evaluateAndRaiseError() {
  ASSERT(!MContext.empty(), "Context not set for expression");
  auto Result = eval();
  if (!Result) {
    // Even if evaluation fails, set the result (to zero) as
    // we don't expect the caller to exit due to this error.
    ThisModule.getConfig().raiseDiagEntry(
        addContextToDiagEntry(std::move(Result.error()), MContext));
    commit();
    return {};
  }
  commit();
  return Result.value();
}

eld::Expected<uint64_t> Expression::eval() {
  auto V = evalImpl();
  if (V)
    EvaluatedValue = V.value();
  return V;
}

void Expression::setContextRecursively(const std::string &Context) {
  setContext(Context);
  if (Expression *L = getLeftExpression())
    L->setContextRecursively(Context);
  if (Expression *R = getRightExpression())
    R->setContextRecursively(Context);
}

//===----------------------------------------------------------------------===//
/// Symbol Operand
Symbol::Symbol(Module &CurModule, GNULDBackend &PBackend, std::string PName)
    : Expression(PName, Expression::SYMBOL, CurModule, PBackend),
      ThisSymbol(nullptr) {}

void Symbol::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  // format output for operand
  Outs << Name;
  if (WithValues) {
    Outs << "(0x";
    Outs.write_hex(*MResult) << ")";
  }
}

bool Symbol::hasDot() const {
  if (!ThisSymbol)
    ThisSymbol = ThisModule.getNamePool().findSymbol(Name);
  return ThisSymbol == ThisModule.getDotSymbol();
}

eld::Expected<uint64_t> Symbol::evalImpl() {

  if (!ThisSymbol)
    ThisSymbol = ThisModule.getNamePool().findSymbol(Name);

  if (!ThisSymbol || ThisSymbol->resolveInfo()->isUndef() ||
      ThisSymbol->resolveInfo()->isBitCode())
    return std::make_unique<plugin::DiagnosticEntry>(
        Diag::undefined_symbol_in_linker_script,
        std::vector<std::string>{Name});

  if (ThisSymbol->hasFragRef() && !ThisSymbol->shouldIgnore()) {
    FragmentRef *FragRef = ThisSymbol->fragRef();
    ELFSection *Section = FragRef->getOutputELFSection();
    bool IsAllocSection = Section ? Section->isAlloc() : false;

    ASSERT(IsAllocSection,
           "using a symbol that points to a non allocatable section!");
    return Section->addr() + FragRef->getOutputOffset(ThisModule);
  }
  return ThisSymbol->value();
}

void Symbol::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  ResolveInfo *R = ThisModule.getNamePool().findInfo(Name);
  if (R == nullptr) {
    return;
  }
  // Dont add DOT symbols.
  if (R == ThisModule.getDotSymbol()->resolveInfo())
    return;
  Symbols.push_back(R);
}

void Symbol::getSymbolNames(std::unordered_set<std::string> &SymbolTokens) {
  // Dont add DOT symbols.
  if (Name == ThisModule.getDotSymbol()->resolveInfo()->name())
    return;
  SymbolTokens.insert(Name);
}

//===----------------------------------------------------------------------===//
/// Integer Operand
void Integer::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (ExpressionHasParenthesis)
    Outs << "(";
  // format output for operand
  Outs << "0x";
  Outs.write_hex(ExpressionValue);
  if (ExpressionHasParenthesis)
    Outs << ")";
}
eld::Expected<uint64_t> Integer::evalImpl() { return ExpressionValue; }
void Integer::getSymbols(std::vector<ResolveInfo *> &Symbols) {}

void Integer::getSymbolNames(std::unordered_set<std::string> &SymbolTokens) {}

//===----------------------------------------------------------------------===//
/// Add Operator
void Add::commit() {
  LeftExpression.commit();
  RightExpression.commit();
  Expression::commit();
}
void Add::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  // format output for operator
  if (ExpressionHasParenthesis)
    Outs << "(";
  if (!hasAssign()) {
    LeftExpression.dump(Outs, WithValues);
    Outs << " " << Name << " ";
  }
  RightExpression.dump(Outs, WithValues);
  if (ExpressionHasParenthesis)
    Outs << ")";
}
eld::Expected<uint64_t> Add::evalImpl() {
  // evaluate sub expressions
  auto Left = LeftExpression.eval();
  if (!Left)
    return Left;
  auto Right = RightExpression.eval();
  if (!Right)
    return Right;
  if (!ThisModule.getScript().phdrsSpecified() &&
      RightExpression.isSizeOfHeaders()) {
    if (ThisModule.getDotSymbol() &&
        ThisModule.getDotSymbol()->value() == ThisBackend.getImageStartVMA()) {
      // Load file headers and program header
      ThisBackend.setNeedEhdr();
      ThisBackend.setNeedPhdr();
    }
  }
  return Left.value() + Right.value();
}

bool Add::hasDot() const {
  return LeftExpression.hasDot() || RightExpression.hasDot();
}

void Add::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  LeftExpression.getSymbols(Symbols);
  RightExpression.getSymbols(Symbols);
}

void Add::getSymbolNames(std::unordered_set<std::string> &SymbolTokens) {
  LeftExpression.getSymbolNames(SymbolTokens);
  RightExpression.getSymbolNames(SymbolTokens);
}

//===----------------------------------------------------------------------===//
/// Subtract Operator
void Subtract::commit() {
  LeftExpression.commit();
  RightExpression.commit();
  Expression::commit();
}
void Subtract::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (ExpressionHasParenthesis)
    Outs << "(";
  if (!hasAssign()) {
    // format output for operator
    LeftExpression.dump(Outs, WithValues);
    Outs << " " << Name << " ";
  }
  RightExpression.dump(Outs, WithValues);
  if (ExpressionHasParenthesis)
    Outs << ")";
}
eld::Expected<uint64_t> Subtract::evalImpl() {
  // evaluate sub expressions
  auto Left = LeftExpression.eval();
  if (!Left)
    return Left;
  auto Right = RightExpression.eval();
  if (!Right)
    return Right;
  return Left.value() - Right.value();
}
void Subtract::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  LeftExpression.getSymbols(Symbols);
  RightExpression.getSymbols(Symbols);
}

void Subtract::getSymbolNames(std::unordered_set<std::string> &SymbolTokens) {
  LeftExpression.getSymbolNames(SymbolTokens);
  RightExpression.getSymbolNames(SymbolTokens);
}
bool Subtract::hasDot() const {
  return LeftExpression.hasDot() || RightExpression.hasDot();
}

//===----------------------------------------------------------------------===//
/// Modulo Operator
void Modulo::commit() {
  LeftExpression.commit();
  RightExpression.commit();
  Expression::commit();
}
void Modulo::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (ExpressionHasParenthesis)
    Outs << "(";
  // format output for operator
  LeftExpression.dump(Outs, WithValues);
  Outs << " " << Name << " ";
  RightExpression.dump(Outs, WithValues);
  if (ExpressionHasParenthesis)
    Outs << ")";
}
eld::Expected<uint64_t> Modulo::evalImpl() {
  // evaluate sub expressions
  auto Left = LeftExpression.eval();
  if (!Left)
    return Left;
  auto Right = RightExpression.eval();
  if (!Right)
    return Right;
  if (Right.value() == 0) {
    std::string ErrorString;
    llvm::raw_string_ostream ErrorStringStream(ErrorString);
    dump(ErrorStringStream);
    return std::make_unique<plugin::DiagnosticEntry>(
        Diag::fatal_modulo_by_zero,
        std::vector<std::string>{ErrorStringStream.str()});
  }
  return Left.value() % Right.value();
}
void Modulo::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  LeftExpression.getSymbols(Symbols);
  RightExpression.getSymbols(Symbols);
}

void Modulo::getSymbolNames(std::unordered_set<std::string> &SymbolTokens) {
  LeftExpression.getSymbolNames(SymbolTokens);
  RightExpression.getSymbolNames(SymbolTokens);
}

bool Modulo::hasDot() const {
  return LeftExpression.hasDot() || RightExpression.hasDot();
}

//===----------------------------------------------------------------------===//
/// Multiply Operator
void Multiply::commit() {
  LeftExpression.commit();
  RightExpression.commit();
  Expression::commit();
}
void Multiply::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (ExpressionHasParenthesis)
    Outs << "(";
  if (!hasAssign()) {
    // format output for operator
    LeftExpression.dump(Outs, WithValues);
    Outs << " " << Name << " ";
  }
  RightExpression.dump(Outs, WithValues);
  if (ExpressionHasParenthesis)
    Outs << ")";
}
eld::Expected<uint64_t> Multiply::evalImpl() {
  // evaluate sub expressions
  auto Left = LeftExpression.eval();
  if (!Left)
    return Left;
  auto Right = RightExpression.eval();
  if (!Right)
    return Right;
  return Left.value() * Right.value();
}
void Multiply::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  LeftExpression.getSymbols(Symbols);
  RightExpression.getSymbols(Symbols);
}

void Multiply::getSymbolNames(std::unordered_set<std::string> &SymbolTokens) {
  LeftExpression.getSymbolNames(SymbolTokens);
  RightExpression.getSymbolNames(SymbolTokens);
}

bool Multiply::hasDot() const {
  return LeftExpression.hasDot() || RightExpression.hasDot();
}

//===----------------------------------------------------------------------===//
/// Divide Operator
void Divide::commit() {
  LeftExpression.commit();
  RightExpression.commit();
  Expression::commit();
}
void Divide::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (ExpressionHasParenthesis)
    Outs << "(";
  if (!hasAssign()) {
    // format output for operator
    LeftExpression.dump(Outs, WithValues);
    Outs << " " << Name << " ";
  }
  RightExpression.dump(Outs, WithValues);
  if (ExpressionHasParenthesis)
    Outs << ")";
}
eld::Expected<uint64_t> Divide::evalImpl() {
  // evaluate sub expressions
  auto Left = LeftExpression.eval();
  if (!Left)
    return Left;
  auto Right = RightExpression.eval();
  if (!Right)
    return Right;
  if (Right.value() == 0) {
    std::string ErrorString;
    llvm::raw_string_ostream ErrorStringStream(ErrorString);
    dump(ErrorStringStream);
    return std::make_unique<plugin::DiagnosticEntry>(
        Diag::fatal_divide_by_zero,
        std::vector<std::string>{ErrorStringStream.str()});
  }
  return Left.value() / Right.value();
}
void Divide::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  LeftExpression.getSymbols(Symbols);
  RightExpression.getSymbols(Symbols);
}

void Divide::getSymbolNames(std::unordered_set<std::string> &SymbolTokens) {
  LeftExpression.getSymbolNames(SymbolTokens);
  RightExpression.getSymbolNames(SymbolTokens);
}

bool Divide::hasDot() const {
  return LeftExpression.hasDot() || RightExpression.hasDot();
}

//===----------------------------------------------------------------------===//
/// SizeOf
SizeOf::SizeOf(Module &CurModule, GNULDBackend &PBackend, std::string PName)
    : Expression(PName, Expression::SIZEOF, CurModule, PBackend),
      ThisSection(nullptr) {}
void SizeOf::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  Outs << "SIZEOF(" << Name;
  if (WithValues) {
    Outs << " = 0x";
    Outs.write_hex(*MResult);
  }
  Outs << ")";
}
eld::Expected<uint64_t> SizeOf::evalImpl() {

  if (Name.size() && Name[0] == ':') {
    // If the name is a segment and we don't have PHDR's. SIZEOF on segment will
    // not work.
    if (!ThisModule.getScript().phdrsSpecified())
      return std::make_unique<plugin::DiagnosticEntry>(
          Diag::size_of_used_without_phdrs);

    // If a segment is specified, lets check the segment table for a segment
    // that exists.
    std::string SegmentName = Name.substr(1);
    if (ELFSegment *Seg = ThisBackend.findSegment(SegmentName)) {
      ThisBackend.setupSegmentOffset(Seg);
      ThisBackend.setupSegment(Seg);
      ThisBackend.clearSegmentOffset(Seg);
      return Seg->filesz();
    }

    return std::make_unique<plugin::DiagnosticEntry>(
        Diag::fatal_segment_not_defined_ldscript,
        std::vector<std::string>{SegmentName});
  }
  // As the section table is populated only during PostLayout, we have to
  // go the other way around to access the section. This is because size of
  // empty
  // sections are known only after all the assignments are complete
  if (ThisSection == nullptr)
    ThisSection = ThisModule.getScript().sectionMap().find(Name);
  if (ThisSection == nullptr)
    return std::make_unique<plugin::DiagnosticEntry>(
        Diag::undefined_symbol_in_linker_script,
        std::vector<std::string>{Name});

  // NOTE: output sections with no content or those that have been garbaged
  // collected will not be in the Module SectionTable, therefore the size
  // will automatically default to zero (from initialization)
  return ThisSection->size();
}

void SizeOf::getSymbols(std::vector<ResolveInfo *> &Symbols) {}

void SizeOf::getSymbolNames(std::unordered_set<std::string> &SymbolTokens) {}

//===----------------------------------------------------------------------===//
/// SizeOfHeaders
SizeOfHeaders::SizeOfHeaders(Module &CurModule, GNULDBackend &PBackend,
                             ScriptFile *S)
    : Expression("SIZEOF_HEADERS", Expression::SIZEOF_HEADERS, CurModule,
                 PBackend) {
  // SIZEOF_HEADERS is an insane command. If its at the beginning of the script,
  // the BFD linker sees that there is an empty hole created before the first
  // section begins and inserts program headers and loads them. ELD tries to be
  // do a simpler implementation of the same.
  if (!S->firstLinkerScriptWithOutputSection())
    ThisModule.getScript().setSizeOfHeader();
}

void SizeOfHeaders::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  Outs << "SIZEOF_HEADERS";
  if (WithValues) {
    Outs << "("
         << " = 0x";
    Outs.write_hex(*MResult) << ")";
  }
}

eld::Expected<uint64_t> SizeOfHeaders::evalImpl() {
  uint64_t Offset = 0;
  std::vector<ELFSection *> Sections;
  if (!ThisBackend.isEhdrNeeded())
    Sections.push_back(ThisBackend.getEhdr());
  if (!ThisBackend.isPhdrNeeded())
    Sections.push_back(ThisBackend.getPhdr());
  for (auto &S : Sections) {
    if (!S)
      continue;
    Offset = Offset + S->size();
  }
  return Offset;
}

void SizeOfHeaders::getSymbols(std::vector<ResolveInfo *> &Symbols) {}

void SizeOfHeaders::getSymbolNames(
    std::unordered_set<std::string> &SymbolTokens) {}

//===----------------------------------------------------------------------===//
/// Addr
void Addr::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  Outs << "ADDR(\"" << Name;
  if (WithValues) {
    Outs << " = 0x";
    Outs.write_hex(*MResult);
  }
  Outs << "\")";
}
eld::Expected<uint64_t> Addr::evalImpl() {
  // As the section table is populated only during PostLayout, we have to
  // go the other way around to access the section. This is because size of
  // empty
  // sections are known only after all the assignments are complete
  if (ThisSection == nullptr)
    ThisSection = ThisModule.getScript().sectionMap().find(Name);
  if (ThisSection == nullptr)
    return std::make_unique<plugin::DiagnosticEntry>(
        Diag::undefined_symbol_in_linker_script,
        std::vector<std::string>{Name});
  if (!ThisSection->hasVMA())
    ThisModule.getConfig().raise(Diag::warn_forward_reference)
        << MContext << Name;
  // evaluate sub expression
  return ThisSection->addr();
}

void Addr::getSymbols(std::vector<ResolveInfo *> &Symbols) {}

void Addr::getSymbolNames(std::unordered_set<std::string> &SymbolTokens) {}

//===----------------------------------------------------------------------===//
/// LoadAddr
LoadAddr::LoadAddr(Module &Module, GNULDBackend &Backend, std::string Name)
    : Expression(Name, Expression::LOADADDR, Module, Backend),
      ThisSection(nullptr) {
  ForwardReference = !Module.findInOutputSectionDescNameSet(Name);
}

void LoadAddr::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  Outs << "LOADADDR(" << Name;
  if (WithValues) {
    Outs << " = 0x";
    Outs.write_hex(*MResult);
  }
  Outs << ")";
}
eld::Expected<uint64_t> LoadAddr::evalImpl() {
  // As the section table is populated only during PostLayout, we have to
  // go the other way around to access the section. This is because size of
  // empty
  // sections are known only after all the assignments are complete
  if (ThisSection == nullptr)
    ThisSection = ThisModule.getScript().sectionMap().find(Name);
  if (ThisSection == nullptr)
    return std::make_unique<plugin::DiagnosticEntry>(
        Diag::undefined_symbol_in_linker_script,
        std::vector<std::string>{Name});
  if (ForwardReference && ThisModule.getConfig().showBadDotAssignmentWarnings())
    return std::make_unique<plugin::DiagnosticEntry>(
        Diag::warn_forward_reference, std::vector<std::string>{Name});
  // evaluate sub expression
  return ThisSection->pAddr();
}
void LoadAddr::getSymbols(std::vector<ResolveInfo *> &Symbols) {}

void LoadAddr::getSymbolNames(std::unordered_set<std::string> &SymbolTokens) {}
//===----------------------------------------------------------------------===//
/// OffsetOf
void OffsetOf::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  Outs << "OFFSETOF(" << Name;
  if (WithValues) {
    Outs << " = 0x";
    Outs.write_hex(*MResult);
  }
}
eld::Expected<uint64_t> OffsetOf::evalImpl() {
  // As the section table is populated only during PostLayout, we have to
  // go the other way around to access the section. This is because size of
  // empty
  // sections are known only after all the assignments are complete
  if (ThisSection == nullptr)
    ThisSection = ThisModule.getScript().sectionMap().find(Name);
  assert(ThisSection != nullptr);
  // evaluate sub expression
  if (ThisSection->hasOffset())
    return ThisSection->offset();
  return 0;
}
void OffsetOf::getSymbols(std::vector<ResolveInfo *> &Symbols) {}

void OffsetOf::getSymbolNames(std::unordered_set<std::string> &SymbolTokens) {}

//===----------------------------------------------------------------------===//
/// Ternary
void Ternary::commit() {
  ConditionExpression.commit();
  LeftExpression.commit();
  RightExpression.commit();
  Expression::commit();
}
void Ternary::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (ExpressionHasParenthesis)
    Outs << "(";
  ConditionExpression.dump(Outs, WithValues);
  Outs << " ? ";
  LeftExpression.dump(Outs, WithValues);
  Outs << " : ";
  RightExpression.dump(Outs, WithValues);
  if (ExpressionHasParenthesis)
    Outs << ")";
}
eld::Expected<uint64_t> Ternary::evalImpl() {
  auto Cond = ConditionExpression.eval();
  if (!Cond)
    return Cond;
  return Cond.value() ? LeftExpression.eval() : RightExpression.eval();
}
void Ternary::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  ConditionExpression.getSymbols(Symbols);
  if (ConditionExpression.result())
    LeftExpression.getSymbols(Symbols);
  else
    RightExpression.getSymbols(Symbols);
}

void Ternary::getSymbolNames(std::unordered_set<std::string> &SymbolTokens) {
  ConditionExpression.getSymbolNames(SymbolTokens);
  // FIXME: Ideally, one of the LeftExpression / RightExpression should
  // be selected depending upon the ConditionExpression evaluation. However,
  // we need to add undefined symbols before the expressions are ready to be
  // evaluated. It is safer to add an extra (redundant) symbol as compared to
  // not adding a required symbol, as the later can cause an undefined reference
  // error.
  LeftExpression.getSymbolNames(SymbolTokens);
  RightExpression.getSymbolNames(SymbolTokens);
}

bool Ternary::hasDot() const {
  return ConditionExpression.hasDot() || LeftExpression.hasDot() ||
         RightExpression.hasDot();
}

//===----------------------------------------------------------------------===//
/// AlignExpr Operator
void AlignExpr::commit() {
  ExpressionToEvaluate.commit();
  AlignmentExpression.commit();
  Expression::commit();
}
void AlignExpr::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  // format output for operator
  Outs << Name << "(";
  ExpressionToEvaluate.dump(Outs, WithValues);
  Outs << ", ";
  AlignmentExpression.dump(Outs, WithValues);
  Outs << ")";
}
eld::Expected<uint64_t> AlignExpr::evalImpl() {
  // evaluate sub expressions
  auto Expr = ExpressionToEvaluate.eval();
  if (!Expr)
    return Expr;
  auto Align = AlignmentExpression.eval();
  if (!Align)
    return Align;
  uint64_t Value = Expr.value();
  uint64_t AlignValue = Align.value();
  if (!AlignValue)
    return Value;
  if (ThisModule.getConfig().showLinkerScriptWarnings() &&
      !llvm::isPowerOf2_64(AlignValue))
    ThisModule.getConfig().raise(
        Diag::warn_non_power_of_2_value_to_align_builtin)
        << getContext() << utility::toHex(AlignValue);
  return llvm::alignTo(Value, AlignValue);
}

void AlignExpr::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  ExpressionToEvaluate.getSymbols(Symbols);
}

void AlignExpr::getSymbolNames(std::unordered_set<std::string> &SymbolTokens) {
  ExpressionToEvaluate.getSymbolNames(SymbolTokens);
  AlignmentExpression.getSymbolNames(SymbolTokens);
}

bool AlignExpr::hasDot() const {
  return ExpressionToEvaluate.hasDot() || AlignmentExpression.hasDot();
}

//===----------------------------------------------------------------------===//
/// AlignOf Operator
void AlignOf::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  // format output for operator
  Outs << "ALIGNOF(" << Name << ")";
}
void AlignOf::getSymbols(std::vector<ResolveInfo *> &Symbols) {}

void AlignOf::getSymbolNames(std::unordered_set<std::string> &SymbolTokens) {}

eld::Expected<uint64_t> AlignOf::evalImpl() {
  // As the section table is populated only during PostLayout, we have to
  // go the other way around to access the section. This is because size of
  // empty
  // sections are known only after all the assignments are complete
  if (ThisSection == nullptr)
    ThisSection = ThisModule.getScript().sectionMap().find(Name);
  assert(ThisSection != nullptr);
  // evaluate sub expressions
  return ThisSection->getAddrAlign();
}

//===----------------------------------------------------------------------===//
/// Absolute Operator
void Absolute::commit() {
  ExpressionToEvaluate.commit();
  Expression::commit();
}
void Absolute::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  // format output for operator
  Outs << "ABSOLUTE(";
  ExpressionToEvaluate.dump(Outs, WithValues);
  Outs << ")";
}
eld::Expected<uint64_t> Absolute::evalImpl() {
  return ExpressionToEvaluate.eval();
}
void Absolute::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  ExpressionToEvaluate.getSymbols(Symbols);
}

void Absolute::getSymbolNames(std::unordered_set<std::string> &SymbolTokens) {
  ExpressionToEvaluate.getSymbolNames(SymbolTokens);
}

bool Absolute::hasDot() const { return ExpressionToEvaluate.hasDot(); }

//===----------------------------------------------------------------------===//
/// ConditionGT Operator
void ConditionGT::commit() {
  LeftExpression.commit();
  RightExpression.commit();
  Expression::commit();
}
void ConditionGT::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (ExpressionHasParenthesis)
    Outs << "(";
  // format output for operator
  LeftExpression.dump(Outs, WithValues);
  Outs << " " << Name << " ";
  RightExpression.dump(Outs, WithValues);
  if (ExpressionHasParenthesis)
    Outs << ")";
}
eld::Expected<uint64_t> ConditionGT::evalImpl() {
  auto Left = LeftExpression.eval();
  if (!Left)
    return Left;
  auto Right = RightExpression.eval();
  if (!Right)
    return Right;
  return Left.value() > Right.value();
}
void ConditionGT::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  LeftExpression.getSymbols(Symbols);
  RightExpression.getSymbols(Symbols);
}

void ConditionGT::getSymbolNames(
    std::unordered_set<std::string> &SymbolTokens) {
  LeftExpression.getSymbolNames(SymbolTokens);
  RightExpression.getSymbolNames(SymbolTokens);
}

bool ConditionGT::hasDot() const {
  return LeftExpression.hasDot() || RightExpression.hasDot();
}

//===----------------------------------------------------------------------===//
/// ConditionLT Operator
void ConditionLT::commit() {
  LeftExpression.commit();
  RightExpression.commit();
  Expression::commit();
}
void ConditionLT::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (ExpressionHasParenthesis)
    Outs << "(";
  // format output for operator
  LeftExpression.dump(Outs, WithValues);
  Outs << " " << Name << " ";
  RightExpression.dump(Outs, WithValues);
  if (ExpressionHasParenthesis)
    Outs << ")";
}
eld::Expected<uint64_t> ConditionLT::evalImpl() {
  // evaluate sub expressions
  auto Left = LeftExpression.eval();
  if (!Left)
    return Left;
  auto Right = RightExpression.eval();
  if (!Right)
    return Right;
  return Left.value() < Right.value();
}
void ConditionLT::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  LeftExpression.getSymbols(Symbols);
  RightExpression.getSymbols(Symbols);
}

void ConditionLT::getSymbolNames(
    std::unordered_set<std::string> &SymbolTokens) {
  LeftExpression.getSymbolNames(SymbolTokens);
  RightExpression.getSymbolNames(SymbolTokens);
}

bool ConditionLT::hasDot() const {
  return LeftExpression.hasDot() || RightExpression.hasDot();
}

//===----------------------------------------------------------------------===//
/// ConditionEQ Operator
void ConditionEQ::commit() {
  LeftExpression.commit();
  RightExpression.commit();
  Expression::commit();
}
void ConditionEQ::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (ExpressionHasParenthesis)
    Outs << "(";
  // format output for operator
  LeftExpression.dump(Outs, WithValues);
  Outs << " " << Name << " ";
  RightExpression.dump(Outs, WithValues);
  if (ExpressionHasParenthesis)
    Outs << ")";
}
eld::Expected<uint64_t> ConditionEQ::evalImpl() {
  // evaluate sub expressions
  auto Left = LeftExpression.eval();
  if (!Left)
    return Left;
  auto Right = RightExpression.eval();
  if (!Right)
    return Right;
  return Left.value() == Right.value();
}
void ConditionEQ::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  LeftExpression.getSymbols(Symbols);
  RightExpression.getSymbols(Symbols);
}

void ConditionEQ::getSymbolNames(
    std::unordered_set<std::string> &SymbolTokens) {
  LeftExpression.getSymbolNames(SymbolTokens);
  RightExpression.getSymbolNames(SymbolTokens);
}

bool ConditionEQ::hasDot() const {
  return LeftExpression.hasDot() || RightExpression.hasDot();
}

//===----------------------------------------------------------------------===//
/// ConditionGTE Operator
void ConditionGTE::commit() {
  LeftExpression.commit();
  RightExpression.commit();
  Expression::commit();
}
void ConditionGTE::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (ExpressionHasParenthesis)
    Outs << "(";
  // format output for operator
  LeftExpression.dump(Outs, WithValues);
  Outs << " " << Name << " ";
  RightExpression.dump(Outs, WithValues);
  if (ExpressionHasParenthesis)
    Outs << ")";
}
eld::Expected<uint64_t> ConditionGTE::evalImpl() {
  // evaluate sub expressions
  auto Left = LeftExpression.eval();
  if (!Left)
    return Left;
  auto Right = RightExpression.eval();
  if (!Right)
    return Right;
  return Left.value() >= Right.value();
}
void ConditionGTE::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  LeftExpression.getSymbols(Symbols);
  RightExpression.getSymbols(Symbols);
}

void ConditionGTE::getSymbolNames(
    std::unordered_set<std::string> &SymbolTokens) {
  LeftExpression.getSymbolNames(SymbolTokens);
  RightExpression.getSymbolNames(SymbolTokens);
}

bool ConditionGTE::hasDot() const {
  return LeftExpression.hasDot() || RightExpression.hasDot();
}

//===----------------------------------------------------------------------===//
/// ConditionLTE Operator
void ConditionLTE::commit() {
  LeftExpression.commit();
  RightExpression.commit();
  Expression::commit();
}
void ConditionLTE::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (ExpressionHasParenthesis)
    Outs << "(";
  // format output for operator
  LeftExpression.dump(Outs, WithValues);
  Outs << " " << Name << " ";
  RightExpression.dump(Outs, WithValues);
  if (ExpressionHasParenthesis)
    Outs << ")";
}
eld::Expected<uint64_t> ConditionLTE::evalImpl() {
  // evaluate sub expressions
  auto Left = LeftExpression.eval();
  if (!Left)
    return Left;
  auto Right = RightExpression.eval();
  if (!Right)
    return Right;
  return Left.value() <= Right.value();
}
void ConditionLTE::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  LeftExpression.getSymbols(Symbols);
  RightExpression.getSymbols(Symbols);
}

void ConditionLTE::getSymbolNames(
    std::unordered_set<std::string> &SymbolTokens) {
  LeftExpression.getSymbolNames(SymbolTokens);
  RightExpression.getSymbolNames(SymbolTokens);
}

bool ConditionLTE::hasDot() const {
  return LeftExpression.hasDot() || RightExpression.hasDot();
}

//===----------------------------------------------------------------------===//
/// ConditionNEQ Operator
void ConditionNEQ::commit() {
  LeftExpression.commit();
  RightExpression.commit();
  Expression::commit();
}
void ConditionNEQ::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (ExpressionHasParenthesis)
    Outs << "(";
  // format output for operator
  LeftExpression.dump(Outs, WithValues);
  Outs << " " << Name << " ";
  RightExpression.dump(Outs, WithValues);
  if (ExpressionHasParenthesis)
    Outs << ")";
}
eld::Expected<uint64_t> ConditionNEQ::evalImpl() {
  // evaluate sub expressions
  auto Left = LeftExpression.eval();
  if (!Left)
    return Left;
  auto Right = RightExpression.eval();
  if (!Right)
    return Right;
  return Left.value() != Right.value();
}
void ConditionNEQ::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  LeftExpression.getSymbols(Symbols);
  RightExpression.getSymbols(Symbols);
}

void ConditionNEQ::getSymbolNames(
    std::unordered_set<std::string> &SymbolTokens) {
  LeftExpression.getSymbolNames(SymbolTokens);
  RightExpression.getSymbolNames(SymbolTokens);
}

bool ConditionNEQ::hasDot() const {
  return LeftExpression.hasDot() || RightExpression.hasDot();
}

//===----------------------------------------------------------------------===//
/// Complement Operator
void Complement::commit() {
  ExpressionToEvaluate.commit();
  Expression::commit();
}
void Complement::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  // format output for operator
  Outs << Name;
  ExpressionToEvaluate.dump(Outs, WithValues);
}
eld::Expected<uint64_t> Complement::evalImpl() {
  // evaluate sub expressions
  auto Expr = ExpressionToEvaluate.eval();
  if (!Expr)
    return Expr;
  return ~Expr.value();
}
void Complement::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  ExpressionToEvaluate.getSymbols(Symbols);
}

void Complement::getSymbolNames(std::unordered_set<std::string> &SymbolTokens) {
  ExpressionToEvaluate.getSymbolNames(SymbolTokens);
}

bool Complement::hasDot() const { return ExpressionToEvaluate.hasDot(); }

//===----------------------------------------------------------------------===//
/// Unary plus operator
void UnaryPlus::commit() {
  ExpressionToEvaluate.commit();
  Expression::commit();
}
void UnaryPlus::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  // format output for operator
  Outs << Name;
  ExpressionToEvaluate.dump(Outs, WithValues);
}
eld::Expected<uint64_t> UnaryPlus::evalImpl() {
  return ExpressionToEvaluate.eval();
}
void UnaryPlus::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  ExpressionToEvaluate.getSymbols(Symbols);
}

void UnaryPlus::getSymbolNames(std::unordered_set<std::string> &SymbolTokens) {
  ExpressionToEvaluate.getSymbolNames(SymbolTokens);
}

bool UnaryPlus::hasDot() const { return ExpressionToEvaluate.hasDot(); }
//===----------------------------------------------------------------------===//
/// Unary minus operator
void UnaryMinus::commit() {
  ExpressionToEvaluate.commit();
  Expression::commit();
}
void UnaryMinus::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  // format output for operator
  Outs << "-";
  ExpressionToEvaluate.dump(Outs, WithValues);
}
eld::Expected<uint64_t> UnaryMinus::evalImpl() {
  // evaluate sub expressions
  auto Expr = ExpressionToEvaluate.eval();
  if (!Expr)
    return Expr;
  return -Expr.value();
}
void UnaryMinus::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  ExpressionToEvaluate.getSymbols(Symbols);
}

void UnaryMinus::getSymbolNames(std::unordered_set<std::string> &SymbolTokens) {
  ExpressionToEvaluate.getSymbolNames(SymbolTokens);
}

bool UnaryMinus::hasDot() const { return ExpressionToEvaluate.hasDot(); }
//===----------------------------------------------------------------------===//
/// Unary not operator
void UnaryNot::commit() {
  ExpressionToEvaluate.commit();
  Expression::commit();
}
void UnaryNot::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  // format output for operator
  Outs << Name;
  ExpressionToEvaluate.dump(Outs, WithValues);
}
eld::Expected<uint64_t> UnaryNot::evalImpl() {
  // evaluate sub expressions
  auto Expr = ExpressionToEvaluate.eval();
  if (!Expr)
    return Expr;
  return !Expr.value();
}
void UnaryNot::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  ExpressionToEvaluate.getSymbols(Symbols);
}

void UnaryNot::getSymbolNames(std::unordered_set<std::string> &SymbolTokens) {
  ExpressionToEvaluate.getSymbolNames(SymbolTokens);
}

bool UnaryNot::hasDot() const { return ExpressionToEvaluate.hasDot(); }
//===----------------------------------------------------------------------===//
/// Constant Operator
void Constant::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  // format output for operator
  Outs << "CONSTANT(" << Name << ")";
}
eld::Expected<uint64_t> Constant::evalImpl() {
  // evaluate sub expressions
  switch (type()) {
  case Expression::MAXPAGESIZE:
    return ThisBackend.abiPageSize();
  case Expression::COMMONPAGESIZE:
    return ThisBackend.commonPageSize();
  default:
    // this can't happen because all constants are part of the syntax
    ASSERT(0, "Unexpected constant");
    return {};
  }
}
void Constant::getSymbols(std::vector<ResolveInfo *> &Symbols) {}

void Constant::getSymbolNames(std::unordered_set<std::string> &SymbolTokens) {}

//===----------------------------------------------------------------------===//
/// SegmentStart Operator
void SegmentStart::commit() {
  ExpressionToEvaluate.commit();
  Expression::commit();
}
void SegmentStart::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  // format output for operator
  Outs << "SEGMENT_START(\"" << SegmentName << "\", ";
  ExpressionToEvaluate.dump(Outs, WithValues);
  Outs << ")";
}
eld::Expected<uint64_t> SegmentStart::evalImpl() {
  GeneralOptions::AddressMapType &AddressMap =
      ThisModule.getConfig().options().addressMap();
  GeneralOptions::AddressMapType::const_iterator Addr;

  if (SegmentName.compare("text-segment") == 0)
    Addr = AddressMap.find(".text");
  else if (SegmentName.compare("data-segment") == 0)
    Addr = AddressMap.find(".data");
  else if (SegmentName.compare("bss-segment") == 0)
    Addr = AddressMap.find(".bss");
  else
    Addr = AddressMap.find(SegmentName);

  if (Addr != AddressMap.end())
    return Addr->getValue();
  return ExpressionToEvaluate.eval();
}
void SegmentStart::getSymbols(std::vector<ResolveInfo *> &Symbols) {}

void SegmentStart::getSymbolNames(
    std::unordered_set<std::string> &SymbolTokens) {}

bool SegmentStart::hasDot() const { return ExpressionToEvaluate.hasDot(); }

//===----------------------------------------------------------------------===//
/// AssertCmd Operator
void AssertCmd::commit() {
  ExpressionToEvaluate.commit();
  Expression::commit();

  // NOTE: we perform the Assertion during commit so if we want to debug a
  // linker
  // script we can just evaluate it without having to worry about asserts
  // causing
  // errors during evaluation
  if (MResult == 0) {
    std::string Buf;
    llvm::raw_string_ostream Os(Buf);
    dump(Os);
    ThisModule.getConfig().raise(Diag::assert_failed) << Buf;
  }
}
void AssertCmd::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  // format output for operator
  Outs << Name << "(";
  ExpressionToEvaluate.dump(Outs, WithValues);
  Outs << ", \"" << AssertionMessage << "\")";
}
eld::Expected<uint64_t> AssertCmd::evalImpl() {
  auto Expr = ExpressionToEvaluate.eval();
  if (!Expr)
    return Expr;
  return Expr.value() != 0;
}
void AssertCmd::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  ExpressionToEvaluate.getSymbols(Symbols);
}

void AssertCmd::getSymbolNames(std::unordered_set<std::string> &SymbolTokens) {
  ExpressionToEvaluate.getSymbolNames(SymbolTokens);
}

bool AssertCmd::hasDot() const { return ExpressionToEvaluate.hasDot(); }

//===----------------------------------------------------------------------===//
/// RightShift Operator
void RightShift::commit() {
  LeftExpression.commit();
  RightExpression.commit();
  Expression::commit();
}
void RightShift::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (ExpressionHasParenthesis)
    Outs << "(";
  if (!hasAssign()) {
    // format output for operator
    LeftExpression.dump(Outs, WithValues);
    Outs << " " << Name << " ";
  }
  RightExpression.dump(Outs, WithValues);
  if (ExpressionHasParenthesis)
    Outs << ")";
}
eld::Expected<uint64_t> RightShift::evalImpl() {
  // evaluate sub expressions
  auto Left = LeftExpression.eval();
  if (!Left)
    return Left;
  auto Right = RightExpression.eval();
  if (!Right)
    return Right;
  return Left.value() >> Right.value();
}

void RightShift::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  LeftExpression.getSymbols(Symbols);
  RightExpression.getSymbols(Symbols);
}

void RightShift::getSymbolNames(std::unordered_set<std::string> &SymbolTokens) {
  LeftExpression.getSymbolNames(SymbolTokens);
  RightExpression.getSymbolNames(SymbolTokens);
}

bool RightShift::hasDot() const {
  return LeftExpression.hasDot() || RightExpression.hasDot();
}

//===----------------------------------------------------------------------===//
/// LeftShift Operator
void LeftShift::commit() {
  LeftExpression.commit();
  RightExpression.commit();
  Expression::commit();
}
void LeftShift::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (ExpressionHasParenthesis)
    Outs << "(";
  if (!hasAssign()) {
    // format output for operator
    LeftExpression.dump(Outs, WithValues);
    Outs << " " << Name << " ";
  }
  RightExpression.dump(Outs, WithValues);
  if (ExpressionHasParenthesis)
    Outs << ")";
}
eld::Expected<uint64_t> LeftShift::evalImpl() {
  // evaluate sub expressions
  auto Left = LeftExpression.eval();
  if (!Left)
    return Left;
  auto Right = RightExpression.eval();
  if (!Right)
    return Right;
  return Left.value() << Right.value();
}
void LeftShift::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  LeftExpression.getSymbols(Symbols);
  RightExpression.getSymbols(Symbols);
}

void LeftShift::getSymbolNames(std::unordered_set<std::string> &SymbolTokens) {
  LeftExpression.getSymbolNames(SymbolTokens);
  RightExpression.getSymbolNames(SymbolTokens);
}

bool LeftShift::hasDot() const {
  return LeftExpression.hasDot() || RightExpression.hasDot();
}

//===----------------------------------------------------------------------===//
/// BitwiseOr Operator
void BitwiseOr::commit() {
  LeftExpression.commit();
  RightExpression.commit();
  Expression::commit();
}
void BitwiseOr::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (ExpressionHasParenthesis)
    Outs << "(";
  if (!hasAssign()) {
    // format output for operator
    LeftExpression.dump(Outs, WithValues);
    Outs << " " << Name << " ";
  }
  RightExpression.dump(Outs, WithValues);
  if (ExpressionHasParenthesis)
    Outs << ")";
}
eld::Expected<uint64_t> BitwiseOr::evalImpl() {
  // evaluate sub expressions
  auto Left = LeftExpression.eval();
  if (!Left)
    return Left;
  auto Right = RightExpression.eval();
  if (!Right)
    return Right;
  return Left.value() | Right.value();
}
void BitwiseOr::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  LeftExpression.getSymbols(Symbols);
  RightExpression.getSymbols(Symbols);
}

void BitwiseOr::getSymbolNames(std::unordered_set<std::string> &SymbolTokens) {
  LeftExpression.getSymbolNames(SymbolTokens);
  RightExpression.getSymbolNames(SymbolTokens);
}

bool BitwiseOr::hasDot() const {
  return LeftExpression.hasDot() || RightExpression.hasDot();
}

//===----------------------------------------------------------------------===//
/// BitwiseXor Operator
void BitwiseXor::commit() {
  LeftExpression.commit();
  RightExpression.commit();
  Expression::commit();
}
void BitwiseXor::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (ExpressionHasParenthesis)
    Outs << "(";
  if (!hasAssign()) {
    // format output for operator
    LeftExpression.dump(Outs, WithValues);
    Outs << " " << Name << " ";
  }
  RightExpression.dump(Outs, WithValues);
  if (ExpressionHasParenthesis)
    Outs << ")";
}
eld::Expected<uint64_t> BitwiseXor::evalImpl() {
  // evaluate sub expressions
  auto Left = LeftExpression.eval();
  if (!Left)
    return Left;
  auto Right = RightExpression.eval();
  if (!Right)
    return Right;
  return Left.value() ^ Right.value();
}
void BitwiseXor::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  LeftExpression.getSymbols(Symbols);
  RightExpression.getSymbols(Symbols);
}

void BitwiseXor::getSymbolNames(std::unordered_set<std::string> &SymbolTokens) {
  LeftExpression.getSymbolNames(SymbolTokens);
  RightExpression.getSymbolNames(SymbolTokens);
}

bool BitwiseXor::hasDot() const {
  return LeftExpression.hasDot() || RightExpression.hasDot();
}

//===----------------------------------------------------------------------===//
/// BitwiseAnd Operator
void BitwiseAnd::commit() {
  LeftExpression.commit();
  RightExpression.commit();
  Expression::commit();
}
void BitwiseAnd::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (ExpressionHasParenthesis)
    Outs << "(";
  if (!hasAssign()) {
    // format output for operator
    LeftExpression.dump(Outs, WithValues);
    Outs << " " << Name << " ";
  }
  RightExpression.dump(Outs, WithValues);
  if (ExpressionHasParenthesis)
    Outs << ")";
}
eld::Expected<uint64_t> BitwiseAnd::evalImpl() {
  // evaluate sub expressions
  auto Left = LeftExpression.eval();
  if (!Left)
    return Left;
  auto Right = RightExpression.eval();
  if (!Right)
    return Right;
  return Left.value() & Right.value();
}
void BitwiseAnd::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  LeftExpression.getSymbols(Symbols);
  RightExpression.getSymbols(Symbols);
}

void BitwiseAnd::getSymbolNames(std::unordered_set<std::string> &SymbolTokens) {
  LeftExpression.getSymbolNames(SymbolTokens);
  RightExpression.getSymbolNames(SymbolTokens);
}

bool BitwiseAnd::hasDot() const {
  return LeftExpression.hasDot() || RightExpression.hasDot();
}

//===----------------------------------------------------------------------===//
/// Defined Operator
void Defined::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  // format output for operator
  Outs << "DEFINED(" << Name << ")";
}
eld::Expected<uint64_t> Defined::evalImpl() {
  const LDSymbol *Symbol = ThisModule.getNamePool().findSymbol(Name);
  if (Symbol == nullptr)
    return 0;

  if (Symbol->scriptDefined()) {
    if (Symbol->scriptValueDefined())
      return 1;
    return 0;
  }
  return !Symbol->resolveInfo()->isUndef();
}

void Defined::getSymbols(std::vector<ResolveInfo *> &Symbols) {}

void Defined::getSymbolNames(std::unordered_set<std::string> &SymbolTokens) {}

bool Defined::hasDot() const {
  LDSymbol *Symbol = ThisModule.getNamePool().findSymbol(Name);
  return Symbol == ThisModule.getDotSymbol();
}

//===----------------------------------------------------------------------===//
/// DataSegmentAlign Operator
void DataSegmentAlign::commit() {
  MaxPageSize.commit();
  CommonPageSize.commit();
  Expression::commit();
}
void DataSegmentAlign::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  // format output for operator
  Outs << Name << "(";
  MaxPageSize.dump(Outs, WithValues);
  Outs << ", ";
  CommonPageSize.dump(Outs, WithValues);
  Outs << ")";
}
eld::Expected<uint64_t> DataSegmentAlign::evalImpl() {
  auto MaxPageSize = this->MaxPageSize.eval();
  if (!MaxPageSize)
    return MaxPageSize;
  auto CommonPageSize = this->CommonPageSize.eval();
  if (!CommonPageSize)
    return CommonPageSize;
  uint64_t Dot = ThisModule.getDotSymbol()->value();
  uint64_t Form1 = 0, Form2 = 0;

  uint64_t DotAligned = Dot;

  alignAddress(DotAligned, MaxPageSize.value());

  Form1 = DotAligned + (Dot & (MaxPageSize.value() - 1));
  Form2 = DotAligned + (Dot & (MaxPageSize.value() - CommonPageSize.value()));

  if (Form1 <= Form2)
    return Form1;
  return Form2;
}
void DataSegmentAlign::getSymbols(std::vector<ResolveInfo *> &Symbols) {}

void DataSegmentAlign::getSymbolNames(
    std::unordered_set<std::string> &SymbolTokens) {}

//===----------------------------------------------------------------------===//
/// DataSegmentRelRoEnd Operator
void DataSegmentRelRoEnd::commit() {
  LeftExpression.commit();
  RightExpression.commit();
  CommonPageSize.commit();
  Expression::commit();
}
void DataSegmentRelRoEnd::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  // format output for operator
  Outs << Name << "(";
  LeftExpression.dump(Outs, WithValues);
  Outs << ", ";
  RightExpression.dump(Outs, WithValues);
  Outs << ")";
}
eld::Expected<uint64_t> DataSegmentRelRoEnd::evalImpl() {
  auto CommonPageSize = this->CommonPageSize.eval();
  if (!CommonPageSize)
    return CommonPageSize;
  auto Expr1 = LeftExpression.eval();
  if (!Expr1)
    return Expr1;
  auto Expr2 = RightExpression.eval();
  if (!Expr2)
    return Expr2;
  uint64_t Value = Expr1.value() + Expr2.value();
  alignAddress(Value, CommonPageSize.value());
  return Value;
}
void DataSegmentRelRoEnd::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  LeftExpression.getSymbols(Symbols);
  RightExpression.getSymbols(Symbols);
}

void DataSegmentRelRoEnd::getSymbolNames(
    std::unordered_set<std::string> &SymbolTokens) {
  LeftExpression.getSymbolNames(SymbolTokens);
  RightExpression.getSymbolNames(SymbolTokens);
}

bool DataSegmentRelRoEnd::hasDot() const {
  return LeftExpression.hasDot() || RightExpression.hasDot();
}

//===----------------------------------------------------------------------===//
/// DataSegmentEnd Operator
void DataSegmentEnd::commit() {
  ExpressionToEvaluate.commit();
  Expression::commit();
}
void DataSegmentEnd::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  // format output for operator
  Outs << Name << "(";
  ExpressionToEvaluate.dump(Outs, WithValues);
  Outs << ")";
}
eld::Expected<uint64_t> DataSegmentEnd::evalImpl() {
  // evaluate sub expressions
  auto Expr = ExpressionToEvaluate.eval();
  if (!Expr)
    return Expr;
  return Expr.value() != 0; // TODO: What does this do?
}
void DataSegmentEnd::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  ExpressionToEvaluate.getSymbols(Symbols);
}

void DataSegmentEnd::getSymbolNames(
    std::unordered_set<std::string> &SymbolTokens) {
  ExpressionToEvaluate.getSymbolNames(SymbolTokens);
}

bool DataSegmentEnd::hasDot() const { return ExpressionToEvaluate.hasDot(); }

//===----------------------------------------------------------------------===//
/// Max Operator
void Max::commit() {
  LeftExpression.commit();
  RightExpression.commit();
  Expression::commit();
}
void Max::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (ExpressionHasParenthesis)
    Outs << "(";
  Outs << Name << "(";
  // format output for operator
  LeftExpression.dump(Outs, WithValues);
  Outs << ",";
  RightExpression.dump(Outs, WithValues);
  Outs << ")";
  if (ExpressionHasParenthesis)
    Outs << ")";
}
eld::Expected<uint64_t> Max::evalImpl() {
  // evaluate sub expressions
  auto Left = LeftExpression.eval();
  if (!Left)
    return Left;
  auto Right = RightExpression.eval();
  if (!Right)
    return Right;

  return Left.value() > Right.value() ? Left.value() : Right.value();
}
void Max::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  LeftExpression.getSymbols(Symbols);
  RightExpression.getSymbols(Symbols);
}

void Max::getSymbolNames(std::unordered_set<std::string> &SymbolTokens) {
  LeftExpression.getSymbolNames(SymbolTokens);
  RightExpression.getSymbolNames(SymbolTokens);
}

bool Max::hasDot() const {
  return LeftExpression.hasDot() || RightExpression.hasDot();
}

//===----------------------------------------------------------------------===//
/// Min Operator
void Min::commit() {
  LeftExpression.commit();
  RightExpression.commit();
  Expression::commit();
}
void Min::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (ExpressionHasParenthesis)
    Outs << "(";
  // format output for operator
  LeftExpression.dump(Outs, WithValues);
  Outs << " " << Name << " ";
  RightExpression.dump(Outs, WithValues);
  if (ExpressionHasParenthesis)
    Outs << ")";
}
eld::Expected<uint64_t> Min::evalImpl() {
  // evaluate sub expressions
  auto Left = LeftExpression.eval();
  if (!Left)
    return Left;
  auto Right = RightExpression.eval();
  if (!Right)
    return Right;
  return Left.value() < Right.value() ? Left.value() : Right.value();
}
void Min::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  LeftExpression.getSymbols(Symbols);
  RightExpression.getSymbols(Symbols);
}

void Min::getSymbolNames(std::unordered_set<std::string> &SymbolTokens) {
  LeftExpression.getSymbolNames(SymbolTokens);
  RightExpression.getSymbolNames(SymbolTokens);
}
bool Min::hasDot() const {
  return LeftExpression.hasDot() || RightExpression.hasDot();
}

//===----------------------------------------------------------------------===//
/// Fill
void Fill::commit() {
  ExpressionToEvaluate.commit();
  Expression::commit();
}
void Fill::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  Outs << "FILL"
       << "(";
  ExpressionToEvaluate.dump(Outs, WithValues);
  Outs << ")";
}
eld::Expected<uint64_t> Fill::evalImpl() { return ExpressionToEvaluate.eval(); }

void Fill::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  ExpressionToEvaluate.getSymbols(Symbols);
}

void Fill::getSymbolNames(std::unordered_set<std::string> &SymbolTokens) {
  ExpressionToEvaluate.getSymbolNames(SymbolTokens);
}

bool Fill::hasDot() const { return ExpressionToEvaluate.hasDot(); }

//===----------------------------------------------------------------------===//
/// Log2Ceil operator
void Log2Ceil::commit() {
  ExpressionToEvaluate.commit();
  Expression::commit();
}
void Log2Ceil::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  // format output for operator
  Outs << Name;
  Outs << "(";
  ExpressionToEvaluate.dump(Outs, WithValues);
  Outs << ")";
}

eld::Expected<uint64_t> Log2Ceil::evalImpl() {
  auto Val = ExpressionToEvaluate.eval();
  if (!Val)
    return Val;
  return llvm::Log2_64_Ceil(std::max(Val.value(), UINT64_C(1)));
}

void Log2Ceil::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  ExpressionToEvaluate.getSymbols(Symbols);
}

void Log2Ceil::getSymbolNames(std::unordered_set<std::string> &SymbolTokens) {
  ExpressionToEvaluate.getSymbolNames(SymbolTokens);
}

bool Log2Ceil::hasDot() const { return ExpressionToEvaluate.hasDot(); }

//===----------------------------------------------------------------------===//
/// LogicalOp Operator
void LogicalOp::commit() {
  LeftExpression.commit();
  RightExpression.commit();
  Expression::commit();
}

void LogicalOp::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (ExpressionHasParenthesis)
    Outs << "(";
  if (!hasAssign()) {
    // format output for operator
    LeftExpression.dump(Outs, WithValues);
    if (isLogicalAnd())
      Outs << " "
           << "&&"
           << " ";
    if (isLogicalOR())
      Outs << " "
           << "||"
           << " ";
  }
  RightExpression.dump(Outs, WithValues);
  if (ExpressionHasParenthesis)
    Outs << ")";
}
eld::Expected<uint64_t> LogicalOp::evalImpl() {
  // evaluate sub expressions
  auto Left = LeftExpression.eval();
  if (!Left)
    return Left;
  auto Right = RightExpression.eval();
  if (!Right)
    return Right;
  if (isLogicalAnd())
    return Left.value() && Right.value();
  ASSERT(isLogicalOR(), "logical operator can be || or && only");
  return Left.value() || Right.value();
}

void LogicalOp::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  LeftExpression.getSymbols(Symbols);
  RightExpression.getSymbols(Symbols);
}

void LogicalOp::getSymbolNames(std::unordered_set<std::string> &SymbolTokens) {
  LeftExpression.getSymbolNames(SymbolTokens);
  RightExpression.getSymbolNames(SymbolTokens);
}

bool LogicalOp::hasDot() const {
  return LeftExpression.hasDot() || RightExpression.hasDot();
}
//===----------------------------------------------------------------------===//
/// QueryMemory support
QueryMemory::QueryMemory(Expression::Type Type, Module &Module,
                         GNULDBackend &Backend, const std::string &Name)
    : Expression(Name, Type, Module, Backend) {}

void QueryMemory::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (isOrigin())
    Outs << "ORIGIN(";
  else if (isLength())
    Outs << "LENGTH(";
  Outs << Name;
  Outs << ")";
}

eld::Expected<uint64_t> QueryMemory::evalImpl() {
  auto Region = ThisModule.getScript().getMemoryRegion(Name);
  if (!Region)
    return std::move(Region.error());
  if (isOrigin())
    return Region.value()->getOrigin();
  return Region.value()->getLength();
}

void QueryMemory::getSymbols(std::vector<ResolveInfo *> &Symbols) {}

void QueryMemory::getSymbolNames(
    std::unordered_set<std::string> &SymbolTokens) {}

//===----------------------------------------------------------------------===//
/// NullExpression support
NullExpression::NullExpression(Module &Module,
                         GNULDBackend &Backend)
    : Expression("[NULL]", Expression::Type::NULLEXPR, Module, Backend) {}

void NullExpression::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  Outs << Name;
}

eld::Expected<uint64_t> NullExpression::evalImpl() {
  return std::make_unique<plugin::DiagnosticEntry>(
      Diag::internal_error_null_expression, std::vector<std::string>{});
}

void NullExpression::getSymbols(std::vector<ResolveInfo *> &Symbols) {}

void NullExpression::getSymbolNames(
    std::unordered_set<std::string> &SymbolTokens) {}