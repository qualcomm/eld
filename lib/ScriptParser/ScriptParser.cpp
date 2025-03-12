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
#include "eld/Target/GNULDBackend.h"
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
    StringRef tok = peek();
    if (atEOF())
      break;

    if (tok == ";") {
      skip();
      continue;
    }

    if (tok == "ENTRY") {
      skip();
      readEntry();
    } else if (tok == "SECTIONS") {
      skip();
      readSections();
    } else if (tok == "INPUT" || tok == "GROUP") {
      bool isInputCmd = (tok == "INPUT");
      skip();
      readInputOrGroup(isInputCmd);
    } else if (tok == "OUTPUT") {
      skip();
      readOutput();
    } else if (tok == "PHDRS") {
      skip();
      readPhdrs();
    } else if (tok == "NOCROSSREFS") {
      skip();
      readNoCrossRefs();
    } else if (tok == "SEARCH_DIR") {
      skip();
      readSearchDir();
    } else if (tok == "OUTPUT_ARCH") {
      skip();
      readOutputArch();
    } else if (tok == "MEMORY") {
      skip();
      readMemory();
    } else if (tok == "EXTERN") {
      skip();
      readExtern();
    } else if (tok == "REGION_ALIAS") {
      skip();
      readRegionAlias();
    } else if (tok == "OUTPUT_FORMAT") {
      skip();
      readOutputFormat();
    } else if (tok == "VERSION") {
      skip();
      readVersion();
    } else if (readInclude()) {
    } else if (readAssignment()) {
    } else if (readPluginDirective()) {
    } else {
      setError("unknown directive: " + tok);
    }
  }
}

bool ScriptParser::readAssignment() {
  llvm::StringRef defTok = peek();
  llvm::StringRef tok = next(LexState::Expr);
  if (llvm::StringRef(defTok.data() + tok.size()).starts_with("/*")) {
    prev();
    return false;
  }
  if (tok == "ASSERT") {
    readAssert();
    // Read optional semi-colon at the end of ASSERT.
    consume(";");
    return true;
  }
  bool ret = false;
  StringRef op = peek(LexState::Expr);
  // TODO: Support '^=' operator.
  if (op.starts_with("=") ||
      (op.size() == 2 && op[1] == '=' && strchr("*/+-&|", op[0])) ||
      op == "<<=" || op == ">>=") {
    ret = readSymbolAssignment(tok);
    if (!ret)
      return false;
  } else if (tok == "PROVIDE" || tok == "HIDDEN" || tok == "PROVIDE_HIDDEN") {
    readProvideHidden(tok);
    ret = true;
  } else
    prev();
  if (ret)
    expectButContinue(";");
  return ret;
}

void ScriptParser::readEntry() {
  expect("(");
  StringRef tok = next();
  StringRef entrySymbol = unquote(tok);
  expect(")");
  auto EntryCmd = m_ScriptFile.addEntryPoint(entrySymbol.str());
  EntryCmd->setLineNumberInContext(prevTokLine);
  if (m_Config.options().shouldTraceLinkerScript())
    EntryCmd->dump(llvm::outs());
}

eld::Expression *ScriptParser::readExpr() {
  // Our lexer is context-aware. Set the in-expression bit so that
  // they apply different tokenization rules.
  LexState orig = lexState;
  lexState = LexState::Expr;
  eld::Expression *e = readExpr1(readPrimary(), /*minPrec=*/0);
  lexState = orig;
  return e;
}

eld::Expression *ScriptParser::readExpr1(eld::Expression *lhs, int minPrec) {
  while (!atEOF() && Diagnose()) {
    // Read an operator and an expression.
    StringRef op1 = peek();
    if (precedence(op1) < minPrec)
      break;
    if (consume("?"))
      return readTernary(lhs);
    skip();
    eld::Expression *rhs = readPrimary();

    // Evaluate the remaining part of the expression first if the
    // next operator has greater precedence than the previous one.
    // For example, if we have read "+" and "3", and if the next
    // operator is "*", then we'll evaluate 3 * ... part first.
    while (!atEOF()) {
      StringRef op2 = peek();
      if (precedence(op2) <= precedence(op1))
        break;
      rhs = readExpr1(rhs, precedence(op2));
    }

    lhs = &combine(op1, *lhs, *rhs);
  }
  return lhs;
}

int ScriptParser::precedence(StringRef op) {
  return StringSwitch<int>(op)
      .Cases("*", "/", "%", 11)
      .Cases("+", "-", 10)
      .Cases("<<", ">>", 9)
      .Cases("<", "<=", ">", ">=", 8)
      .Cases("==", "!=", 7)
      .Case("&", 6)
      .Case("^", 5)
      .Case("|", 4)
      .Case("&&", 3)
      .Case("||", 2)
      .Case("?", 1)
      .Default(-1);
}

