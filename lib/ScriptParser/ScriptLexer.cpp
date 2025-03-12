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
StringRef ScriptLexer::getLine() {
  StringRef s = getCurrentMB().getBuffer();

  size_t pos = s.rfind('\n', prevTok.data() - s.data());
  if (pos != StringRef::npos)
    s = s.substr(pos + 1);
  return s.substr(0, s.find_first_of("\r\n"));
}

// Returns 0-based column number of the current token.
size_t ScriptLexer::getColumnNumber() {
  return prevTok.data() - getLine().data();
}

std::string ScriptLexer::getCurrentLocation() {
  std::string filename = std::string(getCurrentMB().getBufferIdentifier());
  return (filename + ":" + Twine(prevTokLine)).str();
}

ScriptLexer::ScriptLexer(eld::LinkerConfig &Config, ScriptFile &scriptFile)
    : m_Config(Config), m_ScriptFile(scriptFile) {
  activeFilenames.insert(m_ScriptFile.getPath());
  InputFile *IF = m_ScriptFile.getContext();
  llvm::MemoryBufferRef memBufRef = IF->getInput()->getMemoryBufferRef();
  curBuf = Buffer(memBufRef);
  mbs.push_back(memBufRef);
}

bool ScriptLexer::Diagnose() const {
  DiagnosticPrinter *DP = m_Config.getPrinter();
  if (DP->getNumFatalErrors() == 0 && DP->getNumErrors() == m_NonFatalErrors) {
    return true;
  }
  return m_Config.getDiagEngine()->diagnose();
}

// We don't want to record cascading errors. Keep only the first one.
void ScriptLexer::setError(const Twine &msg) {
  if (!Diagnose())
    return;

  std::string s = (getCurrentLocation() + ": " + msg).str();
  if (prevTok.size())
    s += "\n>>> " + getLine().str() + "\n>>> " +
         std::string(getColumnNumber(), ' ') + "^";
  m_Config.raise(diag::error_linker_script) << s;
}

void ScriptLexer::lex() {
  for (;;) {
    StringRef &s = curBuf.s;
    s = skipSpace(s);
    if (s.empty()) {
      // If this buffer is from an INCLUDE command, switch to the "return
      // value"; otherwise, mark EOF.
      if (buffers.empty()) {
        eof = true;
        return;
      }
      activeFilenames.erase(curBuf.filename);
      m_ScriptFile.popScriptStack();
      curBuf = buffers.pop_back_val();
      LayoutPrinter *LP = m_ScriptFile.module().getLayoutPrinter();
      if (LP)
        LP->closeLinkerScript();
      continue;
    }
    curTokLexState = lexState;

    // Quoted token. Note that double-quote characters are parts of a token
    // because, in a glob match context, only unquoted tokens are interpreted
    // as glob patterns. Double-quoted tokens are literal patterns in that
    // context.
    if (s.starts_with("\"")) {
      size_t e = s.find("\"", 1);
      if (e == StringRef::npos) {
        size_t lineno =
            StringRef(curBuf.begin, s.data() - curBuf.begin).count('\n');
        m_Config.raise(diag::error_linker_script)
            << llvm::Twine(curBuf.filename + ":" + Twine(lineno + 1) +
                           ": unclosed quote")
                   .str();
        return;
      }

      curTok = s.take_front(e + 1);
      s = s.substr(e + 1);
      return;
    }

    // Some operators form separate tokens.
    if (s.starts_with("<<=") || s.starts_with(">>=")) {
      curTok = s.substr(0, 3);
      s = s.substr(3);
      return;
    }

    if (s.size() > 1 && (s[1] == '=' && strchr("+-*/!&^|", s[0]))) {
      curTok = s.substr(0, 2);
      s = s.substr(2);
      return;
    }

    // Unquoted token. The non-expression token is more relaxed than tokens in
    // C-like languages, so that you can write "file-name.cpp" as one bare
    // token.
    size_t pos;
    if (curTokLexState == LexState::Expr) {
      pos = s.find_first_not_of(
          "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
          "0123456789_.$");
      if (pos == 0 && s.size() >= 2 &&
          ((s[0] == s[1] && strchr("<>&|", s[0])) ||
           is_contained({"==", "!=", "<=", ">=", "<<", ">>"}, s.substr(0, 2))))
        pos = 2;
      if (s.starts_with("/DISCARD/"))
        pos = llvm::StringRef("/DISCARD/").size();
    } else {
      // Last char must be ':'!!!
      llvm::StringRef tokenChars =
          "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
          "0123456789_.$/\\~=+[]*?-!^:";
      // Drop ':' if we are lexing output section name
      if (curTokLexState == LexState::SectionName)
        tokenChars = tokenChars.drop_back();

      pos = s.find_first_not_of(tokenChars);
    }

    if (pos == 0)
      pos = 1;
    curTok = s.substr(0, pos);
    s = s.substr(pos);
    break;
  }
}

