//===- Expression.cpp------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Script/Expression.h"
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

using namespace eld;

Expression::Expression(std::string Name, Type Type, Module &Module,
                       GNULDBackend &Backend, uint64_t Value)
    : m_Name(Name), m_Type(Type), m_Module(Module), m_Backend(Backend),
      m_Result(0), m_Eval(Value) {}

void Expression::setContext(const std::string &Context) {
  ASSERT(!Context.empty(), "Empty context for expression");
  m_Context = Context;
}

uint64_t Expression::result() const {
  ASSERT(m_Result, "Expression result is not yet committed");
  return *m_Result;
}

std::unique_ptr<plugin::DiagnosticEntry> Expression::AddContextToDiagEntry(
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
  ASSERT(!m_Context.empty(), "Context not set for expression");
  auto result = eval();
  if (!result)
    return AddContextToDiagEntry(std::move(result.error()), m_Context);
  commit();
  return result;
}

std::optional<uint64_t> Expression::evaluateAndRaiseError() {
  ASSERT(!m_Context.empty(), "Context not set for expression");
  auto result = eval();
  if (!result) {
    // Even if evaluation fails, set the result (to zero) as
    // we don't expect the caller to exit due to this error.
    m_Module.getConfig().raiseDiagEntry(
        AddContextToDiagEntry(std::move(result.error()), m_Context));
    commit();
    return {};
  }
  commit();
  return result.value();
}

eld::Expected<uint64_t> Expression::eval() {
  auto V = evalImpl();
  if (V)
    m_Eval = V.value();
  return V;
}

void Expression::setContextRecursively(const std::string &context) {
  setContext(context);
  if (Expression *L = getLeftExpression())
    L->setContextRecursively(context);
  if (Expression *R = getRightExpression())
    R->setContextRecursively(context);
}

//===----------------------------------------------------------------------===//
/// Symbol Operand
Symbol::Symbol(Module &PModule, GNULDBackend &PBackend, std::string PName)
    : Expression(PName, Expression::SYMBOL, PModule, PBackend),
      m_Symbol(nullptr) {}

void Symbol::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  // format output for operand
  Outs << m_Name;
  if (WithValues) {
    Outs << "(0x";
    Outs.write_hex(*m_Result) << ")";
  }
}

bool Symbol::hasDot() const {
  if (!m_Symbol)
    m_Symbol = m_Module.getNamePool().findSymbol(m_Name);
  return m_Symbol == m_Module.getDotSymbol();
}

eld::Expected<uint64_t> Symbol::evalImpl() {

  if (!m_Symbol)
    m_Symbol = m_Module.getNamePool().findSymbol(m_Name);

  if (!m_Symbol || m_Symbol->resolveInfo()->isBitCode())
    return std::make_unique<plugin::DiagnosticEntry>(
        diag::undefined_symbol_in_linker_script,
        std::vector<std::string>{m_Name});

  if (m_Symbol->hasFragRef() && !m_Symbol->shouldIgnore()) {
    FragmentRef *FragRef = m_Symbol->fragRef();
    ELFSection *Section = FragRef->getOutputELFSection();
    bool IsAllocSection = Section ? Section->isAlloc() : false;

    ASSERT(IsAllocSection,
           "using a symbol that points to a non allocatable section!");
    return Section->addr() + FragRef->getOutputOffset(m_Module);
  } else {
    return m_Symbol->value();
  }
}

void Symbol::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  ResolveInfo *R = m_Module.getNamePool().findInfo(m_Name);
  if (R == nullptr) {
    return;
  }
  // Dont add DOT symbols.
  if (R == m_Module.getDotSymbol()->resolveInfo())
    return;
  Symbols.push_back(R);
}

void Symbol::getSymbolNames(std::unordered_set<std::string> &symbolTokens) {
  // Dont add DOT symbols.
  if (m_Name == m_Module.getDotSymbol()->resolveInfo()->name())
    return;
  symbolTokens.insert(m_Name);
}

//===----------------------------------------------------------------------===//
/// Integer Operand
void Integer::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (m_Paren)
    Outs << "(";
  // format output for operand
  Outs << "0x";
  Outs.write_hex(m_Value);
  if (m_Paren)
    Outs << ")";
}
eld::Expected<uint64_t> Integer::evalImpl() { return m_Value; }
void Integer::getSymbols(std::vector<ResolveInfo *> &Symbols) {}

void Integer::getSymbolNames(std::unordered_set<std::string> &symbolTokens) {}

//===----------------------------------------------------------------------===//
/// Add Operator
void Add::commit() {
  m_Left.commit();
  m_Right.commit();
  Expression::commit();
}
void Add::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  // format output for operator
  if (m_Paren)
    Outs << "(";
  if (!hasAssign()) {
    m_Left.dump(Outs, WithValues);
    Outs << " " << m_Name << " ";
  }
  m_Right.dump(Outs, WithValues);
  if (m_Paren)
    Outs << ")";
}
eld::Expected<uint64_t> Add::evalImpl() {
  // evaluate sub expressions
  auto Left = m_Left.eval();
  if (!Left)
    return Left;
  auto Right = m_Right.eval();
  if (!Right)
    return Right;
  if (!m_Module.getScript().phdrsSpecified() && m_Right.isSizeOfHeaders()) {
    if (m_Module.getDotSymbol() &&
        m_Module.getDotSymbol()->value() == m_Backend.getImageStartVMA()) {
      // Load file headers and program header
      m_Backend.setNeedEhdr();
      m_Backend.setNeedPhdr();
    }
  }
  return Left.value() + Right.value();
}

bool Add::hasDot() const { return m_Left.hasDot() || m_Right.hasDot(); }

void Add::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  m_Left.getSymbols(Symbols);
  m_Right.getSymbols(Symbols);
}

void Add::getSymbolNames(std::unordered_set<std::string> &symbolTokens) {
  m_Left.getSymbolNames(symbolTokens);
  m_Right.getSymbolNames(symbolTokens);
}

//===----------------------------------------------------------------------===//
/// Subtract Operator
void Subtract::commit() {
  m_Left.commit();
  m_Right.commit();
  Expression::commit();
}
void Subtract::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (m_Paren)
    Outs << "(";
  if (!hasAssign()) {
    // format output for operator
    m_Left.dump(Outs, WithValues);
    Outs << " " << m_Name << " ";
  }
  m_Right.dump(Outs, WithValues);
  if (m_Paren)
    Outs << ")";
}
eld::Expected<uint64_t> Subtract::evalImpl() {
  // evaluate sub expressions
  auto Left = m_Left.eval();
  if (!Left)
    return Left;
  auto Right = m_Right.eval();
  if (!Right)
    return Right;
  return Left.value() - Right.value();
}
void Subtract::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  m_Left.getSymbols(Symbols);
  m_Right.getSymbols(Symbols);
}

