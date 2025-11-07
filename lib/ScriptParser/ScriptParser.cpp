//===- ScriptParser.cpp----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
//
// This file contains a recursive-descendent parser for linker scripts.
// Parsed results are stored to Config and Script global objects.
//
//===----------------------------------------------------------------------===//
#include "eld/ScriptParser/ScriptParser.h"
#include "eld/Config/GeneralOptions.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Input/InputFile.h"
#include "eld/Input/LinkerScriptFile.h"
#include "eld/PluginAPI/DiagnosticEntry.h"
#include "eld/Script/Assignment.h"
#include "eld/Script/ExcludeFiles.h"
#include "eld/Script/Expression.h"
#include "eld/Script/InputSectDesc.h"
#include "eld/Script/OutputSectDesc.h"
#include "eld/Script/PhdrDesc.h"
#include "eld/Script/ScriptFile.h"
#include "eld/Script/StrToken.h"
#include "eld/Script/WildcardPattern.h"
#include "eld/ScriptParser/ScriptLexer.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MemoryBufferRef.h"
#include "llvm/Support/SaveAndRestore.h"
#include <optional>
#include <utility>

using namespace llvm;
using namespace eld::v2;
using namespace eld;

ScriptParser::ScriptParser(eld::LinkerConfig &Config, eld::ScriptFile &File)
    : ScriptLexer(Config, File) {}

void ScriptParser::readLinkerScript() {
  while (!atEOF()) {
    StringRef Tok = next();
    if (atEOF())
      break;

    if (Tok == ";") {
      continue;
    }

    if (Tok == "ENTRY") {
      readEntry();
    } else if (Tok == "SECTIONS") {
      readSections();
    } else if (Tok == "INPUT" || Tok == "GROUP") {
      bool IsInputCmd = (Tok == "INPUT");
      readInputOrGroup(IsInputCmd);
    } else if (Tok == "OUTPUT") {
      readOutput();
    } else if (Tok == "PHDRS") {
      readPhdrs();
    } else if (Tok == "NOCROSSREFS") {
      readNoCrossRefs();
    } else if (Tok == "SEARCH_DIR") {
      readSearchDir();
    } else if (Tok == "OUTPUT_ARCH") {
      readOutputArch();
    } else if (Tok == "MEMORY") {
      readMemory();
    } else if (Tok == "EXTERN") {
      readExtern();
    } else if (Tok == "REGION_ALIAS") {
      readRegionAlias();
    } else if (Tok == "OUTPUT_FORMAT") {
      readOutputFormat();
    } else if (Tok == "VERSION") {
      readVersion();
    } else if (readInclude(Tok)) {
    } else if (readAssignment(Tok)) {
    } else if (readPluginDirective(Tok)) {
    } else {
      setError("unknown directive: " + Tok);
    }
  }
}

bool ScriptParser::readAssignment(llvm::StringRef Tok) {
  if (Tok == "ASSERT") {
    readAssert();
    // Read optional semi-colon at the end of ASSERT.
    consume(";");
    return true;
  }
  bool Ret = false;
  StringRef Op = peek(LexState::Expr);
  if (Op.starts_with("=") ||
      (Op.size() == 2 && Op[1] == '=' && strchr("*/+-&|^", Op[0])) ||
      Op == "<<=" || Op == ">>=") {
    Ret = readSymbolAssignment(Tok);
  } else if (Tok == "PROVIDE" || Tok == "HIDDEN" || Tok == "PROVIDE_HIDDEN") {
    readProvideHidden(Tok);
    Ret = true;
  }
  if (Ret)
    expectButContinue(";");
  return Ret;
}

void ScriptParser::readEntry() {
  expect("(");
  StringRef Tok = next();
  StringRef EntrySymbol = unquote(Tok);
  expect(")");
  auto *EntryCmd = ThisScriptFile.addEntryPoint(EntrySymbol.str());
  EntryCmd->setLineNumberInContext(PrevTokLine);
  if (ThisConfig.options().shouldTraceLinkerScript())
    EntryCmd->dump(llvm::outs());
}

eld::Expression *ScriptParser::readExpr() {
  if (atEOF()) {
    Module &Module = ThisScriptFile.module();
    // We do not return nullptr here because the returned expression is
    // dereferenced at many places. We can add a null-pointer check everywhere,
    // but that would impose issues if we want to extend the parser to continue
    // parsing despite errors (the way we do with --no-inhibit-exec for overall
    // linking). Adding checks everywhere would also violate the parser design
    // to be able to continue parsing even after errors have occurred.
    return make<NullExpression>(Module);
  }
  // Our lexer is context-aware. Set the in-expression bit so that
  // they apply different tokenization rules.
  enum LexState Orig = LexState;
  LexState = LexState::Expr;
  eld::Expression *E = readExpr1(readPrimary(), /*minPrec=*/0);
  LexState = Orig;
  return E;
}

eld::Expression *ScriptParser::readExpr1(eld::Expression *Lhs, int MinPrec) {
  while (!atEOF() && diagnose()) {
    // Read an operator and an expression.
    StringRef Op1 = peek();
    if (precedence(Op1) < MinPrec)
      break;
    if (consume("?"))
      return readTernary(Lhs);
    skip();
    eld::Expression *Rhs = readPrimary();

    // Evaluate the remaining part of the expression first if the
    // next operator has greater precedence than the previous one.
    // For example, if we have read "+" and "3", and if the next
    // operator is "*", then we'll evaluate 3 * ... part first.
    while (!atEOF()) {
      StringRef Op2 = peek();
      if (precedence(Op2) <= precedence(Op1))
        break;
      Rhs = readExpr1(Rhs, precedence(Op2));
    }

    Lhs = &combine(Op1, *Lhs, *Rhs);
  }
  return Lhs;
}

int ScriptParser::precedence(StringRef Op) {
  return StringSwitch<int>(Op)
      .Cases("*", "/", "%", 11)
      .Cases("+", "-", 10)
      .Cases("<<", ">>", 9)
      .Cases({"<", "<=", ">", ">="}, 8)
      .Cases("==", "!=", 7)
      .Case("&", 6)
      .Case("^", 5)
      .Case("|", 4)
      .Case("&&", 3)
      .Case("||", 2)
      .Case("?", 1)
      .Default(-1);
}