eld::Expression &ScriptParser::combine(llvm::StringRef op, eld::Expression &l,
                                       eld::Expression &r) {
  Module &module = m_ScriptFile.module();
  GNULDBackend &backend = m_ScriptFile.backend();
  if (op == "+")
    return *(make<Add>(module, backend, l, r));
  if (op == "-")
    return *(make<Subtract>(module, backend, l, r));
  if (op == "*")
    return *(make<Multiply>(module, backend, l, r));
  if (op == "/") {
    // FIXME: It be useful to pass current location for reporting
    // division by zero error!
    return *(make<Divide>(module, backend, l, r));
  }
  if (op == "%") {
    // FIXME: It be useful to pass current location for reporting
    // modulo by zero error!
    return *(make<Modulo>(module, backend, l, r));
  }
  if (op == "<<")
    return *(make<LeftShift>(module, backend, l, r));
  if (op == ">>")
    return *(make<RightShift>(module, backend, l, r));
  if (op == "<")
    return *(make<ConditionLT>(module, backend, l, r));
  if (op == ">")
    return *(make<ConditionGT>(module, backend, l, r));
  if (op == ">=")
    return *(make<ConditionGTE>(module, backend, l, r));
  if (op == "<=")
    return *(make<ConditionLTE>(module, backend, l, r));
  if (op == "==")
    return *(make<ConditionEQ>(module, backend, l, r));
  if (op == "!=")
    return *(make<ConditionNEQ>(module, backend, l, r));
  if (op == "||")
    return *(make<LogicalOp>(Expression::LOGICAL_OR, module, backend, l, r));
  if (op == "&&")
    return *(make<LogicalOp>(Expression::LOGICAL_AND, module, backend, l, r));
  if (op == "&")
    return *(make<BitwiseAnd>(module, backend, l, r));
  // FIXME: Support xor operator!
  // if (op == "^")
  //   return *(make<op>(module, backend, l, r));
  if (op == "|")
    return *(make<BitwiseOr>(module, backend, l, r));
  llvm_unreachable("invalid operator");
}

eld::Expression *ScriptParser::readPrimary() {
  if (peek() == "(")
    return readParenExpr(/*setParen=*/true);

  Module &module = m_ScriptFile.module();
  GNULDBackend &backend = m_ScriptFile.backend();
  if (consume("~")) {
    Expression *e = readPrimary();
    return make<Complement>(module, backend, *e);
  }
  if (consume("!")) {
    Expression *e = readPrimary();
    return make<UnaryNot>(module, backend, *e);
  }
  if (consume("-")) {
    Expression *e = readPrimary();
    return make<UnaryMinus>(module, backend, *e);
  }
  if (consume("+")) {
    Expression *e = readPrimary();
    return make<UnaryPlus>(module, backend, *e);
  }

  StringRef tok = next();
  std::string location = getCurrentLocation();

  // Built-in functions are parsed here.
  // https://sourceware.org/binutils/docs/ld/Builtin-Functions.html.
  if (tok == "ABSOLUTE") {
    Expression *e = readParenExpr(/*setParen=*/true);
    return make<Absolute>(module, backend, *e);
  }
  if (tok == "ADDR") {
    StringRef name = unquote(readParenLiteral());
    // FIXME: Location might be handly for 'undefined section' error.
    return make<Addr>(module, backend, name.str());
  }
  if (tok == "ALIGN") {
    expect("(");
    Expression *e = readExpr();
    if (consume(")")) {
      // FIXME: Location given here may be overwritten for outermost ALIGN
      // expressions.
      return make<AlignExpr>(module, backend, location, *e,
                             *make<Symbol>(module, backend, "."));
    }
    expect(",");
    Expression *e2 = readExpr();
    expect(")");
    // FIXME: Location given here may be overwritten for outermost ALIGN
    // expressions.
    return make<AlignExpr>(module, backend, location, *e2, *e);
  }
  if (tok == "ALIGNOF") {
    StringRef name = unquote(readParenLiteral());
    // FIXME: Location might be useful for undefined section related errors.
    return make<AlignOf>(module, backend, name.str());
  }
  if (tok == "ASSERT")
    return readAssert();
  if (tok == "CONSTANT")
    return readConstant();
  if (tok == "DATA_SEGMENT_ALIGN") {
    expect("(");
    Expression *e1 = readExpr();
    expect(",");
    Expression *e2 = readExpr();
    expect(")");
    return make<DataSegmentAlign>(module, backend, *e1, *e2);
  }
  if (tok == "DATA_SEGMENT_END") {
    expect("(");
    Expression *e = readExpr();
    expect(")");
    return make<DataSegmentEnd>(module, backend, *e);
  }
  if (tok == "DATA_SEGMENT_RELRO_END") {
    expect("(");
    Expression *e1 = readExpr();
    expect(",");
    Expression *e2 = readExpr();
    expect(")");
    return make<DataSegmentRelRoEnd>(module, backend, *e1, *e2);
  }
  if (tok == "DEFINED") {
    StringRef name = unquote(readParenLiteral());
    return make<Defined>(module, backend, name.str());
  }
  if (tok == "LENGTH") {
    StringRef name = readParenLiteral();
    return make<QueryMemory>(Expression::LENGTH, module, backend, name.str());
  }
  if (tok == "LOADADDR") {
    StringRef name = unquote(readParenLiteral());
    return make<LoadAddr>(module, backend, name.str());
  }
  if (tok == "LOG2CEIL") {
    expect("(");
    Expression *e = readExpr();
    expect(")");
    return make<Log2Ceil>(module, backend, *e);
  }
  if (tok == "MAX" || tok == "MIN") {
    expect("(");
    Expression *e1 = readExpr();
    expect(",");
    Expression *e2 = readExpr();
    expect(")");
    if (tok == "MIN")
      return make<Min>(module, backend, *e1, *e2);
    return make<Max>(module, backend, *e1, *e2);
  }
  if (tok == "ORIGIN") {
    StringRef name = readParenLiteral();
    return make<QueryMemory>(Expression::ORIGIN, module, backend, name.str());
  }
  if (tok == "SEGMENT_START") {
    expect("(");
    StringRef name = unquote(next());
    expect(",");
    Expression *e = readExpr();
    expect(")");
    return make<SegmentStart>(module, backend, name.str(), *e);
  }
  if (tok == "SIZEOF") {
    StringRef name = readParenName();
    return make<SizeOf>(module, backend, name.str());
  }
  if (tok == "SIZEOF_HEADERS")
    return make<SizeOfHeaders>(module, backend, &m_ScriptFile);

  // Tok is a literal number.
  if (std::optional<uint64_t> val = parseInt(tok))
    return make<Integer>(module, backend, tok.str(), val.value());

  // Tok is a symbol name.
  if (tok.starts_with("\""))
    tok = unquote(tok);
  if (!isValidSymbolName(tok))
    setError("malformed number: " + tok);
  return make<Symbol>(module, backend, tok.str());
}