void Subtract::getSymbolNames(std::unordered_set<std::string> &symbolTokens) {
  m_Left.getSymbolNames(symbolTokens);
  m_Right.getSymbolNames(symbolTokens);
}
bool Subtract::hasDot() const { return m_Left.hasDot() || m_Right.hasDot(); }

//===----------------------------------------------------------------------===//
/// Modulo Operator
void Modulo::commit() {
  m_Left.commit();
  m_Right.commit();
  Expression::commit();
}
void Modulo::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (m_Paren)
    Outs << "(";
  // format output for operator
  m_Left.dump(Outs, WithValues);
  Outs << " " << m_Name << " ";
  m_Right.dump(Outs, WithValues);
  if (m_Paren)
    Outs << ")";
}
eld::Expected<uint64_t> Modulo::evalImpl() {
  // evaluate sub expressions
  auto Left = m_Left.eval();
  if (!Left)
    return Left;
  auto Right = m_Right.eval();
  if (!Right)
    return Right;
  if (Right.value() == 0) {
    std::string errorString;
    llvm::raw_string_ostream errorStringStream(errorString);
    dump(errorStringStream);
    return std::make_unique<plugin::DiagnosticEntry>(
        diag::fatal_modulo_by_zero,
        std::vector<std::string>{errorStringStream.str()});
  }
  return Left.value() % Right.value();
}
void Modulo::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  m_Left.getSymbols(Symbols);
  m_Right.getSymbols(Symbols);
}

void Modulo::getSymbolNames(std::unordered_set<std::string> &symbolTokens) {
  m_Left.getSymbolNames(symbolTokens);
  m_Right.getSymbolNames(symbolTokens);
}

bool Modulo::hasDot() const { return m_Left.hasDot() || m_Right.hasDot(); }

//===----------------------------------------------------------------------===//
/// Multiply Operator
void Multiply::commit() {
  m_Left.commit();
  m_Right.commit();
  Expression::commit();
}
void Multiply::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (m_Paren)
    Outs << "(";
  if (!hasAssign()) {
    // format output for operator
    m_Left.dump(Outs, WithValues);
    Outs << " " << m_Name << " ";
  }
  m_Right.dump(Outs, WithValues);
  if (m_Paren)
    Outs << ")";
}
eld::Expected<uint64_t> Multiply::evalImpl() {
  // evaluate sub expressions
  auto Left = m_Left.eval();
  if (!Left)
    return Left;
  auto Right = m_Right.eval();
  if (!Right)
    return Right;
  return Left.value() * Right.value();
}
void Multiply::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  m_Left.getSymbols(Symbols);
  m_Right.getSymbols(Symbols);
}

void Multiply::getSymbolNames(std::unordered_set<std::string> &symbolTokens) {
  m_Left.getSymbolNames(symbolTokens);
  m_Right.getSymbolNames(symbolTokens);
}

bool Multiply::hasDot() const { return m_Left.hasDot() || m_Right.hasDot(); }

//===----------------------------------------------------------------------===//
/// Divide Operator
void Divide::commit() {
  m_Left.commit();
  m_Right.commit();
  Expression::commit();
}
void Divide::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (m_Paren)
    Outs << "(";
  if (!hasAssign()) {
    // format output for operator
    m_Left.dump(Outs, WithValues);
    Outs << " " << m_Name << " ";
  }
  m_Right.dump(Outs, WithValues);
  if (m_Paren)
    Outs << ")";
}
eld::Expected<uint64_t> Divide::evalImpl() {
  // evaluate sub expressions
  auto Left = m_Left.eval();
  if (!Left)
    return Left;
  auto Right = m_Right.eval();
  if (!Right)
    return Right;
  if (Right.value() == 0) {
    std::string errorString;
    llvm::raw_string_ostream errorStringStream(errorString);
    dump(errorStringStream);
    return std::make_unique<plugin::DiagnosticEntry>(
        diag::fatal_divide_by_zero,
        std::vector<std::string>{errorStringStream.str()});
  }
  return Left.value() / Right.value();
}
void Divide::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  m_Left.getSymbols(Symbols);
  m_Right.getSymbols(Symbols);
}

void Divide::getSymbolNames(std::unordered_set<std::string> &symbolTokens) {
  m_Left.getSymbolNames(symbolTokens);
  m_Right.getSymbolNames(symbolTokens);
}

bool Divide::hasDot() const { return m_Left.hasDot() || m_Right.hasDot(); }

//===----------------------------------------------------------------------===//
/// SizeOf
SizeOf::SizeOf(Module &PModule, GNULDBackend &PBackend, std::string PName)
    : Expression(PName, Expression::SIZEOF, PModule, PBackend),
      m_Sect(nullptr) {}
void SizeOf::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  Outs << "SIZEOF(" << m_Name;
  if (WithValues) {
    Outs << " = 0x";
    Outs.write_hex(*m_Result);
  }
  Outs << ")";
}
eld::Expected<uint64_t> SizeOf::evalImpl() {

  if (m_Name.size() && m_Name[0] == ':') {
    // If the name is a segment and we don't have PHDR's. SIZEOF on segment will
    // not work.
    if (!m_Module.getScript().phdrsSpecified())
      return std::make_unique<plugin::DiagnosticEntry>(
          diag::size_of_used_without_phdrs);

    // If a segment is specified, lets check the segment table for a segment
    // that exists.
    std::string SegmentName = m_Name.substr(1);
    if (ELFSegment *Seg = m_Backend.findSegment(SegmentName)) {
      m_Backend.setupSegmentOffset(Seg);
      m_Backend.setupSegment(Seg);
      m_Backend.clearSegmentOffset(Seg);
      return Seg->filesz();
    }

    return std::make_unique<plugin::DiagnosticEntry>(
        diag::fatal_segment_not_defined_ldscript,
        std::vector<std::string>{SegmentName});
  }
  // As the section table is populated only during PostLayout, we have to
  // go the other way around to access the section. This is because size of
  // empty
  // sections are known only after all the assignments are complete
  if (m_Sect == nullptr)
    m_Sect = m_Module.getScript().sectionMap().find(m_Name);
  if (m_Sect == nullptr)
    return std::make_unique<plugin::DiagnosticEntry>(
        diag::undefined_symbol_in_linker_script,
        std::vector<std::string>{m_Name});

  // NOTE: output sections with no content or those that have been garbaged
  // collected will not be in the Module SectionTable, therefore the size
  // will automatically default to zero (from initialization)
  return m_Sect->size();
}