eld::Expression &ScriptParser::combine(llvm::StringRef Op, eld::Expression &L,
                                       eld::Expression &R) {
  Module &Module = ThisScriptFile.module();
  if (Op == "+")
    return *(make<Add>(Module, L, R));
  if (Op == "-")
    return *(make<Subtract>(Module, L, R));
  if (Op == "*")
    return *(make<Multiply>(Module, L, R));
  if (Op == "/") {
    // FIXME: It be useful to pass current location for reporting
    // division by zero error!
    return *(make<Divide>(Module, L, R));
  }
  if (Op == "%") {
    // FIXME: It be useful to pass current location for reporting
    // modulo by zero error!
    return *(make<Modulo>(Module, L, R));
  }
  if (Op == "<<")
    return *(make<LeftShift>(Module, L, R));
  if (Op == ">>")
    return *(make<RightShift>(Module, L, R));
  if (Op == "<")
    return *(make<ConditionLT>(Module, L, R));
  if (Op == ">")
    return *(make<ConditionGT>(Module, L, R));
  if (Op == ">=")
    return *(make<ConditionGTE>(Module, L, R));
  if (Op == "<=")
    return *(make<ConditionLTE>(Module, L, R));
  if (Op == "==")
    return *(make<ConditionEQ>(Module, L, R));
  if (Op == "!=")
    return *(make<ConditionNEQ>(Module, L, R));
  if (Op == "||")
    return *(make<LogicalOp>(Expression::LOGICAL_OR, Module, L, R));
  if (Op == "&&")
    return *(make<LogicalOp>(Expression::LOGICAL_AND, Module, L, R));
  if (Op == "&")
    return *(make<BitwiseAnd>(Module, L, R));
  if (Op == "^")
    return *(make<BitwiseXor>(Module, L, R));
  if (Op == "|")
    return *(make<BitwiseOr>(Module, L, R));
  llvm_unreachable("invalid operator");
}

eld::Expression *ScriptParser::readPrimary() {
  if (peek() == "(")
    return readParenExpr(/*setParen=*/true);

  Module &Module = ThisScriptFile.module();
  if (consume("~")) {
    Expression *E = readPrimary();
    return make<Complement>(Module, *E);
  }
  if (consume("!")) {
    Expression *E = readPrimary();
    return make<UnaryNot>(Module, *E);
  }
  if (consume("-")) {
    Expression *E = readPrimary();
    return make<UnaryMinus>(Module, *E);
  }
  if (consume("+")) {
    Expression *E = readPrimary();
    return make<UnaryPlus>(Module, *E);
  }

  StringRef Tok = next();
  std::string Location = getCurrentLocation();

  // Built-in functions are parsed here.
  // https://sourceware.org/binutils/docs/ld/Builtin-Functions.html.
  if (Tok == "ABSOLUTE") {
    Expression *E = readParenExpr(/*setParen=*/true);
    return make<Absolute>(Module, *E);
  }
  if (Tok == "ADDR") {
    StringRef Name = unquote(readParenLiteral());
    // FIXME: Location might be handly for 'undefined section' error.
    return make<Addr>(Module, Name.str());
  }
  if (Tok == "ALIGN") {
    expect("(");
    Expression *E = readExpr();
    if (consume(")")) {
      // FIXME: Location given here may be overwritten for outermost ALIGN
      // expressions.
      return make<AlignExpr>(Module, Location, *E, *make<Symbol>(Module, "."));
    }
    expect(",");
    Expression *E2 = readExpr();
    expect(")");
    // FIXME: Location given here may be overwritten for outermost ALIGN
    // expressions.
    return make<AlignExpr>(Module, Location, *E2, *E);
  }
  if (Tok == "ALIGNOF") {
    StringRef Name = unquote(readParenLiteral());
    // FIXME: Location might be useful for undefined section related errors.
    return make<AlignOf>(Module, Name.str());
  }
  if (Tok == "ASSERT")
    return readAssert();
  if (Tok == "CONSTANT")
    return readConstant();
  if (Tok == "DATA_SEGMENT_ALIGN") {
    expect("(");
    Expression *E1 = readExpr();
    expect(",");
    Expression *E2 = readExpr();
    expect(")");
    return make<DataSegmentAlign>(Module, *E1, *E2);
  }
  if (Tok == "DATA_SEGMENT_END") {
    expect("(");
    Expression *E = readExpr();
    expect(")");
    return make<DataSegmentEnd>(Module, *E);
  }
  if (Tok == "DATA_SEGMENT_RELRO_END") {
    expect("(");
    Expression *E1 = readExpr();
    expect(",");
    Expression *E2 = readExpr();
    expect(")");
    return make<DataSegmentRelRoEnd>(Module, *E1, *E2);
  }
  if (Tok == "DEFINED") {
    StringRef Name = unquote(readParenLiteral());
    return make<Defined>(Module, Name.str());
  }
  if (Tok == "LENGTH") {
    StringRef Name = readParenLiteral();
    return make<QueryMemory>(Expression::LENGTH, Module, Name.str());
  }
  if (Tok == "LOADADDR") {
    StringRef Name = unquote(readParenLiteral());
    return make<LoadAddr>(Module, Name.str());
  }
  if (Tok == "LOG2CEIL") {
    expect("(");
    Expression *E = readExpr();
    expect(")");
    return make<Log2Ceil>(Module, *E);
  }
  if (Tok == "MAX" || Tok == "MIN") {
    expect("(");
    Expression *E1 = readExpr();
    expect(",");
    Expression *E2 = readExpr();
    expect(")");
    if (Tok == "MIN")
      return make<Min>(Module, *E1, *E2);
    return make<Max>(Module, *E1, *E2);
  }
  if (Tok == "ORIGIN") {
    StringRef Name = readParenLiteral();
    return make<QueryMemory>(Expression::ORIGIN, Module, Name.str());
  }
  if (Tok == "SEGMENT_START") {
    expect("(");
    StringRef Name = unquote(next());
    expect(",");
    Expression *E = readExpr();
    expect(")");
    return make<SegmentStart>(Module, Name.str(), *E);
  }
  if (Tok == "SIZEOF") {
    StringRef Name = readParenName();
    return make<SizeOf>(Module, Name.str());
  }
  if (Tok == "SIZEOF_HEADERS")
    return make<SizeOfHeaders>(Module, &ThisScriptFile);

  // Tok is a literal number.
  if (std::optional<uint64_t> Val = parseInt(Tok))
    return make<Integer>(Module, Tok.str(), Val.value());

  // Tok is a symbol name.
  if (Tok.starts_with("\""))
    Tok = unquote(Tok);
  if (!isValidSymbolName(Tok))
    setError("malformed number: " + Tok);
  return make<Symbol>(Module, Tok.str());
}