Expression *ScriptParser::readParenExpr(bool setParen) {
  expect("(");
  Expression *e = readExpr();
  expect(")");
  if (setParen)
    e->setParen();
  return e;
}

StringRef ScriptParser::readParenLiteral() {
  expect("(");
  LexState orig = lexState;
  lexState = LexState::Expr;
  StringRef tok = next();
  lexState = orig;
  expect(")");
  return tok;
}

Expression *ScriptParser::readConstant() {
  StringRef s = readParenLiteral();
  Module &module = m_ScriptFile.module();
  GNULDBackend &backend = m_ScriptFile.backend();
  if (s == "COMMONPAGESIZE")
    return make<Constant>(module, backend, "COMMONPAGESIZE",
                          Expression::COMMONPAGESIZE);
  if (s == "MAXPAGESIZE")
    return make<Constant>(module, backend, "MAXPAGESIZE",
                          Expression::MAXPAGESIZE);
  setError("unknown constant: " + s);
  return make<Integer>(module, backend, "", 0);
}

std::optional<uint64_t> ScriptParser::parseInt(StringRef tok) const {
  // Hexadecimal
  uint64_t val = 0;
  if (tok.starts_with_insensitive("0x")) {
    if (!to_integer(tok.substr(2), val, 16))
      return std::nullopt;
    return val;
  }
  if (tok.ends_with_insensitive("H")) {
    if (!to_integer(tok.drop_back(), val, 16))
      return std::nullopt;
    return val;
  }

  // Decimal
  if (tok.ends_with_insensitive("K")) {
    if (!to_integer(tok.drop_back(), val, 10))
      return std::nullopt;
    return val * 1024;
  }
  if (tok.ends_with_insensitive("M")) {
    if (!to_integer(tok.drop_back(), val, 10))
      return std::nullopt;
    return val * 1024 * 1024;
  }
  if (!to_integer(tok, val, 10))
    return std::nullopt;
  return val;
}

bool ScriptParser::isValidSymbolName(StringRef s) {
  auto valid = [](char c) {
    return isAlnum(c) || c == '$' || c == '.' || c == '_';
  };
  return !s.empty() && !isDigit(s[0]) && llvm::all_of(s, valid);
}

bool ScriptParser::readSymbolAssignment(StringRef tok,
                                        Assignment::Type assignType) {
  StringRef name = unquote(tok);
  StringRef op = next(LexState::Expr);
  // TODO: Support '^='
  assert(op == "=" || op == "*=" || op == "/=" || op == "+=" || op == "-=" ||
         op == "&=" || op == "|=" || op == "<<=" || op == ">>=");
  // Note: GNU ld does not support %=.
  Expression *e = readExpr();
  Module &module = m_ScriptFile.module();
  GNULDBackend &backend = m_ScriptFile.backend();
  if (op != "=") {
    Symbol *S = make<Symbol>(module, backend, name.str());
    std::string loc = getCurrentLocation();
    char subOp = op[0];
    switch (subOp) {
    case '*':
      e = make<Multiply>(module, backend, *S, *e);
      break;
    case '/':
      e = make<Divide>(module, backend, *S, *e);
      break;
    case '+':
      e = make<Add>(module, backend, *S, *e);
      break;
    case '-':
      e = make<Subtract>(module, backend, *S, *e);
      break;
    case '<':
      e = make<LeftShift>(module, backend, *S, *e);
      break;
    case '>':
      e = make<RightShift>(module, backend, *S, *e);
      break;
    case '&':
      e = make<BitwiseAnd>(module, backend, *S, *e);
      break;
    case '|':
      e = make<BitwiseOr>(module, backend, *S, *e);
      break;
    default:
      llvm_unreachable("");
    }
    e->setAssign();
  }
  m_ScriptFile.addAssignment(name.str(), e, assignType);
  return true;
}

Expression *ScriptParser::readTernary(Expression *cond) {
  Expression *l = readExpr();
  expect(":");
  Expression *r = readExpr();
  return make<Ternary>(m_ScriptFile.module(), m_ScriptFile.backend(), *cond, *l,
                       *r);
}

void ScriptParser::readProvideHidden(StringRef tok) {
  Assignment::Type assignType;
  if (tok == "PROVIDE")
    assignType = Assignment::Type::PROVIDE;
  else if (tok == "HIDDEN")
    assignType = Assignment::Type::HIDDEN;
  else if (tok == "PROVIDE_HIDDEN")
    assignType = Assignment::Type::PROVIDE_HIDDEN;
  else
    llvm_unreachable("Expected PROVIDE/HIDDEN/PROVIDE_HIDDEN assignments!");
  expect("(");
  tok = next();
  if (peek() != "=") {
    setError("= expected, but got " + next());
    while (!atEOF() && next() != ")")
      ;
  }
  readSymbolAssignment(tok, assignType);
  expect(")");
}

void ScriptParser::readSections() {
  expect("{");
  m_ScriptFile.enterSectionsCmd();
  while (peek() != "}" && !atEOF()) {
    if (readInclude()) {
    } else if (readAssignment()) {
    } else {
      llvm::StringRef sectName = unquote(next(LexState::SectionName));
      readOutputSectionDescription(sectName);
    }
  }
  expect("}");
  m_ScriptFile.leaveSectionsCmd();
}