void SizeOf::getSymbols(std::vector<ResolveInfo *> &Symbols) {}

void SizeOf::getSymbolNames(std::unordered_set<std::string> &symbolTokens) {}

//===----------------------------------------------------------------------===//
/// SizeOfHeaders
SizeOfHeaders::SizeOfHeaders(Module &PModule, GNULDBackend &PBackend,
                             ScriptFile *S)
    : Expression("SIZEOF_HEADERS", Expression::SIZEOF_HEADERS, PModule,
                 PBackend) {
  // SIZEOF_HEADERS is an insane command. If its at the beginning of the script,
  // the BFD linker sees that there is an empty hole created before the first
  // section begins and inserts program headers and loads them. ELD tries to be
  // do a simpler implementation of the same.
  if (!S->firstLinkerScriptWithOutputSection())
    m_Module.getScript().setSizeOfHeader();
}

void SizeOfHeaders::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  Outs << "SIZEOF_HEADERS";
  if (WithValues) {
    Outs << "("
         << " = 0x";
    Outs.write_hex(*m_Result) << ")";
  }
}

eld::Expected<uint64_t> SizeOfHeaders::evalImpl() {
  uint64_t offset = 0;
  std::vector<ELFSection *> Sections;
  if (!m_Backend.isEhdrNeeded())
    Sections.push_back(m_Backend.getEhdr());
  if (!m_Backend.isPhdrNeeded())
    Sections.push_back(m_Backend.getPhdr());
  for (auto &S : Sections) {
    if (!S)
      continue;
    offset = offset + S->size();
  }
  return offset;
}

void SizeOfHeaders::getSymbols(std::vector<ResolveInfo *> &Symbols) {}

void SizeOfHeaders::getSymbolNames(
    std::unordered_set<std::string> &symbolTokens) {}

//===----------------------------------------------------------------------===//
/// Addr
void Addr::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  Outs << "ADDR(\"" << m_Name;
  if (WithValues) {
    Outs << " = 0x";
    Outs.write_hex(*m_Result);
  }
  Outs << "\")";
}
eld::Expected<uint64_t> Addr::evalImpl() {
  // As the section table is populated only during PostLayout, we have to
  // go the other way around to access the section. This is because size of
  // empty
  // sections are known only after all the assignments are complete
  if (m_Sect == nullptr)
    m_Sect = m_Module.getScript().sectionMap().find(m_Name);
  if (m_Sect == nullptr)
    return std::make_unique<plugin::DiagnosticEntry>(
        diag::undefined_symbol_in_linker_script,
        std::vector<std::string>{m_Name});
  if (!m_Sect->hasVMA())
    m_Module.getConfig().raise(diag::warn_forward_reference)
        << m_Context << m_Name;
  // evaluate sub expression
  return m_Sect->addr();
}

void Addr::getSymbols(std::vector<ResolveInfo *> &Symbols) {}

void Addr::getSymbolNames(std::unordered_set<std::string> &symbolTokens) {}

//===----------------------------------------------------------------------===//
/// LoadAddr
LoadAddr::LoadAddr(Module &Module, GNULDBackend &Backend, std::string Name)
    : Expression(Name, Expression::LOADADDR, Module, Backend), m_Sect(nullptr) {
  m_ForwardReference = !Module.findInOutputSectionDescNameSet(Name);
}

void LoadAddr::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  Outs << "LOADADDR(" << m_Name;
  if (WithValues) {
    Outs << " = 0x";
    Outs.write_hex(*m_Result);
  }
  Outs << ")";
}
eld::Expected<uint64_t> LoadAddr::evalImpl() {
  // As the section table is populated only during PostLayout, we have to
  // go the other way around to access the section. This is because size of
  // empty
  // sections are known only after all the assignments are complete
  if (m_Sect == nullptr)
    m_Sect = m_Module.getScript().sectionMap().find(m_Name);
  if (m_Sect == nullptr)
    return std::make_unique<plugin::DiagnosticEntry>(
        diag::undefined_symbol_in_linker_script,
        std::vector<std::string>{m_Name});
  if (m_ForwardReference && m_Module.getConfig().showBadDotAssignmentWarnings())
    return std::make_unique<plugin::DiagnosticEntry>(
        diag::warn_forward_reference, std::vector<std::string>{m_Name});
  // evaluate sub expression
  return m_Sect->pAddr();
}
void LoadAddr::getSymbols(std::vector<ResolveInfo *> &Symbols) {}

void LoadAddr::getSymbolNames(std::unordered_set<std::string> &symbolTokens) {}
//===----------------------------------------------------------------------===//
/// OffsetOf
void OffsetOf::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  Outs << "OFFSETOF(" << m_Name;
  if (WithValues) {
    Outs << " = 0x";
    Outs.write_hex(*m_Result);
  }
}
eld::Expected<uint64_t> OffsetOf::evalImpl() {
  // As the section table is populated only during PostLayout, we have to
  // go the other way around to access the section. This is because size of
  // empty
  // sections are known only after all the assignments are complete
  if (m_Sect == nullptr)
    m_Sect = m_Module.getScript().sectionMap().find(m_Name);
  assert(m_Sect != nullptr);
  // evaluate sub expression
  if (m_Sect->hasOffset())
    return m_Sect->offset();
  return 0;
}
void OffsetOf::getSymbols(std::vector<ResolveInfo *> &Symbols) {}

void OffsetOf::getSymbolNames(std::unordered_set<std::string> &symbolTokens) {}

//===----------------------------------------------------------------------===//
/// Ternary
void Ternary::commit() {
  m_Cond.commit();
  m_Left.commit();
  m_Right.commit();
  Expression::commit();
}
void Ternary::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (m_Paren)
    Outs << "(";
  m_Cond.dump(Outs, WithValues);
  Outs << " ? ";
  m_Left.dump(Outs, WithValues);
  Outs << " : ";
  m_Right.dump(Outs, WithValues);
  if (m_Paren)
    Outs << ")";
}
eld::Expected<uint64_t> Ternary::evalImpl() {
  auto Cond = m_Cond.eval();
  if (!Cond)
    return Cond;
  return Cond.value() ? m_Left.eval() : m_Right.eval();
}
void Ternary::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  if (m_Cond.result())
    m_Left.getSymbols(Symbols);
  else
    m_Right.getSymbols(Symbols);
}

