//===- ScriptLexer.cpp-----------------------------------------------------===//
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
// This file defines a lexer for the linker script.
//
// The linker script's grammar is not complex but ambiguous due to the
// lack of the formal specification of the language. What we are trying to
// do in this and other files in LLD is to make a "reasonable" linker
// script processor.
//
// Among simplicity, compatibility and efficiency, we put the most
// emphasis on simplicity when we wrote this lexer. Compatibility with the
// GNU linkers is important, but we did not try to clone every tiny corner
// case of their lexers, as even ld.bfd and ld.gold are subtly different
// in various corner cases. We do not care much about efficiency because
// the time spent in parsing linker scripts is usually negligible.
//
// Our grammar of the linker script is LL(2), meaning that it needs at
// most two-token lookahead to parse. The only place we need two-token
// lookahead is labels in version scripts, where we need to parse "local :"
// as if "local:".
//
// Overall, this lexer works fine for most linker scripts. There might
// be room for improving compatibility, but that's probably not at the
// top of our todo list.
//
//===----------------------------------------------------------------------===//

#include "eld/ScriptParser/ScriptLexer.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Core/Module.h"
#include "eld/Script/ScriptFile.h"
#include "eld/Support/MsgHandling.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MemoryBufferRef.h"
#include "llvm/Support/SaveAndRestore.h"
#include <algorithm>

using namespace llvm;
using namespace eld;
using namespace eld::v2;

// Returns a whole line containing the current token.
StringRef ScriptLexer::getLine() const {
  StringRef S = getCurrentMB().getBuffer();

  size_t Pos = S.rfind('\n', PrevTok.data() - S.data());
  if (Pos != StringRef::npos)
    S = S.substr(Pos + 1);
  return S.substr(0, S.find_first_of("\r\n"));
}

// Returns 0-based column number of the current token.
size_t ScriptLexer::getColumnNumber() const {
  return computeColumnWidth(getLine().data(), PrevTok.data());
}

std::string ScriptLexer::getCurrentLocation() const {
  std::string Filename = std::string(getCurrentMB().getBufferIdentifier());
  return (Filename + ":" + Twine(PrevTokLine)).str();
}

ScriptLexer::ScriptLexer(eld::LinkerConfig &Config, ScriptFile &ScriptFile)
    : ThisConfig(Config), ThisScriptFile(ScriptFile) {
  ActiveFilenames.insert(ThisScriptFile.getPath());
  InputFile *IF = ThisScriptFile.getContext();
  llvm::MemoryBufferRef MemBufRef = IF->getInput()->getMemoryBufferRef();
  CurBuf = Buffer(MemBufRef);
  MemoryBuffers.push_back(MemBufRef);
}

bool ScriptLexer::diagnose() const {
  DiagnosticPrinter *DP = ThisConfig.getPrinter();
  if (DP->getNumFatalErrors() == 0 && DP->getNumErrors() == MNonFatalErrors) {
    return true;
  }
  return ThisConfig.getDiagEngine()->diagnose();
}

// We don't want to record cascading errors. Keep only the first one.
void ScriptLexer::setError(const Twine &Msg) {
  if (!diagnose())
    return;

  std::string S = (getCurrentLocation() + ": " + Msg).str();
  if (PrevTok.size())
    S += "\n>>> " + getLine().str() + "\n>>> " +
         std::string(getColumnNumber(), ' ') + "^";
  ThisConfig.raise(Diag::error_linker_script) << S;
}

void ScriptLexer::setNote(const Twine &msg,
                          std::optional<llvm::StringRef> columnTok) const {
  std::string s = (getCurrentLocation() + ": " + msg).str();
  if (PrevTok.size()) {
    std::size_t columnNumber = 0;
    if (columnTok)
      columnNumber = computeColumnWidth(getLine().data(), columnTok->data());
    else
      columnNumber = getColumnNumber();
    s += "\n>>> " + getLine().str() + "\n>>> " +
         std::string(columnNumber, ' ') + "^";
  }
  ThisConfig.raise(Diag::note_linker_script) << s;
}