Expression *ScriptParser::readAssert() {
  expect("(");
  Expression *e = readExpr();
  expect(",");
  StringRef msg = unquote(next());
  expect(")");
  Expression *assertCmd = make<eld::AssertCmd>(
      m_ScriptFile.module(), m_ScriptFile.backend(), msg.str(), *e);
  m_ScriptFile.addAssignment("ASSERT", assertCmd, Assignment::ASSERT);
  return assertCmd;
}

void ScriptParser::readInputOrGroup(bool isInputCmd) {
  expect("(");
  m_ScriptFile.createStringList();
  while (peek() != ")" && !atEOF()) {
    if (consume("AS_NEEDED")) {
      readAsNeeded();
    } else
      addFile(unquote(next()));
    consume(",");
  }
  expect(")");
  StringList *inputs = m_ScriptFile.getCurrentStringList();
  if (isInputCmd)
    m_ScriptFile.addInputCmd(
        *inputs, m_ScriptFile.getLinkerScriptFile().getInput()->getAttribute());
  else
    m_ScriptFile.addGroupCmd(
        *inputs, m_ScriptFile.getLinkerScriptFile().getInput()->getAttribute());
}

void ScriptParser::readAsNeeded() {
  expect("(");
  m_ScriptFile.setAsNeeded(true);
  while (peek() != ")" && !atEOF()) {
    addFile(unquote(next()));
    consume(",");
  }
  expect(")");
  m_ScriptFile.setAsNeeded(false);
}

void ScriptParser::addFile(StringRef name) {
  StrToken *inputStrTok = nullptr;
  if (name.consume_front("-l"))
    inputStrTok =
        m_ScriptFile.createNameSpecToken(name.str(), m_ScriptFile.asNeeded());
  else
    inputStrTok =
        m_ScriptFile.createFileToken(name.str(), m_ScriptFile.asNeeded());
  m_ScriptFile.getCurrentStringList()->push_back(inputStrTok);
}

void ScriptParser::readOutput() {
  expect("(");
  StringRef name = next();
  m_ScriptFile.addOutputCmd(unquote(name).str());
  expect(")");
}

void ScriptParser::readOutputSectionDescription(llvm::StringRef outSectName) {
  OutputSectDesc::Prolog prologue = readOutputSectDescPrologue();
  m_ScriptFile.enterOutputSectDesc(outSectName.str(), prologue);
  expect("{");
  while (peek() != "}" && !atEOF()) {
    StringRef tok = peek();
    if (tok == ";") {
      // Empty commands are allowed. Do nothing.
      skip();
    } else if (tok == "FILL") {
      skip();
      readFill();
    } else if (readInclude()) {
    } else if (readOutputSectionData()) {
    } else if (readAssignment()) {
    } else {
      tok = next();
      readInputSectionDescription(tok);
    }
  }
  expect("}");
  OutputSectDesc::Epilog epilogue = readOutputSectDescEpilogue();
  m_ScriptFile.leavingOutputSectDesc();
  m_ScriptFile.leaveOutputSectDesc(epilogue);
}

void ScriptParser::readInputSectionDescription(StringRef tok) {
  InputSectDesc::Policy policy = InputSectDesc::Policy::NoKeep;
  if (tok == "KEEP")
    policy = InputSectDesc::Policy::Keep;
  else if (tok == "DONTMOVE")
    policy = InputSectDesc::Policy::Fixed;
  else if (tok == "KEEP_DONTMOVE")
    policy = InputSectDesc::Policy::KeepFixed;
  if (policy != InputSectDesc::Policy::NoKeep) {
    expect("(");
    tok = next();
  }
  InputSectDesc::Spec ISDSpec = readInputSectionDescSpec(tok);
  if (policy != InputSectDesc::Policy::NoKeep)
    expect(")");
  m_ScriptFile.addInputSectDesc(policy, ISDSpec);
}

InputSectDesc::Spec ScriptParser::readInputSectionDescSpec(StringRef tok) {
  ExcludeFiles *EF = nullptr;
  if (tok == "EXCLUDE_FILE") {
    EF = readExcludeFile();
    tok = next();
  }
  WildcardPattern *filePat = nullptr, *archiveMem = nullptr;
  bool isArchive = false;
  if (!tok.contains(':'))
    filePat = m_ScriptFile.createWildCardPattern(tok);
  else {
    std::pair<llvm::StringRef, llvm::StringRef> split = tok.split(':');
    filePat = m_ScriptFile.createWildCardPattern(split.first);
    if (!split.second.empty())
      archiveMem = m_ScriptFile.createWildCardPattern(split.second);
    isArchive = true;
  }
  StringList *wildcardSections = nullptr;
  if (consume("(")) {
    m_ScriptFile.createStringList();
    while (peek() != ")" && !atEOF()) {
      WildcardPattern *sectPat = readWildcardPattern();
      m_ScriptFile.getCurrentStringList()->push_back(sectPat);
    }
    expect(")");
    wildcardSections = m_ScriptFile.getCurrentStringList();
  }
  InputSectDesc::Spec ISDSpec;
  ISDSpec.initialize();
  ISDSpec.m_pWildcardFile = filePat;
  ISDSpec.m_pWildcardSections = wildcardSections;
  ISDSpec.m_pArchiveMember = archiveMem;
  ISDSpec.m_pIsArchive = isArchive;
  ISDSpec.m_ExcludeFiles = EF;
  return ISDSpec;
}