void Ternary::getSymbolNames(std::unordered_set<std::string> &symbolTokens) {
  if (m_Cond.result())
    m_Left.getSymbolNames(symbolTokens);
  else
    m_Right.getSymbolNames(symbolTokens);
}

bool Ternary::hasDot() const {
  return m_Cond.hasDot() || m_Left.hasDot() || m_Right.hasDot();
}

//===----------------------------------------------------------------------===//
/// AlignExpr Operator
void AlignExpr::commit() {
  m_Expr.commit();
  m_Align.commit();
  Expression::commit();
}
void AlignExpr::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  // format output for operator
  Outs << m_Name << "(";
  m_Expr.dump(Outs, WithValues);
  Outs << ", ";
  m_Align.dump(Outs, WithValues);
  Outs << ")";
}
eld::Expected<uint64_t> AlignExpr::evalImpl() {
  // evaluate sub expressions
  auto Expr = m_Expr.eval();
  if (!Expr)
    return Expr;
  auto Align = m_Align.eval();
  if (!Align)
    return Align;
  uint64_t Value = Expr.value();
  uint64_t AlignValue = Align.value();
  if (!AlignValue)
    return Value;
  if (m_Module.getConfig().showLinkerScriptWarnings() &&
      !llvm::isPowerOf2_64(AlignValue))
    m_Module.getConfig().raise(diag::warn_non_power_of_2_value_to_align_builtin)
        << getContext() << utility::toHex(AlignValue);
  return llvm::alignTo(Value, AlignValue);
}

void AlignExpr::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  m_Expr.getSymbols(Symbols);
}

void AlignExpr::getSymbolNames(std::unordered_set<std::string> &symbolTokens) {
  m_Expr.getSymbolNames(symbolTokens);
}

bool AlignExpr::hasDot() const { return m_Expr.hasDot() || m_Align.hasDot(); }

//===----------------------------------------------------------------------===//
/// AlignOf Operator
void AlignOf::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  // format output for operator
  Outs << "ALIGNOF(" << m_Name << ")";
}
void AlignOf::getSymbols(std::vector<ResolveInfo *> &Symbols) {}

void AlignOf::getSymbolNames(std::unordered_set<std::string> &symbolTokens) {}

eld::Expected<uint64_t> AlignOf::evalImpl() {
  // As the section table is populated only during PostLayout, we have to
  // go the other way around to access the section. This is because size of
  // empty
  // sections are known only after all the assignments are complete
  if (m_Sect == nullptr)
    m_Sect = m_Module.getScript().sectionMap().find(m_Name);
  assert(m_Sect != nullptr);
  // evaluate sub expressions
  return m_Sect->getAddrAlign();
}

//===----------------------------------------------------------------------===//
/// Absolute Operator
void Absolute::commit() {
  m_Expr.commit();
  Expression::commit();
}
void Absolute::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  // format output for operator
  Outs << "ABSOLUTE(";
  m_Expr.dump(Outs, WithValues);
  Outs << ")";
}
eld::Expected<uint64_t> Absolute::evalImpl() { return m_Expr.eval(); }
void Absolute::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  m_Expr.getSymbols(Symbols);
}

void Absolute::getSymbolNames(std::unordered_set<std::string> &symbolTokens) {
  m_Expr.getSymbolNames(symbolTokens);
}

bool Absolute::hasDot() const { return m_Expr.hasDot(); }

//===----------------------------------------------------------------------===//
/// ConditionGT Operator
void ConditionGT::commit() {
  m_Left.commit();
  m_Right.commit();
  Expression::commit();
}
void ConditionGT::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (m_Paren)
    Outs << "(";
  // format output for operator
  m_Left.dump(Outs, WithValues);
  Outs << " " << m_Name << " ";
  m_Right.dump(Outs, WithValues);
  if (m_Paren)
    Outs << ")";
}
eld::Expected<uint64_t> ConditionGT::evalImpl() {
  auto Left = m_Left.eval();
  if (!Left)
    return Left;
  auto Right = m_Right.eval();
  if (!Right)
    return Right;
  return Left.value() > Right.value();
}
void ConditionGT::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  m_Left.getSymbols(Symbols);
  m_Right.getSymbols(Symbols);
}

void ConditionGT::getSymbolNames(
    std::unordered_set<std::string> &symbolTokens) {
  m_Left.getSymbolNames(symbolTokens);
  m_Right.getSymbolNames(symbolTokens);
}

bool ConditionGT::hasDot() const { return m_Left.hasDot() || m_Right.hasDot(); }

//===----------------------------------------------------------------------===//
/// ConditionLT Operator
void ConditionLT::commit() {
  m_Left.commit();
  m_Right.commit();
  Expression::commit();
}
void ConditionLT::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (m_Paren)
    Outs << "(";
  // format output for operator
  m_Left.dump(Outs, WithValues);
  Outs << " " << m_Name << " ";
  m_Right.dump(Outs, WithValues);
  if (m_Paren)
    Outs << ")";
}
eld::Expected<uint64_t> ConditionLT::evalImpl() {
  // evaluate sub expressions
  auto Left = m_Left.eval();
  if (!Left)
    return Left;
  auto Right = m_Right.eval();
  if (!Right)
    return Right;
  return Left.value() < Right.value();
}
void ConditionLT::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  m_Left.getSymbols(Symbols);
  m_Right.getSymbols(Symbols);
}

void ConditionLT::getSymbolNames(
    std::unordered_set<std::string> &symbolTokens) {
  m_Left.getSymbolNames(symbolTokens);
  m_Right.getSymbolNames(symbolTokens);
}

bool ConditionLT::hasDot() const { return m_Left.hasDot() || m_Right.hasDot(); }

//===----------------------------------------------------------------------===//
/// ConditionEQ Operator
void ConditionEQ::commit() {
  m_Left.commit();
  m_Right.commit();
  Expression::commit();
}
void ConditionEQ::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (m_Paren)
    Outs << "(";
  // format output for operator
  m_Left.dump(Outs, WithValues);
  Outs << " " << m_Name << " ";
  m_Right.dump(Outs, WithValues);
  if (m_Paren)
    Outs << ")";
}
eld::Expected<uint64_t> ConditionEQ::evalImpl() {
  // evaluate sub expressions
  auto Left = m_Left.eval();
  if (!Left)
    return Left;
  auto Right = m_Right.eval();
  if (!Right)
    return Right;
  return Left.value() == Right.value();
}
void ConditionEQ::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  m_Left.getSymbols(Symbols);
  m_Right.getSymbols(Symbols);
}