void ScriptLexer::setWarn(const Twine &Msg) {
  std::string S = (getCurrentLocation() + ": " + Msg).str();
  if (PrevTok.size())
    S += "\n>>> " + getLine().str() + "\n>>> " +
         std::string(getColumnNumber(), ' ') + "^";
  ThisConfig.raise(Diag::warn_linker_script) << S;
}

void ScriptLexer::lex() {
  for (;;) {
    StringRef &S = CurBuf.S;
    S = skipSpace(S);
    if (S.empty()) {
      // If this buffer is from an INCLUDE command, switch to the "return
      // value"; otherwise, mark EOF.
      if (Buffers.empty()) {
        Eof = true;
        return;
      }
      ActiveFilenames.erase(CurBuf.Filename);
      ThisScriptFile.popScriptStack();
      CurBuf = Buffers.pop_back_val();
      LayoutInfo *layoutInfo = ThisScriptFile.module().getLayoutInfo();
      if (layoutInfo)
        layoutInfo->closeLinkerScript();
      continue;
    }
    CurTokLexState = LexState;

    // Quoted token. Note that double-quote characters are parts of a token
    // because, in a glob match context, only unquoted tokens are interpreted
    // as glob patterns. Double-quoted tokens are literal patterns in that
    // context.
    if (S.starts_with("\"")) {
      size_t E = S.find("\"", 1);
      if (E == StringRef::npos) {
        size_t Lineno = computeLineNumber(S);
        ThisConfig.raise(Diag::error_linker_script)
            << llvm::Twine(CurBuf.Filename + ":" + Twine(Lineno) +
                           ": unclosed quote")
                   .str();
        return;
      }

      CurTok = S.take_front(E + 1);
      S = S.substr(E + 1);
      return;
    }

    // Some operators form separate tokens.
    if (S.starts_with("<<=") || S.starts_with(">>=")) {
      CurTok = S.substr(0, 3);
      S = S.substr(3);
      return;
    }

    if (S.size() > 1 && (S[1] == '=' && strchr("+-*/!&^|", S[0]))) {
      CurTok = S.substr(0, 2);
      S = S.substr(2);
      return;
    }

    // Unquoted token. The non-expression token is more relaxed than tokens in
    // C-like languages, so that you can write "file-name.cpp" as one bare
    // token.
    size_t Pos;
    if (CurTokLexState == LexState::Expr) {
      Pos = S.find_first_not_of(
          "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
          "0123456789_.$");
      if (Pos == 0 && S.size() >= 2 &&
          ((S[0] == S[1] && strchr("<>&|", S[0])) ||
           is_contained({"==", "!=", "<=", ">=", "<<", ">>"}, S.substr(0, 2))))
        Pos = 2;
      if (S.starts_with("/DISCARD/"))
        Pos = llvm::StringRef("/DISCARD/").size();
    } else {
      // Last char must be ':'!!!
      llvm::StringRef TokenChars =
          "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
          "0123456789_.$/\\~=+[]*?-!^:";
      // Drop ':' if we are lexing output section name
      if (CurTokLexState == LexState::SectionName)
        TokenChars = TokenChars.drop_back();

      Pos = S.find_first_not_of(TokenChars);
    }

    if (Pos == 0)
      Pos = 1;
    CurTok = S.substr(0, Pos);
    S = S.substr(Pos);
    break;
  }
}

// Skip leading whitespace characters or comments.
StringRef ScriptLexer::skipSpace(StringRef S) {
  for (;;) {
    if (S.starts_with("/*")) {
      size_t E = S.find("*/", 2);
      if (E == StringRef::npos) {
        setError("unclosed comment in a linker script");
        return "";
      }
      CurBuf.LineNumber += S.substr(0, E).count('\n');
      S = S.substr(E + 2);
      continue;
    }
    if (S.starts_with("#") || S.starts_with("//")) {
      size_t E = S.find('\n', 1);
      if (E == StringRef::npos)
        E = S.size() - 1;
      else
        ++CurBuf.LineNumber;
      S = S.substr(E + 1);
      continue;
    }
    StringRef Saved = S;
    S = noteAndSkipNonASCIIUnicodeChars(S);
    S = S.ltrim();
    auto Len = Saved.size() - S.size();
    if (Len == 0)
      return S;
    CurBuf.LineNumber += Saved.substr(0, Len).count('\n');
  }
}