OutputSectDesc::Prolog ScriptParser::readOutputSectDescPrologue() {
  OutputSectDesc::Prolog prologue;
  prologue.init();

  if (peek() != ":") {
    if (consume("(")) {
      if (!readOutputSectTypeAndPermissions(prologue, peek()))
        prologue.m_pVMA = readExpr();
      expect(")");
    } else if (peek() == "{") {
      expect(":");
    } else {
      prologue.m_pPluginCmd = readOutputSectionPluginDirective();
      if (!prologue.m_pPluginCmd)
        prologue.m_pVMA = readExpr();
    }

    if (prologue.m_pVMA != nullptr && consume("(")) {
      StringRef tok = peek();
      if (!readOutputSectTypeAndPermissions(prologue, tok))
        setError("Invalid output section type: " + tok);
      expect(")");
    }

    if (!prologue.m_pPluginCmd)
      prologue.m_pPluginCmd = readOutputSectionPluginDirective();
  }

  expect(":");

  if (consume("AT"))
    prologue.m_pLMA = readParenExpr(/*setParen=*/false);
  if (consume("ALIGN"))
    prologue.m_pAlign = readParenExpr(/*setParen=*/false);
  if (consume("SUBALIGN"))
    prologue.m_pSubAlign = readParenExpr(/*setParen=*/false);

  if (consume("ONLY_IF_RO"))
    prologue.m_Constraint = OutputSectDesc::Constraint::ONLY_IF_RO;
  else if (consume("ONLY_IF_RW"))
    prologue.m_Constraint = OutputSectDesc::Constraint::ONLY_IF_RW;
  return prologue;
}

bool ScriptParser::readOutputSectTypeAndPermissions(
    OutputSectDesc::Prolog &prologue, llvm::StringRef tok) {
  std::optional<OutputSectDesc::Type> expType = readOutputSectType(tok);
  if (expType)
    prologue.m_Type = expType.value();
  else
    return false;
  next();
  if (consume(",")) {
    tok = next();
    std::optional<uint32_t> expFlag = readOutputSectPermissions(tok);
    if (expFlag)
      prologue.m_Flag = expFlag.value();
    else
      setError("Invalid permission flag: " + tok);
  }
  return true;
}