void ConditionEQ::getSymbolNames(
    std::unordered_set<std::string> &symbolTokens) {
  m_Left.getSymbolNames(symbolTokens);
  m_Right.getSymbolNames(symbolTokens);
}

bool ConditionEQ::hasDot() const { return m_Left.hasDot() || m_Right.hasDot(); }

//===----------------------------------------------------------------------===//
/// ConditionGTE Operator
void ConditionGTE::commit() {
  m_Left.commit();
  m_Right.commit();
  Expression::commit();
}
void ConditionGTE::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (m_Paren)
    Outs << "(";
  // format output for operator
  m_Left.dump(Outs, WithValues);
  Outs << " " << m_Name << " ";
  m_Right.dump(Outs, WithValues);
  if (m_Paren)
    Outs << ")";
}
eld::Expected<uint64_t> ConditionGTE::evalImpl() {
  // evaluate sub expressions
  auto Left = m_Left.eval();
  if (!Left)
    return Left;
  auto Right = m_Right.eval();
  if (!Right)
    return Right;
  return Left.value() >= Right.value();
}
void ConditionGTE::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  m_Left.getSymbols(Symbols);
  m_Right.getSymbols(Symbols);
}

void ConditionGTE::getSymbolNames(
    std::unordered_set<std::string> &symbolTokens) {
  m_Left.getSymbolNames(symbolTokens);
  m_Right.getSymbolNames(symbolTokens);
}

bool ConditionGTE::hasDot() const {
  return m_Left.hasDot() || m_Right.hasDot();
}

//===----------------------------------------------------------------------===//
/// ConditionLTE Operator
void ConditionLTE::commit() {
  m_Left.commit();
  m_Right.commit();
  Expression::commit();
}
void ConditionLTE::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (m_Paren)
    Outs << "(";
  // format output for operator
  m_Left.dump(Outs, WithValues);
  Outs << " " << m_Name << " ";
  m_Right.dump(Outs, WithValues);
  if (m_Paren)
    Outs << ")";
}
eld::Expected<uint64_t> ConditionLTE::evalImpl() {
  // evaluate sub expressions
  auto Left = m_Left.eval();
  if (!Left)
    return Left;
  auto Right = m_Right.eval();
  if (!Right)
    return Right;
  return Left.value() <= Right.value();
}
void ConditionLTE::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  m_Left.getSymbols(Symbols);
  m_Right.getSymbols(Symbols);
}

void ConditionLTE::getSymbolNames(
    std::unordered_set<std::string> &symbolTokens) {
  m_Left.getSymbolNames(symbolTokens);
  m_Right.getSymbolNames(symbolTokens);
}

bool ConditionLTE::hasDot() const {
  return m_Left.hasDot() || m_Right.hasDot();
}

//===----------------------------------------------------------------------===//
/// ConditionNEQ Operator
void ConditionNEQ::commit() {
  m_Left.commit();
  m_Right.commit();
  Expression::commit();
}
void ConditionNEQ::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (m_Paren)
    Outs << "(";
  // format output for operator
  m_Left.dump(Outs, WithValues);
  Outs << " " << m_Name << " ";
  m_Right.dump(Outs, WithValues);
  if (m_Paren)
    Outs << ")";
}
eld::Expected<uint64_t> ConditionNEQ::evalImpl() {
  // evaluate sub expressions
  auto Left = m_Left.eval();
  if (!Left)
    return Left;
  auto Right = m_Right.eval();
  if (!Right)
    return Right;
  return Left.value() != Right.value();
}
void ConditionNEQ::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  m_Left.getSymbols(Symbols);
  m_Right.getSymbols(Symbols);
}

void ConditionNEQ::getSymbolNames(
    std::unordered_set<std::string> &symbolTokens) {
  m_Left.getSymbolNames(symbolTokens);
  m_Right.getSymbolNames(symbolTokens);
}

bool ConditionNEQ::hasDot() const {
  return m_Left.hasDot() || m_Right.hasDot();
}

//===----------------------------------------------------------------------===//
/// Complement Operator
void Complement::commit() {
  m_Expr.commit();
  Expression::commit();
}
void Complement::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  // format output for operator
  Outs << m_Name;
  m_Expr.dump(Outs, WithValues);
}
eld::Expected<uint64_t> Complement::evalImpl() {
  // evaluate sub expressions
  auto Expr = m_Expr.eval();
  if (!Expr)
    return Expr;
  return ~Expr.value();
}
void Complement::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  m_Expr.getSymbols(Symbols);
}

void Complement::getSymbolNames(std::unordered_set<std::string> &symbolTokens) {
  m_Expr.getSymbolNames(symbolTokens);
}

bool Complement::hasDot() const { return m_Expr.hasDot(); }

//===----------------------------------------------------------------------===//
/// Unary plus operator
void UnaryPlus::commit() {
  m_Expr.commit();
  Expression::commit();
}
void UnaryPlus::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  // format output for operator
  Outs << m_Name;
  m_Expr.dump(Outs, WithValues);
}
eld::Expected<uint64_t> UnaryPlus::evalImpl() { return m_Expr.eval(); }
void UnaryPlus::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  m_Expr.getSymbols(Symbols);
}

void UnaryPlus::getSymbolNames(std::unordered_set<std::string> &symbolTokens) {
  m_Expr.getSymbolNames(symbolTokens);
}

bool UnaryPlus::hasDot() const { return m_Expr.hasDot(); }
//===----------------------------------------------------------------------===//
/// Unary minus operator
void UnaryMinus::commit() {
  m_Expr.commit();
  Expression::commit();
}
void UnaryMinus::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  // format output for operator
  Outs << "-";
  m_Expr.dump(Outs, WithValues);
}
eld::Expected<uint64_t> UnaryMinus::evalImpl() {
  // evaluate sub expressions
  auto Expr = m_Expr.eval();
  if (!Expr)
    return Expr;
  return -Expr.value();
}
void UnaryMinus::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  m_Expr.getSymbols(Symbols);
}

void UnaryMinus::getSymbolNames(std::unordered_set<std::string> &symbolTokens) {
  m_Expr.getSymbolNames(symbolTokens);
}

bool UnaryMinus::hasDot() const { return m_Expr.hasDot(); }
//===----------------------------------------------------------------------===//
/// Unary not operator
void UnaryNot::commit() {
  m_Expr.commit();
  Expression::commit();
}
void UnaryNot::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  // format output for operator
  Outs << m_Name;
  m_Expr.dump(Outs, WithValues);
}
eld::Expected<uint64_t> UnaryNot::evalImpl() {
  // evaluate sub expressions
  auto Expr = m_Expr.eval();
  if (!Expr)
    return Expr;
  return !Expr.value();
}
void UnaryNot::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  m_Expr.getSymbols(Symbols);
}