Expression *ScriptParser::readParenExpr(bool SetParen) {
  expect("(");
  Expression *E = readExpr();
  expect(")");
  if (SetParen)
    E->setParen();
  return E;
}

StringRef ScriptParser::readParenLiteral() {
  expect("(");
  enum LexState Orig = LexState;
  LexState = LexState::Expr;
  StringRef Tok = next();
  LexState = Orig;
  expect(")");
  return Tok;
}

Expression *ScriptParser::readConstant() {
  StringRef S = readParenLiteral();
  Module &Module = ThisScriptFile.module();
  if (S == "COMMONPAGESIZE")
    return make<Constant>(Module, "COMMONPAGESIZE", Expression::COMMONPAGESIZE);
  if (S == "MAXPAGESIZE")
    return make<Constant>(Module, "MAXPAGESIZE", Expression::MAXPAGESIZE);
  setError("unknown constant: " + S);
  return make<Integer>(Module, "", 0);
}

std::optional<uint64_t> ScriptParser::parseInt(StringRef Tok) const {
  // Hexadecimal
  uint64_t Val = 0;
  if (Tok.starts_with_insensitive("0x")) {
    if (!to_integer(Tok.substr(2), Val, 16))
      return std::nullopt;
    return Val;
  }
  if (Tok.ends_with_insensitive("H")) {
    if (!to_integer(Tok.drop_back(), Val, 16))
      return std::nullopt;
    return Val;
  }

  // Decimal
  if (Tok.ends_with_insensitive("K")) {
    if (!to_integer(Tok.drop_back(), Val, 10))
      return std::nullopt;
    return Val * 1024;
  }
  if (Tok.ends_with_insensitive("M")) {
    if (!to_integer(Tok.drop_back(), Val, 10))
      return std::nullopt;
    return Val * 1024 * 1024;
  }
  if (!to_integer(Tok, Val, 10))
    return std::nullopt;
  return Val;
}

bool ScriptParser::isValidSymbolName(StringRef S) {
  auto Valid = [](char C) {
    return isAlnum(C) || C == '$' || C == '.' || C == '_';
  };
  return !S.empty() && !isDigit(S[0]) && llvm::all_of(S, Valid);
}

bool ScriptParser::readSymbolAssignment(StringRef Tok,
                                        Assignment::Type AssignType) {
  StringRef Name = unquote(Tok);
  StringRef Op = next(LexState::Expr);

  assert(Op == "=" || Op == "*=" || Op == "/=" || Op == "+=" || Op == "-=" ||
         Op == "&=" || Op == "|=" || Op == "^=" || Op == "<<=" || Op == ">>=");
  // Note: GNU ld does not support %=.
  Expression *E = readExpr();
  Module &Module = ThisScriptFile.module();
  if (Op != "=") {
    Symbol *S = make<Symbol>(Module, Name.str());
    std::string Loc = getCurrentLocation();
    char SubOp = Op[0];
    switch (SubOp) {
    case '*':
      E = make<Multiply>(Module, *S, *E);
      break;
    case '/':
      E = make<Divide>(Module, *S, *E);
      break;
    case '+':
      E = make<Add>(Module, *S, *E);
      break;
    case '-':
      E = make<Subtract>(Module, *S, *E);
      break;
    case '<':
      E = make<LeftShift>(Module, *S, *E);
      break;
    case '>':
      E = make<RightShift>(Module, *S, *E);
      break;
    case '&':
      E = make<BitwiseAnd>(Module, *S, *E);
      break;
    case '|':
      E = make<BitwiseOr>(Module, *S, *E);
      break;
    case '^':
      E = make<BitwiseXor>(Module, *S, *E);
      break;
    default:
      llvm_unreachable("");
    }
    E->setAssign();
  }
  ThisScriptFile.addAssignment(Name.str(), E, AssignType);
  return true;
}

Expression *ScriptParser::readTernary(Expression *Cond) {
  Expression *L = readExpr();
  expect(":");
  Expression *R = readExpr();
  return make<Ternary>(ThisScriptFile.module(), *Cond, *L, *R);
}

void ScriptParser::readProvideHidden(StringRef Tok) {
  Assignment::Type AssignType;
  if (Tok == "PROVIDE")
    AssignType = Assignment::Type::PROVIDE;
  else if (Tok == "HIDDEN")
    AssignType = Assignment::Type::HIDDEN;
  else if (Tok == "PROVIDE_HIDDEN")
    AssignType = Assignment::Type::PROVIDE_HIDDEN;
  else
    llvm_unreachable("Expected PROVIDE/HIDDEN/PROVIDE_HIDDEN assignments!");
  expect("(");
  llvm::SaveAndRestore SaveLexState(LexState, LexState::Expr);
  Tok = next();
  if (peek() != "=") {
    setError("= expected, but got " + next());
    while (!atEOF() && next() != ")")
      ;
  }
  readSymbolAssignment(Tok, AssignType);
  expect(")");
}

void ScriptParser::readSections() {
  expect("{");
  ThisScriptFile.enterSectionsCmd();
  while (peek() != "}" && !atEOF()) {
    llvm::StringRef Tok = next();
    if (readInclude(Tok)) {
    } else if (readAssignment(Tok)) {
    } else {
      readOutputSectionDescription(Tok);
    }
  }
  expect("}");
  ThisScriptFile.leaveSectionsCmd();
}

Expression *ScriptParser::readAssert() {
  expect("(");
  Expression *E = readExpr();
  expect(",");
  StringRef Msg = unquote(next());
  expect(")");
  Expression *AssertCmd =
      make<eld::AssertCmd>(ThisScriptFile.module(), Msg.str(), *E);
  ThisScriptFile.addAssignment("ASSERT", AssertCmd, Assignment::ASSERT);
  return AssertCmd;
}

void ScriptParser::readInputOrGroup(bool IsInputCmd) {
  expect("(");
  ThisScriptFile.createStringList();
  while (peek() != ")" && !atEOF()) {
    if (consume("AS_NEEDED")) {
      readAsNeeded();
    } else
      addFile(unquote(next()));
    consume(",");
  }
  expect(")");
  StringList *Inputs = ThisScriptFile.getCurrentStringList();
  if (IsInputCmd)
    ThisScriptFile.addInputCmd(
        *Inputs,
        ThisScriptFile.getLinkerScriptFile().getInput()->getAttribute());
  else
    ThisScriptFile.addGroupCmd(
        *Inputs,
        ThisScriptFile.getLinkerScriptFile().getInput()->getAttribute());
}