std::optional<OutputSectDesc::Type>
ScriptParser::readOutputSectType(StringRef tok) {
  return StringSwitch<std::optional<OutputSectDesc::Type>>(tok)
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
ScriptParser::readOutputSectPermissions(llvm::StringRef tok) {
  if (std::optional<uint64_t> permissions = parseInt(tok))
    return permissions;
  return StringSwitch<std::optional<uint32_t>>(tok)
      .Case("RW", OutputSectDesc::Permissions::RW)
      .Case("RWX", OutputSectDesc::Permissions::RWX)
      .Case("RX", OutputSectDesc::Permissions::RX)
      .Case("R", OutputSectDesc::Permissions::R)
      .Default(std::nullopt);
}

void ScriptParser::readPhdrs() {
  expect("{");
  m_ScriptFile.enterPhdrsCmd();
  while (peek() != "}" && !atEOF()) {
    PhdrSpec phdrSpec;
    phdrSpec.init();
    llvm::StringRef nameTok = next();
    phdrSpec.m_Name =
        m_ScriptFile.createParserStr(nameTok.data(), nameTok.size());
    llvm::StringRef typeTok = next();
    auto optPhdrType = readPhdrType(typeTok);
    if (optPhdrType.has_value())
      phdrSpec.m_Type = optPhdrType.value();
    else
      setError("invalid program header type: " + typeTok);
    while (peek() != ";" && !atEOF()) {
      if (consume("FILEHDR"))
        phdrSpec.m_HasFileHdr = true;
      else if (consume("PHDRS"))
        phdrSpec.m_HasPhdr = true;
      else if (consume("AT"))
        phdrSpec.m_AtAddress = readParenExpr(/*setParen=*/false);
      else if (consume("FLAGS"))
        phdrSpec.m_Flags = readParenExpr(/*setParen=*/false);
      else
        setError("unexpected header attribute: " + next());
    }
    expect(";");
    if (Diagnose())
      m_ScriptFile.addPhdrDesc(phdrSpec);
  }
  expect("}");
  m_ScriptFile.leavePhdrsCmd();
}

std::optional<uint32_t> ScriptParser::readPhdrType(llvm::StringRef tok) const {
  if (std::optional<uint64_t> val = parseInt(tok))
    return *val;

  std::optional<uint32_t> ret =
      llvm::StringSwitch<std::optional<uint32_t>>(tok)
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
  return ret;
}

OutputSectDesc::Epilog ScriptParser::readOutputSectDescEpilogue() {
  m_InOutputSectEpilogue = true;
  OutputSectDesc::Epilog epilogue;
  epilogue.init();

  if (consume(">")) {
    llvm::StringRef region = unquote(next());
    epilogue.m_pRegion =
        m_ScriptFile.createParserStr(region.data(), region.size());
  }

  if (consume("AT")) {
    expect(">");
    llvm::StringRef region = unquote(next());
    epilogue.m_pLMARegion =
        m_ScriptFile.createParserStr(region.data(), region.size());
  }

  m_ScriptFile.createStringList();
  while (Diagnose() && peek().starts_with(":")) {
    llvm::StringRef tok = next(LexState::Expr);
    llvm::StringRef phdrName =
        (tok.size() == 1 ? unquote(next(LexState::Expr)) : tok.substr(1));
    m_ScriptFile.getCurrentStringList()->push_back(
        m_ScriptFile.createParserStr(phdrName.data(), phdrName.size()));
  }
  epilogue.m_pPhdrs = m_ScriptFile.getCurrentStringList();

  if (peek() == "=" || peek().starts_with("=")) {
    lexState = LexState::Expr;
    consume("=");
    epilogue.m_pFillExp = readExpr();
    lexState = LexState::Default;
  }
  // Consume optional comma following output section command.
  consume(",");
  m_InOutputSectEpilogue = false;
  return epilogue;
}

void ScriptParser::readNoCrossRefs() {
  expect("(");
  m_ScriptFile.createStringList();
  while (peek() != ")" && !atEOF()) {
    llvm::StringRef sectionName = unquote(next());
    m_ScriptFile.getCurrentStringList()->push_back(
        m_ScriptFile.createScriptSymbol(sectionName));
  }
  expect(")");
  StringList *SL = m_ScriptFile.getCurrentStringList();
  m_ScriptFile.addNoCrossRefs(*SL);
}

bool ScriptParser::readPluginDirective() {
  llvm::StringRef tok = peek();
  std::optional<plugin::Plugin::Type> optPluginType =
      llvm::StringSwitch<std::optional<plugin::Plugin::Type>>(tok)
          .Case("PLUGIN_SECTION_MATCHER", plugin::Plugin::Type::SectionMatcher)
          .Case("PLUGIN_ITER_SECTIONS", plugin::Plugin::Type::SectionIterator)
          .Case("PLUGIN_OUTPUT_SECTION_ITER",
                plugin::Plugin::Type::OutputSectionIterator)
          .Case("LINKER_PLUGIN", plugin::Plugin::Type::LinkerPlugin)
          .Default(std::nullopt);
  if (!optPluginType.has_value())
    return false;
  skip();
  expect("(");
  llvm::StringRef libName = unquote(next());
  expect(",");
  llvm::StringRef pluginName = unquote(next());
  llvm::StringRef configName;
  if (consume(","))
    configName = unquote(next());
  expect(")");
  m_ScriptFile.addPlugin(optPluginType.value(), libName.str(), pluginName.str(),
                         configName.str());
  return true;
}

void ScriptParser::readSearchDir() {
  expect("(");
  llvm::StringRef searchDir = unquote(next());
  m_ScriptFile.addSearchDirCmd(searchDir.str());
  expect(")");
}

void ScriptParser::readOutputArch() {
  expect("(");
  llvm::StringRef arch = unquote(next());
  m_ScriptFile.addOutputArchCmd(arch.str());
  expect(")");
}

void ScriptParser::readMemory() {
  expect("{");
  while (peek() != "}" && !atEOF()) {
    if (readInclude())
      continue;
    llvm::StringRef tok = next();
    llvm::StringRef name = tok;
    StrToken *memoryAttrs = nullptr;
    if (consume("(")) {
      memoryAttrs = readMemoryAttributes();
      expect(")");
    }
    expect(":");
    Expression *origin = readMemoryAssignment({"ORIGIN", "org", "o"});
    expect(",");
    Expression *length = readMemoryAssignment({"LENGTH", "len", "l"});
    if (origin && length) {
      StrToken *nameToken =
          m_ScriptFile.createParserStr(name.data(), name.size());
      m_ScriptFile.addMemoryRegion(nameToken, memoryAttrs, origin, length);
    }
  }
  expect("}");
}

Expression *
ScriptParser::readMemoryAssignment(std::vector<llvm::StringRef> names) {
  bool expectedToken = false;
  llvm::StringRef tok = next();
  for (llvm::StringRef name : names) {
    if (tok == name) {
      expectedToken = true;
      break;
    }
  }
  if (!expectedToken) {
    std::string namesJoined;
    for (auto iter = names.begin(); iter != names.end(); ++iter) {
      llvm::StringRef name = *iter;
      if (iter != names.end() - 1)
        namesJoined += name.str() + ", ";
      else
        namesJoined += "or " + name.str();
    }
    setError("expected one of: " + namesJoined);
    return nullptr;
  }
  expect("=");
  return readExpr();
}

StrToken *ScriptParser::readMemoryAttributes() {
  llvm::StringRef attrs = next();
  llvm::StringRef::size_type sz = attrs.find_first_not_of("rRwWxXaA!iIlL");
  if (sz != llvm::StringRef::npos) {
    setError("invalid memory region attribute");
    return nullptr;
  }
  return m_ScriptFile.createStrToken("(" + attrs.str() + ")");
}

void ScriptParser::readExtern() {
  expect("(");
  m_ScriptFile.createStringList();
  while (peek() != ")" && !atEOF()) {
    llvm::StringRef sym = unquote(next());
    ScriptSymbol *scriptSym = m_ScriptFile.createScriptSymbol(sym);
    m_ScriptFile.getCurrentStringList()->push_back(scriptSym);
  }
  expect(")");
  StringList *symbolList = m_ScriptFile.getCurrentStringList();
  m_ScriptFile.addExtern(*symbolList);
}

void ScriptParser::readRegionAlias() {
  expect("(");
  llvm::StringRef alias = unquote(next());
  expect(",");
  llvm::StringRef region = unquote(next());
  expect(")");

  StrToken *aliasToken = m_ScriptFile.createStrToken(alias.str());
  StrToken *regionToken = m_ScriptFile.createStrToken(region.str());
  m_ScriptFile.addRegionAlias(aliasToken, regionToken);
}

bool ScriptParser::readOutputSectionData() {
  llvm::StringRef tok = peek();
  std::optional<OutputSectData::OSDKind> optDataKind =
      llvm::StringSwitch<std::optional<OutputSectData::OSDKind>>(tok)
          .Case("BYTE", OutputSectData::OSDKind::Byte)
          .Case("SHORT", OutputSectData::OSDKind::Short)
          .Case("LONG", OutputSectData::OSDKind::Long)
          .Case("QUAD", OutputSectData::OSDKind::Quad)
          .Case("SQUAD", OutputSectData::OSDKind::Squad)
          .Default(std::nullopt);
  if (!optDataKind.has_value())
    return false;
  skip();
  expect("(");
  Expression *exp = readExpr();
  expect(")");
  m_ScriptFile.addOutputSectData(optDataKind.value(), exp);
  return true;
}

std::optional<WildcardPattern::SortPolicy> ScriptParser::readSortPolicy() {
  llvm::StringRef tok = peek();
  std::optional<WildcardPattern::SortPolicy> optSortPolicy =
      llvm::StringSwitch<std::optional<WildcardPattern::SortPolicy>>(tok)
          .Case("SORT_NONE", WildcardPattern::SortPolicy::SORT_NONE)
          .Cases("SORT", "SORT_BY_NAME",
                 WildcardPattern::SortPolicy::SORT_BY_NAME)
          .Case("SORT_BY_ALIGNMENT",
                WildcardPattern::SortPolicy::SORT_BY_ALIGNMENT)
          .Case("SORT_BY_INIT_PRIORITY",
                WildcardPattern::SortPolicy::SORT_BY_INIT_PRIORITY)
          .Default(std::nullopt);
  if (optSortPolicy.has_value())
    skip();
  return optSortPolicy;
}

WildcardPattern::SortPolicy ScriptParser::computeSortPolicy(
    WildcardPattern::SortPolicy outerSortPolicy,
    std::optional<WildcardPattern::SortPolicy> innerSortPolicy) {
  if (!innerSortPolicy.has_value())
    return outerSortPolicy;
  if (outerSortPolicy == innerSortPolicy.value())
    return outerSortPolicy;
  if (outerSortPolicy == WildcardPattern::SortPolicy::SORT_BY_NAME &&
      innerSortPolicy.value() == WildcardPattern::SortPolicy::SORT_BY_ALIGNMENT)
    return WildcardPattern::SortPolicy::SORT_BY_NAME_ALIGNMENT;
  if (outerSortPolicy == WildcardPattern::SortPolicy::SORT_BY_ALIGNMENT &&
      innerSortPolicy.value() == WildcardPattern::SortPolicy::SORT_BY_NAME)
    return WildcardPattern::SortPolicy::SORT_BY_ALIGNMENT_NAME;
  setError("Invalid sort policy combination");
  return WildcardPattern::SortPolicy::SORT_NONE;
}

WildcardPattern *ScriptParser::readWildcardPattern() {
  WildcardPattern *sectPat = nullptr;
  if (peek() == "EXCLUDE_FILE") {
    skip();
    ExcludeFiles *EF = readExcludeFile();
    sectPat = m_ScriptFile.createWildCardPattern(
        m_ScriptFile.createParserStr(next()),
        WildcardPattern::SortPolicy::EXCLUDE, EF);
    return sectPat;
  }
  std::optional<WildcardPattern::SortPolicy> outerSortPolicy = readSortPolicy();
  if (outerSortPolicy.has_value()) {
    expect("(");
    std::optional<WildcardPattern::SortPolicy> innerSortPolicy =
        readSortPolicy();
    if (innerSortPolicy.has_value()) {
      WildcardPattern::SortPolicy sortPolicy =
          computeSortPolicy(outerSortPolicy.value(), innerSortPolicy);
      expect("(");
      sectPat = m_ScriptFile.createWildCardPattern(
          m_ScriptFile.createParserStr(next()), sortPolicy);
      expect(")");
    } else {
      WildcardPattern::SortPolicy sortPolicy =
          computeSortPolicy(outerSortPolicy.value());
      sectPat = m_ScriptFile.createWildCardPattern(
          m_ScriptFile.createParserStr(next()), sortPolicy);
    }
    expect(")");
  } else {
    sectPat = m_ScriptFile.createWildCardPattern(
        m_ScriptFile.createParserStr(next()));
  }
  return sectPat;
}

PluginCmd *ScriptParser::readOutputSectionPluginDirective() {
  llvm::StringRef tok = peek();
  std::optional<plugin::Plugin::Type> optPluginType =
      llvm::StringSwitch<std::optional<plugin::Plugin::Type>>(tok)
          .Case("PLUGIN_CONTROL_FILESZ", plugin::Plugin::Type::ControlFileSize)
          .Case("PLUGIN_CONTROL_MEMSZ", plugin::Plugin::Type::ControlMemorySize)
          .Default(std::nullopt);
  if (!optPluginType.has_value())
    return nullptr;
  skip();
  expect("(");
  llvm::StringRef libName = unquote(next());
  expect(",");
  llvm::StringRef pluginName = unquote(next());
  llvm::StringRef configName;
  if (consume(","))
    configName = unquote(next());
  expect(")");
  return m_ScriptFile.addPlugin(optPluginType.value(), libName.str(),
                                pluginName.str(), configName.str());
}

void ScriptParser::readFill() {
  expect("(");
  Expression *E = readExpr();
  Fill *fill = make<Fill>(m_ScriptFile.module(), m_ScriptFile.backend(), *E);
  m_ScriptFile.addAssignment("FILL", fill, Assignment::FILL);
  expect(")");
}

ExcludeFiles *ScriptParser::readExcludeFile() {
  expect("(");
  m_ScriptFile.createExcludeFiles();
  ExcludeFiles *currentExcludeFiles = m_ScriptFile.getCurrentExcludeFiles();
  while (Diagnose() && !consume(")")) {
    llvm::StringRef excludePat = next();
    StrToken *excludePatTok = m_ScriptFile.createStrToken(excludePat.str());
    ExcludePattern *P = m_ScriptFile.createExcludePattern(excludePatTok);
    currentExcludeFiles->push_back(P);
  }
  return currentExcludeFiles;
}

bool ScriptParser::readInclude() {
  llvm::StringRef tok = peek();
  if (tok != "INCLUDE" && tok != "INCLUDE_OPTIONAL")
    return false;
  skip();
  bool isOptionalInclude = tok == "INCLUDE_OPTIONAL";
  llvm::StringRef fileName = unquote(next());
  LayoutPrinter *LP = m_ScriptFile.module().getLayoutPrinter();
  bool result = true;
  std::string resolvedFileName = m_ScriptFile.findIncludeFile(
      fileName.str(), result, /*state=*/!isOptionalInclude);
  if (LP)
    LP->recordLinkerScript(resolvedFileName, result);
  if (!result && isOptionalInclude)
    return true;
  m_ScriptFile.module().getScript().addToHash(resolvedFileName);
  if (result) {
    Input *input =
        make<Input>(resolvedFileName, m_Config.getDiagEngine(), Input::Script);
    if (!input->resolvePath(m_Config)) {
      setError("Cannot resolve path " + resolvedFileName);
      // return value by ScriptParser functions indicates that the command was
      // parsed.
      return true;
    }
    InputFile *IF = InputFile::Create(input, InputFile::GNULinkerScriptKind,
                                      m_Config.getDiagEngine());
    input->setInputFile(IF);
    m_ScriptFile.setContext(IF);
    llvm::MemoryBufferRef memRef = input->getMemoryBufferRef();
    buffers.push_back(curBuf);
    curBuf = Buffer(memRef);
    mbs.push_back(memRef);
    if (!activeFilenames.insert(curBuf.filename).second)
      setError("there is a cycle in linker script INCLUDEs");
  }
  return true;
}

llvm::StringRef ScriptParser::readParenName() {
  expect("(");
  llvm::SaveAndRestore saveLexState(lexState, LexState::Default);
  StringRef tok = unquote(next());
  expect(")");
  return tok;
}

void ScriptParser::readOutputFormat() {
  expect("(");
  llvm::StringRef defaultFormat = unquote(next());
  if (consume(",")) {
    llvm::StringRef bigFormat = unquote(next());
    expect(",");
    llvm::StringRef littleFormat = unquote(next());
    m_ScriptFile.addOutputFormatCmd(defaultFormat.str(), bigFormat.str(),
                                    littleFormat.str());
  } else {
    m_ScriptFile.addOutputFormatCmd(defaultFormat.str());
  }
  expect(")");
}

void ScriptParser::readVersion() {
  expect("{");
  readVersionScriptCommand();
  expect("}");
}

void ScriptParser::readVersionScriptCommand() {
  m_ScriptFile.createVersionScript();
  if (consume("{")) {
    VersionScriptNode *VSN =
        m_ScriptFile.getVersionScript()->createVersionScriptNode();
    readVersionSymbols(*VSN);
    expect(";");
    return;
  }
  while (peek() != "}" && !atEOF()) {
    llvm::StringRef verStr = next();
    if (verStr == "{") {
      setError("anonymous version definition is used in "
               "combination with other version definitions");
      return;
    }
    expect("{");
    readVersionDeclaration(verStr);
  }
}

void ScriptParser::readVersionDeclaration(llvm::StringRef verStr) {
  VersionScriptNode *VSN =
      m_ScriptFile.getVersionScript()->createVersionScriptNode();
  ASSERT(VSN, "must not be null!");
  StrToken *verStrTok = m_ScriptFile.createStrToken(verStr.str());
  VSN->setName(verStrTok);
  readVersionSymbols(*VSN);
  if (!consume(";")) {
    VSN->setDependency(m_ScriptFile.createStrToken(next().str()));
    expect(";");
  }
}

void ScriptParser::readVersionSymbols(VersionScriptNode &VSN) {
  while (!consume("}") && !atEOF()) {
    if (readInclude())
      continue;
    llvm::StringRef tok = next();
    if (tok == "extern") {
      readVersionExtern(VSN);
    } else {
      if (tok == "local:" || (tok == "local" && consume(":"))) {
        VSN.switchToLocal();
        continue;
      }
      if (tok == "global:" || (tok == "global" && consume(":"))) {
        VSN.switchToGlobal();
        continue;
      }
      VSN.addSymbol(m_ScriptFile.createScriptSymbol(tok.str()));
    }
    expect(";");
  }
}

void ScriptParser::readVersionExtern(VersionScriptNode &VSN) {
  llvm::StringRef tok = next();
  bool isCXX = (tok == "\"C++\"");
  if (!isCXX && tok != "\"C\"")
    setError("Unknown language");
  VSN.setExternLanguage(m_ScriptFile.createStrToken(tok.str()));
  expect("{");
  while (!consume("}") && !atEOF()) {
    tok = next();
    WildcardPattern *tokPat = m_ScriptFile.createWildCardPattern(tok);
    VSN.addSymbol(m_ScriptFile.createScriptSymbol(tokPat));
    if (consume("}"))
      break;
    expect(";");
  }
  VSN.resetExternLanguage();
}

void ScriptParser::readVersionScript() {
  readVersionScriptCommand();
  llvm::StringRef tok = peek();
  if (tok.size())
    setError("EOF expected, but got " + tok);
}

void ScriptParser::parse() {
  switch (m_ScriptFile.getKind()) {
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
    m_ScriptFile.createDynamicList();
    while (!consume("}") && !atEOF()) {
      if (readInclude())
        continue;
      llvm::StringRef tok = next();
      WildcardPattern *tokPat = m_ScriptFile.createWildCardPattern(tok);
      m_ScriptFile.addSymbolToDynamicList(
          m_ScriptFile.createScriptSymbol(tokPat));
      expect(";");
    }
    consume(";");
  }
  if (!atEOF())
    setError("Unexpected token: " + peek());
}

void ScriptParser::readExternList() {
  while (!atEOF() && consume("{")) {
    m_ScriptFile.createExternCmd();
    while (!consume("}") && !atEOF()) {
      if (readInclude())
        continue;
      llvm::StringRef tok = next();
      m_ScriptFile.addSymbolToExternList(m_ScriptFile.createScriptSymbol(tok));
      expect(";");
    }
    // Optionally consume semi-colon
    consume(";");
  }
  if (!atEOF())
    setError("Unexpected token: " + peek());
}