void UnaryNot::getSymbolNames(std::unordered_set<std::string> &symbolTokens) {
  m_Expr.getSymbolNames(symbolTokens);
}

bool UnaryNot::hasDot() const { return m_Expr.hasDot(); }
//===----------------------------------------------------------------------===//
/// Constant Operator
void Constant::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  // format output for operator
  Outs << "CONSTANT(" << m_Name << ")";
}
eld::Expected<uint64_t> Constant::evalImpl() {
  // evaluate sub expressions
  switch (type()) {
  case Expression::MAXPAGESIZE:
    return m_Backend.abiPageSize();
  case Expression::COMMONPAGESIZE:
    return m_Backend.commonPageSize();
  default:
    // this can't happen because all constants are part of the syntax
    ASSERT(0, "Unexpected constant");
    return {};
  }
}
void Constant::getSymbols(std::vector<ResolveInfo *> &Symbols) {}

void Constant::getSymbolNames(std::unordered_set<std::string> &symbolTokens) {}

//===----------------------------------------------------------------------===//
/// SegmentStart Operator
void SegmentStart::commit() {
  m_Expr.commit();
  Expression::commit();
}
void SegmentStart::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  // format output for operator
  Outs << "SEGMENT_START(\"" << m_Segment << "\", ";
  m_Expr.dump(Outs, WithValues);
  Outs << ")";
}
eld::Expected<uint64_t> SegmentStart::evalImpl() {
  GeneralOptions::AddressMap &AddressMap =
      m_Module.getConfig().options().addressMap();
  GeneralOptions::AddressMap::const_iterator Addr;

  if (m_Segment.compare("text-segment") == 0)
    Addr = AddressMap.find(".text");
  else if (m_Segment.compare("data-segment") == 0)
    Addr = AddressMap.find(".data");
  else if (m_Segment.compare("bss-segment") == 0)
    Addr = AddressMap.find(".bss");
  else
    Addr = AddressMap.find(m_Segment);

  if (Addr != AddressMap.end())
    return Addr->getValue();
  return m_Expr.eval();
}
void SegmentStart::getSymbols(std::vector<ResolveInfo *> &Symbols) {}

void SegmentStart::getSymbolNames(
    std::unordered_set<std::string> &symbolTokens) {}

bool SegmentStart::hasDot() const { return m_Expr.hasDot(); }

//===----------------------------------------------------------------------===//
/// AssertCmd Operator
void AssertCmd::commit() {
  m_Expr.commit();
  Expression::commit();

  // NOTE: we perform the Assertion during commit so if we want to debug a
  // linker
  // script we can just evaluate it without having to worry about asserts
  // causing
  // errors during evaluation
  if (m_Result == 0) {
    std::string Buf;
    llvm::raw_string_ostream Os(Buf);
    dump(Os);
    m_Module.getConfig().raise(diag::assert_failed) << Buf;
  }
}
void AssertCmd::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  // format output for operator
  Outs << m_Name << "(";
  m_Expr.dump(Outs, WithValues);
  Outs << ", \"" << m_Msg << "\")";
}
eld::Expected<uint64_t> AssertCmd::evalImpl() {
  auto Expr = m_Expr.eval();
  if (!Expr)
    return Expr;
  return Expr.value() != 0;
}
void AssertCmd::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  m_Expr.getSymbols(Symbols);
}

void AssertCmd::getSymbolNames(std::unordered_set<std::string> &symbolTokens) {
  m_Expr.getSymbolNames(symbolTokens);
}

bool AssertCmd::hasDot() const { return m_Expr.hasDot(); }

//===----------------------------------------------------------------------===//
/// RightShift Operator
void RightShift::commit() {
  m_Left.commit();
  m_Right.commit();
  Expression::commit();
}
void RightShift::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (m_Paren)
    Outs << "(";
  if (!hasAssign()) {
    // format output for operator
    m_Left.dump(Outs, WithValues);
    Outs << " " << m_Name << " ";
  }
  m_Right.dump(Outs, WithValues);
  if (m_Paren)
    Outs << ")";
}
eld::Expected<uint64_t> RightShift::evalImpl() {
  // evaluate sub expressions
  auto Left = m_Left.eval();
  if (!Left)
    return Left;
  auto Right = m_Right.eval();
  if (!Right)
    return Right;
  return Left.value() >> Right.value();
}

void RightShift::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  m_Left.getSymbols(Symbols);
  m_Right.getSymbols(Symbols);
}

void RightShift::getSymbolNames(std::unordered_set<std::string> &symbolTokens) {
  m_Left.getSymbolNames(symbolTokens);
  m_Right.getSymbolNames(symbolTokens);
}

bool RightShift::hasDot() const { return m_Left.hasDot() || m_Right.hasDot(); }

//===----------------------------------------------------------------------===//
/// LeftShift Operator
void LeftShift::commit() {
  m_Left.commit();
  m_Right.commit();
  Expression::commit();
}
void LeftShift::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (m_Paren)
    Outs << "(";
  if (!hasAssign()) {
    // format output for operator
    m_Left.dump(Outs, WithValues);
    Outs << " " << m_Name << " ";
  }
  m_Right.dump(Outs, WithValues);
  if (m_Paren)
    Outs << ")";
}
eld::Expected<uint64_t> LeftShift::evalImpl() {
  // evaluate sub expressions
  auto Left = m_Left.eval();
  if (!Left)
    return Left;
  auto Right = m_Right.eval();
  if (!Right)
    return Right;
  return Left.value() << Right.value();
}
void LeftShift::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  m_Left.getSymbols(Symbols);
  m_Right.getSymbols(Symbols);
}

void LeftShift::getSymbolNames(std::unordered_set<std::string> &symbolTokens) {
  m_Left.getSymbolNames(symbolTokens);
  m_Right.getSymbolNames(symbolTokens);
}

bool LeftShift::hasDot() const { return m_Left.hasDot() || m_Right.hasDot(); }