void ScriptParser::readAsNeeded() {
  expect("(");
  ThisScriptFile.setAsNeeded(true);
  while (peek() != ")" && !atEOF()) {
    addFile(unquote(next()));
    consume(",");
  }
  expect(")");
  ThisScriptFile.setAsNeeded(false);
}

void ScriptParser::addFile(StringRef Name) {
  StrToken *InputStrTok = nullptr;
  if (Name.consume_front("-l"))
    InputStrTok = ThisScriptFile.createNameSpecToken(Name.str(),
                                                     ThisScriptFile.asNeeded());
  else
    InputStrTok =
        ThisScriptFile.createFileToken(Name.str(), ThisScriptFile.asNeeded());
  ThisScriptFile.getCurrentStringList()->pushBack(InputStrTok);
}

void ScriptParser::readOutput() {
  expect("(");
  StringRef Name = next();
  ThisScriptFile.addOutputCmd(unquote(Name).str());
  expect(")");
}

void ScriptParser::readOutputSectionDescription(llvm::StringRef Tok) {
  llvm::StringRef OutSectName = unquote(Tok);
  OutputSectDesc::Prolog Prologue = readOutputSectDescPrologue();
  ThisScriptFile.enterOutputSectDesc(OutSectName.str(), Prologue);
  expect("{");
  while (peek() != "}" && !atEOF()) {
    StringRef Tok = next();
    if (Tok == ";") {
      // Empty commands are allowed. Do nothing.
    } else if (Tok == "FILL") {
      readFill();
    } else if (readInclude(Tok)) {
    } else if (readOutputSectionData(Tok)) {
    } else if (readAssignment(Tok)) {
    } else {
      readInputSectionDescription(Tok);
    }
  }
  expect("}");
  OutputSectDesc::Epilog Epilogue = readOutputSectDescEpilogue();
  ThisScriptFile.leavingOutputSectDesc();
  ThisScriptFile.leaveOutputSectDesc(Epilogue);
}

void ScriptParser::readInputSectionDescription(StringRef Tok) {
  InputSectDesc::Policy Policy = InputSectDesc::Policy::NoKeep;
  if (Tok == "KEEP")
    Policy = InputSectDesc::Policy::Keep;
  else if (Tok == "DONTMOVE")
    Policy = InputSectDesc::Policy::Fixed;
  else if (Tok == "KEEP_DONTMOVE")
    Policy = InputSectDesc::Policy::KeepFixed;
  if (Policy != InputSectDesc::Policy::NoKeep) {
    expect("(");
    Tok = next();
  }
  InputSectDesc::Spec ISDSpec = readInputSectionDescSpec(Tok);
  if (Policy != InputSectDesc::Policy::NoKeep)
    expect(")");
  ThisScriptFile.addInputSectDesc(Policy, ISDSpec);
}

InputSectDesc::Spec ScriptParser::readInputSectionDescSpec(StringRef Tok) {
  ExcludeFiles *EF = nullptr;
  if (Tok == "EXCLUDE_FILE") {
    EF = readExcludeFile();
    Tok = next();
  }
  WildcardPattern *FilePat = nullptr, *ArchiveMem = nullptr;
  bool IsArchive = false;
  if (!isValidFilePattern(Tok))
    setError("Invalid file pattern: " + Tok);
  if (!Tok.contains(':'))
    FilePat = ThisScriptFile.createWildCardPattern(Tok);
  else {
    std::pair<llvm::StringRef, llvm::StringRef> Split = Tok.split(':');
    FilePat = ThisScriptFile.createWildCardPattern(Split.first);
    if (!Split.second.empty()) {
      ArchiveMem = ThisScriptFile.createWildCardPattern(Split.second);
    }
    llvm::StringRef peekTok = peek();
    if (!atEOF() && peekTok != "(" &&
        computeLineNumber(peekTok) == getCurrentLineNumber()) {
      next();
      if (ThisConfig.showLinkerScriptWarnings())
        setWarn("Space between archive:member file pattern is deprecated");
      ArchiveMem = ThisScriptFile.createWildCardPattern(peekTok);
    }
    IsArchive = true;
  }
  StringList *WildcardSections = nullptr;
  if (consume("(")) {
    ThisScriptFile.createStringList();
    while (peek() != ")" && !atEOF()) {
      WildcardPattern *SectPat = readWildcardPattern();
      if (!isValidSectionPattern(SectPat->name()))
        setError("Invalid section pattern: " + SectPat->name());
      ThisScriptFile.getCurrentStringList()->pushBack(SectPat);
    }
    expect(")");
    WildcardSections = ThisScriptFile.getCurrentStringList();
  }
  InputSectDesc::Spec ISDSpec;
  ISDSpec.initialize();
  ISDSpec.WildcardFilePattern = FilePat;
  ISDSpec.WildcardSectionPattern = WildcardSections;
  ISDSpec.InputArchiveMember = ArchiveMem;
  ISDSpec.InputIsArchive = IsArchive;
  ISDSpec.ExcludeFilesRule = EF;
  return ISDSpec;
}

OutputSectDesc::Prolog ScriptParser::readOutputSectDescPrologue() {
  OutputSectDesc::Prolog Prologue;
  Prologue.init();
  if (peek(LexState::Expr) != ":") {
    if (consume("(")) {
      if (!readOutputSectTypeAndPermissions(Prologue, peek()))
        Prologue.OutputSectionVMA = readExpr();
      expect(")");
    } else if (peek() == "{") {
      expect(":");
    } else {
      Prologue.PluginCmd = readOutputSectionPluginDirective();
      if (!Prologue.PluginCmd)
        Prologue.OutputSectionVMA = readExpr();
    }

    if (Prologue.OutputSectionVMA != nullptr && consume("(")) {
      StringRef Tok = peek();
      if (!readOutputSectTypeAndPermissions(Prologue, Tok))
        setError("Invalid output section type: " + Tok);
      expect(")");
    }

    if (!Prologue.PluginCmd)
      Prologue.PluginCmd = readOutputSectionPluginDirective();
    if (Prologue.PluginCmd)
      Prologue.PluginCmd->setHasOutputSection();
  }

  expect(LexState::Expr, ":");

  if (consume("AT"))
    Prologue.OutputSectionLMA = readParenExpr(/*setParen=*/false);
  if (consume("ALIGN"))
    Prologue.Alignment = readParenExpr(/*setParen=*/false);
  if (consume("ALIGN_WITH_INPUT"))
    Prologue.HasAlignWithInput = true;

  if (Prologue.Alignment && Prologue.HasAlignWithInput) {
    setError("ALIGN_WITH_INPUT specified with explicit alignment ");
  }

  if (consume("SUBALIGN"))
    Prologue.OutputSectionSubaAlign = readParenExpr(/*setParen=*/false);

  if (consume("ONLY_IF_RO"))
    Prologue.SectionConstraint = OutputSectDesc::Constraint::ONLY_IF_RO;
  else if (consume("ONLY_IF_RW"))
    Prologue.SectionConstraint = OutputSectDesc::Constraint::ONLY_IF_RW;
  return Prologue;
}