// Skip leading whitespace characters or comments.
StringRef ScriptLexer::skipSpace(StringRef s) {
  for (;;) {
    if (s.starts_with("/*")) {
      size_t e = s.find("*/", 2);
      if (e == StringRef::npos) {
        setError("unclosed comment in a linker script");
        return "";
      }
      curBuf.lineNumber += s.substr(0, e).count('\n');
      s = s.substr(e + 2);
      continue;
    }
    if (s.starts_with("#") || s.starts_with("//")) {
      size_t e = s.find('\n', 1);
      if (e == StringRef::npos)
        e = s.size() - 1;
      else
        ++curBuf.lineNumber;
      s = s.substr(e + 1);
      continue;
    }
    StringRef saved = s;
    s = s.ltrim();
    auto len = saved.size() - s.size();
    if (len == 0)
      return s;
    curBuf.lineNumber += saved.substr(0, len).count('\n');
  }
}

// Used to determine whether to stop parsing. Treat errors like EOF.
bool ScriptLexer::atEOF() { return eof || !Diagnose(); }

StringRef ScriptLexer::next() {
  prevTok = peek();
  // `prevTokLine` is not updated for EOF so that the line number in `setError`
  // will be more useful.
  if (prevTok.size())
    prevTokLine = curBuf.lineNumber;
  return std::exchange(curTok, StringRef(curBuf.s.data(), 0));
}

llvm::StringRef ScriptLexer::next(LexState pLexState) {
  llvm::SaveAndRestore saveLexState(lexState, pLexState);
  return next();
}

StringRef ScriptLexer::peek() {
  // curTok is invalid if curTokLexState and lexState mismatch.
  if (curTok.size() && curTokLexState != lexState) {
    curBuf.s = StringRef(curTok.data(), curBuf.s.end() - curTok.data());
    curTok = {};
  }
  if (curTok.empty())
    lex();
  return curTok;
}

llvm::StringRef ScriptLexer::peek(LexState pLexState) {
  llvm::SaveAndRestore saveLexState(lexState, pLexState);
  return peek();
}

bool ScriptLexer::consume(StringRef tok) {
  if (peek() != tok)
    return false;
  next();
  return true;
}

void ScriptLexer::skip() { (void)next(); }

void ScriptLexer::expect(StringRef expect) {
  if (!Diagnose())
    return;
  StringRef tok = next();
  if (tok != expect) {
    if (atEOF())
      setError("unexpected EOF");
    else
      setError(expect + " expected, but got " + tok);
  }
}

void ScriptLexer::expectButContinue(StringRef expect) {
  if (!Diagnose())
    return;
  StringRef tok = peek();
  if (tok != expect) {
    if (atEOF())
      setError("unexpected EOF");
    else {
      setError(expect + " expected, but got " + tok);
      ++m_NonFatalErrors;
    }
  } else {
    next();
  }
}

// Returns true if S encloses T.
bool ScriptLexer::encloses(StringRef s, StringRef t) {
  return s.bytes_begin() <= t.bytes_begin() && t.bytes_end() <= s.bytes_end();
}

MemoryBufferRef ScriptLexer::getCurrentMB() {
  // Find input buffer containing the current token.
  assert(!mbs.empty());
  for (MemoryBufferRef mb : mbs)
    if (encloses(mb.getBuffer(), curBuf.s))
      return mb;
  return MemoryBufferRef();
}

StringRef ScriptLexer::unquote(StringRef s) {
  if (s.starts_with("\""))
    return s.substr(1, s.size() - 2);
  return s;
}

void ScriptLexer::prev() {
  if (!prevTok.empty()) {
    curBuf.s = prevTok.data();
    curTok = {};
  }
}