//===----------------------------------------------------------------------===//
/// BitwiseOr Operator
void BitwiseOr::commit() {
  m_Left.commit();
  m_Right.commit();
  Expression::commit();
}
void BitwiseOr::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (m_Paren)
    Outs << "(";
  if (!hasAssign()) {
    // format output for operator
    m_Left.dump(Outs, WithValues);
    Outs << " " << m_Name << " ";
  }
  m_Right.dump(Outs, WithValues);
  if (m_Paren)
    Outs << ")";
}
eld::Expected<uint64_t> BitwiseOr::evalImpl() {
  // evaluate sub expressions
  auto Left = m_Left.eval();
  if (!Left)
    return Left;
  auto Right = m_Right.eval();
  if (!Right)
    return Right;
  return Left.value() | Right.value();
}
void BitwiseOr::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  m_Left.getSymbols(Symbols);
  m_Right.getSymbols(Symbols);
}

void BitwiseOr::getSymbolNames(std::unordered_set<std::string> &symbolTokens) {
  m_Left.getSymbolNames(symbolTokens);
  m_Right.getSymbolNames(symbolTokens);
}

bool BitwiseOr::hasDot() const { return m_Left.hasDot() || m_Right.hasDot(); }

//===----------------------------------------------------------------------===//
/// BitwiseAnd Operator
void BitwiseAnd::commit() {
  m_Left.commit();
  m_Right.commit();
  Expression::commit();
}
void BitwiseAnd::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (m_Paren)
    Outs << "(";
  if (!hasAssign()) {
    // format output for operator
    m_Left.dump(Outs, WithValues);
    Outs << " " << m_Name << " ";
  }
  m_Right.dump(Outs, WithValues);
  if (m_Paren)
    Outs << ")";
}
eld::Expected<uint64_t> BitwiseAnd::evalImpl() {
  // evaluate sub expressions
  auto Left = m_Left.eval();
  if (!Left)
    return Left;
  auto Right = m_Right.eval();
  if (!Right)
    return Right;
  return Left.value() & Right.value();
}
void BitwiseAnd::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  m_Left.getSymbols(Symbols);
  m_Right.getSymbols(Symbols);
}

void BitwiseAnd::getSymbolNames(std::unordered_set<std::string> &symbolTokens) {
  m_Left.getSymbolNames(symbolTokens);
  m_Right.getSymbolNames(symbolTokens);
}

bool BitwiseAnd::hasDot() const { return m_Left.hasDot() || m_Right.hasDot(); }

//===----------------------------------------------------------------------===//
/// Defined Operator
void Defined::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  // format output for operator
  Outs << "DEFINED(" << m_Name << ")";
}
eld::Expected<uint64_t> Defined::evalImpl() {
  const LDSymbol *Symbol = m_Module.getNamePool().findSymbol(m_Name);
  if (Symbol == nullptr)
    return 0;

  if (Symbol->scriptDefined()) {
    if (Symbol->scriptValueDefined())
      return 1;
    return 0;
  }
  return 1;
}

void Defined::getSymbols(std::vector<ResolveInfo *> &Symbols) {}

void Defined::getSymbolNames(std::unordered_set<std::string> &symbolTokens) {}

bool Defined::hasDot() const {
  LDSymbol *Symbol = m_Module.getNamePool().findSymbol(m_Name);
  return Symbol == m_Module.getDotSymbol();
}

//===----------------------------------------------------------------------===//
/// DataSegmentAlign Operator
void DataSegmentAlign::commit() {
  m_maxPageSize.commit();
  m_commonPageSize.commit();
  Expression::commit();
}
void DataSegmentAlign::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  // format output for operator
  Outs << m_Name << "(";
  m_maxPageSize.dump(Outs, WithValues);
  Outs << ", ";
  m_commonPageSize.dump(Outs, WithValues);
  Outs << ")";
}
eld::Expected<uint64_t> DataSegmentAlign::evalImpl() {
  auto MaxPageSize = m_maxPageSize.eval();
  if (!MaxPageSize)
    return MaxPageSize;
  auto CommonPageSize = m_commonPageSize.eval();
  if (!CommonPageSize)
    return CommonPageSize;
  uint64_t Dot = m_Module.getDotSymbol()->value();
  uint64_t Form1 = 0, Form2 = 0;

  uint64_t DotAligned = Dot;

  alignAddress(DotAligned, MaxPageSize.value());

  Form1 = DotAligned + (Dot & (MaxPageSize.value() - 1));
  Form2 = DotAligned + (Dot & (MaxPageSize.value() - CommonPageSize.value()));

  if (Form1 <= Form2)
    return Form1;
  else
    return Form2;
}
void DataSegmentAlign::getSymbols(std::vector<ResolveInfo *> &Symbols) {}

void DataSegmentAlign::getSymbolNames(
    std::unordered_set<std::string> &symbolTokens) {}

//===----------------------------------------------------------------------===//
/// DataSegmentRelRoEnd Operator
void DataSegmentRelRoEnd::commit() {
  m_Expr1.commit();
  m_Expr2.commit();
  m_commonPageSize.commit();
  Expression::commit();
}
void DataSegmentRelRoEnd::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  // format output for operator
  Outs << m_Name << "(";
  m_Expr1.dump(Outs, WithValues);
  Outs << ", ";
  m_Expr2.dump(Outs, WithValues);
  Outs << ")";
}
eld::Expected<uint64_t> DataSegmentRelRoEnd::evalImpl() {
  auto CommonPageSize = m_commonPageSize.eval();
  if (!CommonPageSize)
    return CommonPageSize;
  auto Expr1 = m_Expr1.eval();
  if (!Expr1)
    return Expr1;
  auto Expr2 = m_Expr2.eval();
  if (!Expr2)
    return Expr2;
  uint64_t Value = Expr1.value() + Expr2.value();
  alignAddress(Value, CommonPageSize.value());
  return Value;
}
void DataSegmentRelRoEnd::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  m_Expr1.getSymbols(Symbols);
  m_Expr2.getSymbols(Symbols);
}

void DataSegmentRelRoEnd::getSymbolNames(
    std::unordered_set<std::string> &symbolTokens) {
  m_Expr1.getSymbolNames(symbolTokens);
  m_Expr2.getSymbolNames(symbolTokens);
}

bool DataSegmentRelRoEnd::hasDot() const {
  return m_Expr1.hasDot() || m_Expr2.hasDot();
}

//===----------------------------------------------------------------------===//
/// DataSegmentEnd Operator
void DataSegmentEnd::commit() {
  m_Expr.commit();
  Expression::commit();
}
void DataSegmentEnd::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  // format output for operator
  Outs << m_Name << "(";
  m_Expr.dump(Outs, WithValues);
  Outs << ")";
}
eld::Expected<uint64_t> DataSegmentEnd::evalImpl() {
  // evaluate sub expressions
  auto Expr = m_Expr.eval();
  if (!Expr)
    return Expr;
  return Expr.value() != 0; // TODO: What does this do?
}
void DataSegmentEnd::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  m_Expr.getSymbols(Symbols);
}