bool ScriptParser::readOutputSectTypeAndPermissions(
    OutputSectDesc::Prolog &Prologue, llvm::StringRef Tok) {
  std::optional<OutputSectDesc::Type> ExpType = readOutputSectType(Tok);
  if (ExpType)
    Prologue.ThisType = ExpType.value();
  else
    return false;
  next();
  if (consume(",")) {
    Tok = next();
    std::optional<uint32_t> ExpFlag = readOutputSectPermissions(Tok);
    if (ExpFlag)
      Prologue.SectionFlag = ExpFlag.value();
    else
      setError("Invalid permission flag: " + Tok);
  }
  return true;
}

std::optional<OutputSectDesc::Type>
ScriptParser::readOutputSectType(StringRef Tok) {
  return StringSwitch<std::optional<OutputSectDesc::Type>>(Tok)
      .Case("NOLOAD", OutputSectDesc::Type::NOLOAD)
      .Case("DSECT", OutputSectDesc::Type::DSECT)
      .Case("COPY", OutputSectDesc::Type::COPY)
      .Case("INFO", OutputSectDesc::Type::INFO)
      .Case("OVERLAY", OutputSectDesc::Type::OVERLAY)
      .Case("PROGBITS", OutputSectDesc::Type::PROGBITS)
      .Case("UNINIT", OutputSectDesc::Type::UNINIT)
      .Default(std::nullopt);
}

std::optional<uint32_t>
ScriptParser::readOutputSectPermissions(llvm::StringRef Tok) {
  if (std::optional<uint64_t> Permissions = parseInt(Tok))
    return Permissions;
  return StringSwitch<std::optional<uint32_t>>(Tok)
      .Case("RW", OutputSectDesc::Permissions::RW)
      .Case("RWX", OutputSectDesc::Permissions::RWX)
      .Case("RX", OutputSectDesc::Permissions::RX)
      .Case("R", OutputSectDesc::Permissions::R)
      .Default(std::nullopt);
}

void ScriptParser::readPhdrs() {
  expect("{");
  ThisScriptFile.enterPhdrsCmd();
  while (peek() != "}" && !atEOF()) {
    PhdrSpec PhdrSpec;
    PhdrSpec.init();
    llvm::StringRef NameTok = next();
    PhdrSpec.Name =
        ThisScriptFile.createParserStr(NameTok.data(), NameTok.size());
    llvm::StringRef TypeTok = next();
    auto OptPhdrType = readPhdrType(TypeTok);
    if (OptPhdrType.has_value())
      PhdrSpec.ThisType = OptPhdrType.value();
    else
      setError("invalid program header type: " + TypeTok);
    while (peek() != ";" && !atEOF()) {
      if (consume("FILEHDR"))
        PhdrSpec.ScriptHasFileHdr = true;
      else if (consume("PHDRS"))
        PhdrSpec.ScriptHasPhdr = true;
      else if (consume("AT"))
        PhdrSpec.FixedAddress = readParenExpr(/*setParen=*/false);
      else if (consume("FLAGS"))
        PhdrSpec.SectionFlags = readParenExpr(/*setParen=*/false);
      else
        setError("unexpected header attribute: " + next());
    }
    expect(";");
    if (diagnose())
      ThisScriptFile.addPhdrDesc(PhdrSpec);
  }
  expect("}");
  ThisScriptFile.leavePhdrsCmd();
}

std::optional<uint32_t> ScriptParser::readPhdrType(llvm::StringRef Tok) const {
  if (std::optional<uint64_t> Val = parseInt(Tok))
    return *Val;

  std::optional<uint32_t> Ret =
      llvm::StringSwitch<std::optional<uint32_t>>(Tok)
          .Case("PT_NULL", llvm::ELF::PT_NULL)
          .Case("PT_LOAD", llvm::ELF::PT_LOAD)
          .Case("PT_DYNAMIC", llvm::ELF::PT_DYNAMIC)
          .Case("PT_INTERP", llvm::ELF::PT_INTERP)
          .Case("PT_NOTE", llvm::ELF::PT_NOTE)
          .Case("PT_SHLIB", llvm::ELF::PT_SHLIB)
          .Case("PT_PHDR", llvm::ELF::PT_PHDR)
          .Case("PT_ARM_EXIDX", llvm::ELF::PT_ARM_EXIDX)
          .Case("PT_GNU_EH_FRAME", llvm::ELF::PT_GNU_EH_FRAME)
          .Case("PT_TLS", llvm::ELF::PT_TLS)
          .Case("PT_GNU_STACK", llvm::ELF::PT_GNU_STACK)
          .Case("PT_GNU_RELRO", llvm::ELF::PT_GNU_RELRO)
          .Default(std::nullopt);
  return Ret;
}

OutputSectDesc::Epilog ScriptParser::readOutputSectDescEpilogue() {
  MInOutputSectEpilogue = true;
  OutputSectDesc::Epilog Epilogue;

  if (consume(">")) {
    llvm::StringRef Region = unquote(next());
    Epilogue.OutputSectionMemoryRegion =
        ThisScriptFile.createParserStr(Region.data(), Region.size());
  }

  if (consume("AT")) {
    expect(">");
    llvm::StringRef Region = unquote(next());
    Epilogue.OutputSectionLMARegion =
        ThisScriptFile.createParserStr(Region.data(), Region.size());
  }

  ThisScriptFile.createStringList();
  while (diagnose() && peek().starts_with(":")) {
    llvm::StringRef Tok = next(LexState::Expr);
    llvm::StringRef PhdrName =
        (Tok.size() == 1 ? unquote(next(LexState::Expr)) : Tok.substr(1));
    ThisScriptFile.getCurrentStringList()->pushBack(
        ThisScriptFile.createParserStr(PhdrName.data(), PhdrName.size()));
  }
  Epilogue.ScriptPhdrs = ThisScriptFile.getCurrentStringList();

  if (peek() == "=" || peek().starts_with("=")) {
    LexState = LexState::Expr;
    consume("=");
    Epilogue.FillExpression = readExpr();
    LexState = LexState::Default;
  }
  // Consume optional comma following output section command.
  consume(",");
  MInOutputSectEpilogue = false;
  return Epilogue;
}

