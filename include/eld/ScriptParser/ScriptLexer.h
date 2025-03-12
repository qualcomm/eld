//===- ScriptLexer.h-------------------------------------------------------===//
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

#ifndef ELD_SCRIPT_LEXER_V2_H
#define ELD_SCRIPT_LEXER_V2_H

#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/MemoryBufferRef.h"
#include <vector>

namespace eld {

class LinkerConfig;
class ScriptFile;

namespace v2 {

class ScriptLexer {
public:
  enum class LexState {
    Default,
    Expr,
    SectionName,
  };

  explicit ScriptLexer(eld::LinkerConfig &Config, ScriptFile &scriptFile);

  // Set error as needed
  void setError(const llvm::Twine &msg);

  void lex();

  // Skip spaces
  llvm::StringRef skipSpace(llvm::StringRef s);

  // Is lexer reached EOF ?
  bool atEOF();

  // Go to next token
  llvm::StringRef next();

  // Go to next token and use 'pLexState' for lexing the token.
  llvm::StringRef next(LexState pLexState);

  // Peek next token
  llvm::StringRef peek();

  // Peek next token using the pLexState.
  llvm::StringRef peek(LexState pLexState);

  // Skip current token
  void skip();

  // Consume otken
  bool consume(llvm::StringRef tok);

  // Expect next token to be expect
  void expect(llvm::StringRef expect);

  void expectButContinue(llvm::StringRef expect);

  // Consume label
  bool consumeLabel(llvm::StringRef tok);

  // Check if s encloses t
  bool encloses(llvm::StringRef s, llvm::StringRef t);

  // Get current location of token including filename
  std::string getCurrentLocation();

  // Unquote string if quoted
  llvm::StringRef unquote(llvm::StringRef s);

  // Get Line and column information
  llvm::StringRef getLine();

  /// Returns true if there are no reported errors that should
  /// end the link abruptly.
  bool Diagnose() const;

  /// Move the cursor to the previous token. This can be used to move the cursor
  /// back by at most one token. Consecutive calls to prev, without any next()
  /// calls in between, does not change the cursor position. If there
  /// is no previous token, this function is no-op.
  void prev();

protected:
  // Get current memory buffer
  llvm::MemoryBufferRef getCurrentMB();

  size_t getColumnNumber();

private:
  // Handle expression splits.
  void maybeSplitExpr();

  // Check if there is an error

protected:
  struct Buffer {
    // The remaining content to parse and the filename.
    llvm::StringRef s, filename;
    const char *begin = nullptr;
    size_t lineNumber = 1;
    Buffer() = default;
    Buffer(llvm::MemoryBufferRef mb)
        : s(mb.getBuffer()), filename(mb.getBufferIdentifier()),
          begin(mb.getBufferStart()) {}
  };

  eld::LinkerConfig &m_Config;

  // This changes the tokenization rules. Expression tokenization rules are
  // different than non-expression tokenization rules. The difference in Default
  // and SectionName tokenization rule is that SectionName lex state considers
  // ':' as token separator whereas Default does not.
  LexState lexState = LexState::Default;

  // This is required to support '=<fill-expression>'
  // before /DISCARD/ output section description. For example:
  // ```
  // SECTIONS {
  //   FOO : { *(*foo*) } =100+3
  //   /DISCARD/ : { *(*bar*) }
  // }
  //
  // Using normal expression parsing rules, the parser would see tokens:
  // ['100', '+', '3', '/', 'DISCARD', '/'] when parsing the above stated
  // fill-expression. Here, '/' would be considered a division operator rather
  // than the part of '/DISCARD/'. m_InOutputSectEpilogue is used to modify
  // the expression parsing behavior to not split '/DISCARD/' into multiple
  // tokens when expression parsing is happening in output section epilogue.
  bool m_InOutputSectEpilogue = false;

  // The current buffer and parent buffers due to INCLUDE.
  Buffer curBuf;
  llvm::SmallVector<Buffer, 0> buffers;

  // Used to detect INCLUDE() cycles.
  llvm::DenseSet<llvm::StringRef> activeFilenames;

  // The token before the last next().
  llvm::StringRef prevTok;
  // Rules for what is a token are different when we are in an expression.
  // curTok holds the cached return value of peek() and is invalid when the
  // expression state changes.
  llvm::StringRef curTok;
  size_t prevTokLine = 1;
  /// This stores the lex state which was used to tokenize the current token.
  /// Current token term is a little confusing/misleading. Current token here
  /// refers to the token that is returned by the peek() / next() calls. It is
  /// useful to store the current token lex state to know whether the current
  /// token is invalidated and must be recomputed.
  LexState curTokLexState = LexState::Default;
  bool eof = false;

  // All the memory buffers that need to be parsed.
  std::vector<llvm::MemoryBufferRef> mbs;

  ScriptFile &m_ScriptFile;

private:
  // State of the lexer.
  size_t lastLineNumber = 0;
  size_t lastLineNumberOffset = 0;

  size_t m_NonFatalErrors = 0;
};

} // namespace v2
} // namespace eld

#endif