void DataSegmentEnd::getSymbolNames(
    std::unordered_set<std::string> &symbolTokens) {
  m_Expr.getSymbolNames(symbolTokens);
}

bool DataSegmentEnd::hasDot() const { return m_Expr.hasDot(); }

//===----------------------------------------------------------------------===//
/// Max Operator
void Max::commit() {
  m_Left.commit();
  m_Right.commit();
  Expression::commit();
}
void Max::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (m_Paren)
    Outs << "(";
  Outs << m_Name << "(";
  // format output for operator
  m_Left.dump(Outs, WithValues);
  Outs << ",";
  m_Right.dump(Outs, WithValues);
  Outs << ")";
  if (m_Paren)
    Outs << ")";
}
eld::Expected<uint64_t> Max::evalImpl() {
  // evaluate sub expressions
  auto Left = m_Left.eval();
  if (!Left)
    return Left;
  auto Right = m_Right.eval();
  if (!Right)
    return Right;

  return Left.value() > Right.value() ? Left.value() : Right.value();
}
void Max::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  m_Left.getSymbols(Symbols);
  m_Right.getSymbols(Symbols);
}

void Max::getSymbolNames(std::unordered_set<std::string> &symbolTokens) {
  m_Left.getSymbolNames(symbolTokens);
  m_Right.getSymbolNames(symbolTokens);
}

bool Max::hasDot() const { return m_Left.hasDot() || m_Right.hasDot(); }

//===----------------------------------------------------------------------===//
/// Min Operator
void Min::commit() {
  m_Left.commit();
  m_Right.commit();
  Expression::commit();
}
void Min::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (m_Paren)
    Outs << "(";
  // format output for operator
  m_Left.dump(Outs, WithValues);
  Outs << " " << m_Name << " ";
  m_Right.dump(Outs, WithValues);
  if (m_Paren)
    Outs << ")";
}
eld::Expected<uint64_t> Min::evalImpl() {
  // evaluate sub expressions
  auto Left = m_Left.eval();
  if (!Left)
    return Left;
  auto Right = m_Right.eval();
  if (!Right)
    return Right;
  return Left.value() < Right.value() ? Left.value() : Right.value();
}
void Min::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  m_Left.getSymbols(Symbols);
  m_Right.getSymbols(Symbols);
}

void Min::getSymbolNames(std::unordered_set<std::string> &symbolTokens) {
  m_Left.getSymbolNames(symbolTokens);
  m_Right.getSymbolNames(symbolTokens);
}
bool Min::hasDot() const { return m_Left.hasDot() || m_Right.hasDot(); }

//===----------------------------------------------------------------------===//
/// Fill
void Fill::commit() {
  m_Expr.commit();
  Expression::commit();
}
void Fill::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  Outs << "FILL"
       << "(";
  m_Expr.dump(Outs, WithValues);
  Outs << ")";
}
eld::Expected<uint64_t> Fill::evalImpl() { return m_Expr.eval(); }

void Fill::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  m_Expr.getSymbols(Symbols);
}

void Fill::getSymbolNames(std::unordered_set<std::string> &symbolTokens) {
  m_Expr.getSymbolNames(symbolTokens);
}

bool Fill::hasDot() const { return m_Expr.hasDot(); }

//===----------------------------------------------------------------------===//
/// Log2Ceil operator
void Log2Ceil::commit() {
  m_Expr.commit();
  Expression::commit();
}
void Log2Ceil::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  // format output for operator
  Outs << m_Name;
  Outs << "(";
  m_Expr.dump(Outs, WithValues);
  Outs << ")";
}

eld::Expected<uint64_t> Log2Ceil::evalImpl() {
  auto Val = m_Expr.eval();
  if (!Val)
    return Val;
  return llvm::Log2_64_Ceil(std::max(Val.value(), UINT64_C(1)));
}

void Log2Ceil::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  m_Expr.getSymbols(Symbols);
}

void Log2Ceil::getSymbolNames(std::unordered_set<std::string> &symbolTokens) {
  m_Expr.getSymbolNames(symbolTokens);
}

bool Log2Ceil::hasDot() const { return m_Expr.hasDot(); }

//===----------------------------------------------------------------------===//
/// LogicalOp Operator
void LogicalOp::commit() {
  m_Left.commit();
  m_Right.commit();
  Expression::commit();
}

void LogicalOp::dump(llvm::raw_ostream &Outs, bool WithValues) const {
  if (m_Paren)
    Outs << "(";
  if (!hasAssign()) {
    // format output for operator
    m_Left.dump(Outs, WithValues);
    if (isLogicalAnd())
      Outs << " "
           << "&&"
           << " ";
    if (isLogicalOR())
      Outs << " "
           << "||"
           << " ";
  }
  m_Right.dump(Outs, WithValues);
  if (m_Paren)
    Outs << ")";
}
eld::Expected<uint64_t> LogicalOp::evalImpl() {
  // evaluate sub expressions
  auto Left = m_Left.eval();
  if (!Left)
    return Left;
  auto Right = m_Right.eval();
  if (!Right)
    return Right;
  if (isLogicalAnd())
    return Left.value() && Right.value();
  ASSERT(isLogicalOR(), "logical operator can be || or && only");
  return Left.value() || Right.value();
}

void LogicalOp::getSymbols(std::vector<ResolveInfo *> &Symbols) {
  m_Left.getSymbols(Symbols);
  m_Right.getSymbols(Symbols);
}

void LogicalOp::getSymbolNames(std::unordered_set<std::string> &symbolTokens) {
  m_Left.getSymbolNames(symbolTokens);
  m_Right.getSymbolNames(symbolTokens);
}

bool LogicalOp::hasDot() const { return m_Left.hasDot() || m_Right.hasDot(); }
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
  Outs << m_Name;
  Outs << ")";
}

eld::Expected<uint64_t> QueryMemory::evalImpl() {
  auto Region = m_Module.getScript().getMemoryRegion(m_Name);
  if (!Region)
    return std::move(Region.error());
  if (isOrigin())
    return Region.value()->getOrigin();
  return Region.value()->getLength();
}

void QueryMemory::getSymbols(std::vector<ResolveInfo *> &Symbols) {}

void QueryMemory::getSymbolNames(
    std::unordered_set<std::string> &symbolTokens) {}