void ScriptParser::readNoCrossRefs() {
  expect("(");
  ThisScriptFile.createStringList();
  while (peek() != ")" && !atEOF()) {
    StrToken *SectionNameTok = readName((next()));
    ThisScriptFile.getCurrentStringList()->pushBack(
        ThisScriptFile.createScriptSymbol(SectionNameTok));
  }
  expect(")");
  StringList *SL = ThisScriptFile.getCurrentStringList();
  ThisScriptFile.addNoCrossRefs(*SL);
}

bool ScriptParser::readPluginDirective(llvm::StringRef Tok) {
  std::optional<plugin::Plugin::Type> OptPluginType =
      llvm::StringSwitch<std::optional<plugin::Plugin::Type>>(Tok)
          .Case("PLUGIN_SECTION_MATCHER", plugin::Plugin::Type::SectionMatcher)
          .Case("PLUGIN_ITER_SECTIONS", plugin::Plugin::Type::SectionIterator)
          .Case("PLUGIN_OUTPUT_SECTION_ITER",
                plugin::Plugin::Type::OutputSectionIterator)
          .Case("LINKER_PLUGIN", plugin::Plugin::Type::LinkerPlugin)
          .Default(std::nullopt);
  if (!OptPluginType.has_value())
    return false;
  expect("(");
  llvm::StringRef LibName = unquote(next());
  expect(",");
  llvm::StringRef PluginName = unquote(next());
  llvm::StringRef ConfigName;
  if (consume(","))
    ConfigName = unquote(next());
  expect(")");
  ThisScriptFile.addPlugin(OptPluginType.value(), LibName.str(),
                           PluginName.str(), ConfigName.str());
  return true;
}

void ScriptParser::readSearchDir() {
  expect("(");
  llvm::StringRef SearchDir = unquote(next());
  ThisScriptFile.addSearchDirCmd(SearchDir.str());
  expect(")");
}

void ScriptParser::readOutputArch() {
  expect("(");
  llvm::StringRef Arch = unquote(next());
  ThisScriptFile.addOutputArchCmd(Arch.str());
  expect(")");
}

void ScriptParser::readMemory() {
  expect("{");
  while (peek() != "}" && !atEOF()) {
    llvm::StringRef Tok = next();
    if (readInclude(Tok))
      continue;
    llvm::StringRef Name = Tok;
    StrToken *MemoryAttrs = nullptr;
    if (consume("(")) {
      MemoryAttrs = readMemoryAttributes();
      expect(")");
    }
    expect(LexState::Expr, ":");
    Expression *Origin = readMemoryAssignment({"ORIGIN", "org", "o"});
    expect(",");
    Expression *Length = readMemoryAssignment({"LENGTH", "len", "l"});
    if (Origin && Length) {
      StrToken *NameToken = readName(Name);
      ThisScriptFile.addMemoryRegion(NameToken, MemoryAttrs, Origin, Length);
    }
  }
  expect("}");
}

Expression *
ScriptParser::readMemoryAssignment(std::vector<llvm::StringRef> Names) {
  bool ExpectedToken = false;
  llvm::StringRef Tok = next();
  for (llvm::StringRef Name : Names) {
    if (Tok == Name) {
      ExpectedToken = true;
      break;
    }
  }
  if (!ExpectedToken) {
    std::string NamesJoined;
    for (auto Iter = Names.begin(); Iter != Names.end(); ++Iter) {
      llvm::StringRef Name = *Iter;
      if (Iter != Names.end() - 1)
        NamesJoined += Name.str() + ", ";
      else
        NamesJoined += "or " + Name.str();
    }
    setError("expected one of: " + NamesJoined);
    return nullptr;
  }
  expect(LexState::Expr, "=");
  return readExpr();
}

StrToken *ScriptParser::readMemoryAttributes() {
  llvm::StringRef Attrs = next();
  llvm::StringRef::size_type Sz = Attrs.find_first_not_of("rRwWxXaA!iIlL");
  if (Sz != llvm::StringRef::npos) {
    setError("invalid memory region attribute");
    return nullptr;
  }
  return ThisScriptFile.createStrToken("(" + Attrs.str() + ")");
}

void ScriptParser::readExtern() {
  expect("(");
  ThisScriptFile.createStringList();
  while (peek() != ")" && !atEOF()) {
    StrToken *SymTok = readName(next());
    ScriptSymbol *ScriptSym = ThisScriptFile.createScriptSymbol(SymTok);
    ThisScriptFile.getCurrentStringList()->pushBack(ScriptSym);
  }
  expect(")");
  StringList *SymbolList = ThisScriptFile.getCurrentStringList();
  ThisScriptFile.addExtern(*SymbolList);
}

void ScriptParser::readRegionAlias() {
  expect("(");
  StrToken *AliasToken = readName(next());
  expect(",");
  StrToken *RegionToken = readName(next());
  expect(")");

  ThisScriptFile.addRegionAlias(AliasToken, RegionToken);
}

bool ScriptParser::readOutputSectionData(llvm::StringRef Tok) {
  std::optional<OutputSectData::OSDKind> OptDataKind =
      llvm::StringSwitch<std::optional<OutputSectData::OSDKind>>(Tok)
          .Case("BYTE", OutputSectData::OSDKind::Byte)
          .Case("SHORT", OutputSectData::OSDKind::Short)
          .Case("LONG", OutputSectData::OSDKind::Long)
          .Case("QUAD", OutputSectData::OSDKind::Quad)
          .Case("SQUAD", OutputSectData::OSDKind::Squad)
          .Default(std::nullopt);
  if (!OptDataKind.has_value())
    return false;
  expect("(");
  Expression *Exp = readExpr();
  expect(")");
  ThisScriptFile.addOutputSectData(OptDataKind.value(), Exp);
  return true;
}