// Used to determine whether to stop parsing. Treat errors like EOF.
bool ScriptLexer::atEOF() { return Eof || !diagnose(); }

StringRef ScriptLexer::next() {
  PrevTok = peek();
  // `prevTokLine` is not updated for EOF so that the line number in `setError`
  // will be more useful.
  if (PrevTok.size())
    PrevTokLine = CurBuf.LineNumber;
  return std::exchange(CurTok, StringRef(CurBuf.S.data(), 0));
}

llvm::StringRef ScriptLexer::next(enum LexState PLexState) {
  llvm::SaveAndRestore SaveLexState(LexState, PLexState);
  return next();
}

StringRef ScriptLexer::peek() {
  // curTok is invalid if curTokLexState and lexState mismatch.
  if (CurTok.size() && CurTokLexState != LexState) {
    CurBuf.S = StringRef(CurTok.data(), CurBuf.S.end() - CurTok.data());
    CurTok = {};
  }
  if (CurTok.empty())
    lex();
  return CurTok;
}

llvm::StringRef ScriptLexer::peek(enum LexState PLexState) {
  llvm::SaveAndRestore SaveLexState(LexState, PLexState);
  return peek();
}

bool ScriptLexer::consume(StringRef Tok) {
  if (peek() != Tok)
    return false;
  next();
  return true;
}

void ScriptLexer::skip() { (void)next(); }

void ScriptLexer::expect(StringRef Expect) {
  if (!diagnose())
    return;
  StringRef Tok = next();
  if (Tok != Expect) {
    if (atEOF())
      setError("unexpected EOF");
    else
      setError(Expect + " expected, but got " + Tok);
  }
}

void ScriptLexer::expectButContinue(StringRef Expect) {
  if (!diagnose())
    return;
  StringRef Tok = peek();
  if (Tok != Expect) {
    if (atEOF())
      setError("unexpected EOF");
    else {
      setError(Expect + " expected, but got " + Tok);
      ++MNonFatalErrors;
    }
  } else {
    next();
  }
}

// Returns true if S encloses T.
bool ScriptLexer::encloses(StringRef S, StringRef T) const {
  return S.bytes_begin() <= T.bytes_begin() && T.bytes_end() <= S.bytes_end();
}

MemoryBufferRef ScriptLexer::getCurrentMB() const {
  // Find input buffer containing the current token.
  assert(!MemoryBuffers.empty());
  for (MemoryBufferRef Mb : MemoryBuffers)
    if (encloses(Mb.getBuffer(), CurBuf.S))
      return Mb;
  return MemoryBufferRef();
}

StringRef ScriptLexer::unquote(StringRef S) {
  if (S.starts_with("\""))
    return S.substr(1, S.size() - 2);
  return S;
}

void ScriptLexer::prev() {
  if (!PrevTok.empty()) {
    // FIXME: CurBuf.LineNumber needs to be updated!
    CurBuf.S = PrevTok.data();
    CurTok = {};
  }
}

llvm::StringRef
ScriptLexer::noteAndSkipNonASCIIUnicodeChars(llvm::StringRef s) const {
  bool DiagReported = false;
  while (!s.empty() && isNonASCIIUnicode(s[0])) {
    if (!DiagReported && isFirstByteOfMultiByteUnicode(s[0])) {
      setNote("treating non-ascii unicode character as whitespace", s);
      DiagReported = true;
    }
    s = s.drop_front();
  }
  return s;
}

size_t ScriptLexer::computeColumnWidth(llvm::StringRef s,
                                       llvm::StringRef e) const {
  size_t nonASCIIColumnOffset = 0;
  std::for_each(s.data(), e.data(), [&nonASCIIColumnOffset, this](char c) {
    if (isNonASCIIUnicode(c) && !isFirstByteOfMultiByteUnicode(c))
      ++nonASCIIColumnOffset;
  });
  return e.data() - s.data() - nonASCIIColumnOffset;
}

size_t ScriptLexer::computeLineNumber(llvm::StringRef tok) {
  size_t LineNumber =
      llvm::StringRef(CurBuf.Begin, tok.data() - CurBuf.Begin).count('\n');
  return LineNumber + 1;
}