std::optional<WildcardPattern::SortPolicy> ScriptParser::readSortPolicy() {
  llvm::StringRef Tok = peek();
  std::optional<WildcardPattern::SortPolicy> OptSortPolicy =
      llvm::StringSwitch<std::optional<WildcardPattern::SortPolicy>>(Tok)
          .Case("SORT_NONE", WildcardPattern::SortPolicy::SORT_NONE)
          .Cases("SORT", "SORT_BY_NAME",
                 WildcardPattern::SortPolicy::SORT_BY_NAME)
          .Case("SORT_BY_ALIGNMENT",
                WildcardPattern::SortPolicy::SORT_BY_ALIGNMENT)
          .Case("SORT_BY_INIT_PRIORITY",
                WildcardPattern::SortPolicy::SORT_BY_INIT_PRIORITY)
          .Default(std::nullopt);
  if (OptSortPolicy.has_value())
    skip();
  return OptSortPolicy;
}

WildcardPattern::SortPolicy ScriptParser::computeSortPolicy(
    WildcardPattern::SortPolicy OuterSortPolicy,
    std::optional<WildcardPattern::SortPolicy> InnerSortPolicy) {
  if (!InnerSortPolicy.has_value())
    return OuterSortPolicy;
  if (OuterSortPolicy == InnerSortPolicy.value())
    return OuterSortPolicy;
  if (OuterSortPolicy == WildcardPattern::SortPolicy::SORT_BY_NAME &&
      InnerSortPolicy.value() == WildcardPattern::SortPolicy::SORT_BY_ALIGNMENT)
    return WildcardPattern::SortPolicy::SORT_BY_NAME_ALIGNMENT;
  if (OuterSortPolicy == WildcardPattern::SortPolicy::SORT_BY_ALIGNMENT &&
      InnerSortPolicy.value() == WildcardPattern::SortPolicy::SORT_BY_NAME)
    return WildcardPattern::SortPolicy::SORT_BY_ALIGNMENT_NAME;
  setError("Invalid sort policy combination");
  return WildcardPattern::SortPolicy::SORT_NONE;
}

WildcardPattern *ScriptParser::readWildcardPattern() {
  WildcardPattern *SectPat = nullptr;
  if (peek() == "EXCLUDE_FILE") {
    skip();
    ExcludeFiles *EF = readExcludeFile();
    SectPat = ThisScriptFile.createWildCardPattern(
        ThisScriptFile.createParserStr(next()),
        WildcardPattern::SortPolicy::EXCLUDE, EF);
    return SectPat;
  }
  std::optional<WildcardPattern::SortPolicy> OuterSortPolicy = readSortPolicy();
  if (OuterSortPolicy.has_value()) {
    expect("(");
    std::optional<WildcardPattern::SortPolicy> InnerSortPolicy =
        readSortPolicy();
    if (InnerSortPolicy.has_value()) {
      WildcardPattern::SortPolicy SortPolicy =
          computeSortPolicy(OuterSortPolicy.value(), InnerSortPolicy);
      expect("(");
      SectPat = ThisScriptFile.createWildCardPattern(
          ThisScriptFile.createParserStr(next()), SortPolicy);
      expect(")");
    } else {
      WildcardPattern::SortPolicy SortPolicy =
          computeSortPolicy(OuterSortPolicy.value());
      SectPat = ThisScriptFile.createWildCardPattern(
          ThisScriptFile.createParserStr(next()), SortPolicy);
    }
    expect(")");
  } else {
    SectPat = ThisScriptFile.createWildCardPattern(
        ThisScriptFile.createParserStr(next()));
  }
  return SectPat;
}

PluginCmd *ScriptParser::readOutputSectionPluginDirective() {
  llvm::StringRef Tok = peek();
  std::optional<plugin::Plugin::Type> OptPluginType =
      llvm::StringSwitch<std::optional<plugin::Plugin::Type>>(Tok)
          .Case("PLUGIN_CONTROL_FILESZ", plugin::Plugin::Type::ControlFileSize)
          .Case("PLUGIN_CONTROL_MEMSZ", plugin::Plugin::Type::ControlMemorySize)
          .Default(std::nullopt);
  if (!OptPluginType.has_value())
    return nullptr;
  skip();
  expect("(");
  llvm::StringRef LibName = unquote(next());
  expect(",");
  llvm::StringRef PluginName = unquote(next());
  llvm::StringRef ConfigName;
  if (consume(","))
    ConfigName = unquote(next());
  expect(")");
  return ThisScriptFile.addPlugin(OptPluginType.value(), LibName.str(),
                                  PluginName.str(), ConfigName.str());
}

void ScriptParser::readFill() {
  expect("(");
  Expression *E = readExpr();
  Fill *Fill = make<eld::Fill>(ThisScriptFile.module(), *E);
  ThisScriptFile.addAssignment("FILL", Fill, Assignment::FILL);
  expect(")");
}

ExcludeFiles *ScriptParser::readExcludeFile() {
  expect("(");
  ThisScriptFile.createExcludeFiles();
  ExcludeFiles *CurrentExcludeFiles = ThisScriptFile.getCurrentExcludeFiles();
  while (diagnose() && !consume(")")) {
    StrToken *ExcludePatTok = readName(next());
    ExcludePattern *P = ThisScriptFile.createExcludePattern(ExcludePatTok);
    CurrentExcludeFiles->pushBack(P);
  }
  return CurrentExcludeFiles;
}

bool ScriptParser::readInclude(llvm::StringRef Tok) {
  if (Tok != "INCLUDE" && Tok != "INCLUDE_OPTIONAL")
    return false;
  bool IsOptionalInclude = Tok == "INCLUDE_OPTIONAL";
  llvm::StringRef FileName = unquote(next());
  LayoutInfo *layoutInfo = ThisScriptFile.module().getLayoutInfo();
  bool Result = true;
  std::string ResolvedFileName = ThisScriptFile.findIncludeFile(
      FileName.str(), Result, /*state=*/!IsOptionalInclude);
  if (layoutInfo)
    layoutInfo->recordLinkerScript(ResolvedFileName, Result);
  if (!Result && IsOptionalInclude)
    return true;
  ThisScriptFile.module().getScript().addToHash(ResolvedFileName);
  if (Result) {
    Input *Input = make<eld::Input>(ResolvedFileName,
                                    ThisConfig.getDiagEngine(), Input::Script);
    if (!Input->resolvePath(ThisConfig)) {
      setError("Cannot resolve path " + ResolvedFileName);
      // return value by ScriptParser functions indicates that the command was
      // parsed.
      return true;
    }
    InputFile *IF = InputFile::create(Input, InputFile::GNULinkerScriptKind,
                                      ThisConfig.getDiagEngine());
    Input->setInputFile(IF);
    ThisScriptFile.setContext(IF);
    llvm::MemoryBufferRef MemRef = Input->getMemoryBufferRef();
    Buffers.push_back(CurBuf);
    CurBuf = Buffer(MemRef);
    MemoryBuffers.push_back(MemRef);
    if (!ActiveFilenames.insert(CurBuf.Filename).second)
      setError("there is a cycle in linker script INCLUDEs");
  }
  return true;
}

llvm::StringRef ScriptParser::readParenName() {
  expect("(");
  llvm::SaveAndRestore SaveLexState(LexState, LexState::Default);
  StringRef Tok = unquote(next());
  expect(")");
  return Tok;
}

void ScriptParser::readOutputFormat() {
  expect("(");
  llvm::StringRef DefaultFormat = unquote(next());
  if (consume(",")) {
    llvm::StringRef BigFormat = unquote(next());
    expect(",");
    llvm::StringRef LittleFormat = unquote(next());
    ThisScriptFile.addOutputFormatCmd(DefaultFormat.str(), BigFormat.str(),
                                      LittleFormat.str());
  } else {
    ThisScriptFile.addOutputFormatCmd(DefaultFormat.str());
  }
  expect(")");
}

void ScriptParser::readVersion() {
  expect("{");
  readVersionScriptCommand();
  expect("}");
}

void ScriptParser::readVersionScriptCommand() {
  ThisScriptFile.createVersionScript();
  if (consume("{")) {
    VersionScriptNode *VSN =
        ThisScriptFile.getVersionScript()->createVersionScriptNode();
    readVersionSymbols(*VSN);
    expect(";");
    return;
  }
  while (peek() != "}" && !atEOF()) {
    llvm::StringRef VerStr = next();
    if (VerStr == "{") {
      setError("anonymous version definition is used in "
               "combination with other version definitions");
      return;
    }
    expect("{");
    readVersionDeclaration(VerStr);
  }
}

void ScriptParser::readVersionDeclaration(llvm::StringRef VerStr) {
  VersionScriptNode *VSN =
      ThisScriptFile.getVersionScript()->createVersionScriptNode();
  ASSERT(VSN, "must not be null!");
  StrToken *VerStrTok = ThisScriptFile.createStrToken(VerStr.str());
  VSN->setName(VerStrTok);
  readVersionSymbols(*VSN);
  if (!consume(";")) {
    VSN->setDependency(ThisScriptFile.createStrToken(next().str()));
    expect(";");
  }
}

void ScriptParser::readVersionSymbols(VersionScriptNode &VSN) {
  while (!consume("}") && !atEOF()) {
    llvm::StringRef Tok = next();
    if (readInclude(Tok))
      continue;
    if (Tok == "extern") {
      readVersionExtern(VSN);
    } else {
      if (Tok == "local:" || (Tok == "local" && consume(":"))) {
        VSN.switchToLocal();
        continue;
      }
      if (Tok == "global:" || (Tok == "global" && consume(":"))) {
        VSN.switchToGlobal();
        continue;
      }
      VSN.addSymbol(ThisScriptFile.createScriptSymbol(readName(Tok)));
    }
    expect(";");
  }
}

void ScriptParser::readVersionExtern(VersionScriptNode &VSN) {
  llvm::StringRef Tok = next();
  bool IsCxx = (Tok == "\"C++\"");
  if (!IsCxx && Tok != "\"C\"")
    setError("Unknown language");
  VSN.setExternLanguage(ThisScriptFile.createStrToken(Tok.str()));
  expect("{");
  while (!consume("}") && !atEOF()) {
    Tok = next();
    WildcardPattern *TokPat = ThisScriptFile.createWildCardPattern(Tok);
    VSN.addSymbol(ThisScriptFile.createScriptSymbol(TokPat));
    if (consume("}"))
      break;
    expect(";");
  }
  VSN.resetExternLanguage();
}

void ScriptParser::readVersionScript() {
  readVersionScriptCommand();
  llvm::StringRef Tok = peek();
  if (Tok.size())
    setError("EOF expected, but got " + Tok);
}

void ScriptParser::parse() {
  switch (ThisScriptFile.getKind()) {
  case ScriptFile::Kind::VersionScript:
    return readVersionScript();
  case ScriptFile::Kind::DynamicList:
    return readDynamicList();
  case ScriptFile::Kind::ExternList:
  case ScriptFile::Kind::DuplicateCodeList:
  case ScriptFile::Kind::NoReuseTrampolineList:
    return readExternList();
  default:
    return readLinkerScript();
  }
}

void ScriptParser::readDynamicList() {
  while (!atEOF() && consume("{")) {
    ThisScriptFile.createDynamicList();
    while (!consume("}") && !atEOF()) {
      llvm::StringRef Tok = next();
      if (readInclude(Tok))
        continue;
      WildcardPattern *TokPat = ThisScriptFile.createWildCardPattern(Tok);
      ThisScriptFile.addSymbolToDynamicList(
          ThisScriptFile.createScriptSymbol(TokPat));
      expect(";");
    }
    consume(";");
  }
  if (!atEOF())
    setError("Unexpected token: " + peek());
}

void ScriptParser::readExternList() {
  while (!atEOF() && consume("{")) {
    ThisScriptFile.createExternCmd();
    while (!consume("}") && !atEOF()) {
      llvm::StringRef Tok = next();
      if (readInclude(Tok))
        continue;
      ThisScriptFile.addSymbolToExternList(
          ThisScriptFile.createScriptSymbol(readName(Tok)));
      expect(";");
    }
    // Optionally consume semi-colon
    consume(";");
  }
  if (!atEOF())
    setError("Unexpected token: " + peek());
}

bool ScriptParser::isValidFilePattern(llvm::StringRef Pat) {
  if (Pat.size() == 1 && strchr("(){}", Pat[0]) != nullptr)
    return false;
  return true;
}

bool ScriptParser::isValidSectionPattern(llvm::StringRef Pat) {
  if (Pat.size() == 1 && strchr("(){}", Pat[0]) != nullptr)
    return false;
  return true;
}

StrToken *ScriptParser::readName(llvm::StringRef Name) {
  return ThisScriptFile.createParserStr(Name);
}