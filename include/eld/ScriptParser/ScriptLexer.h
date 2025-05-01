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

#ifndef ELD_SCRIPTPARSER_SCRIPTLEXER_H
#define ELD_SCRIPTPARSER_SCRIPTLEXER_H

#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/MemoryBufferRef.h"
#include <optional>
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

  explicit ScriptLexer(eld::LinkerConfig &Config, ScriptFile &ScriptFile);

  // Set error as needed
  void setError(const llvm::Twine &Msg);

  void setNote(const llvm::Twine &msg,
               std::optional<llvm::StringRef> columnTok = std::nullopt) const;

  void lex();

  // Skip spaces
  llvm::StringRef skipSpace(llvm::StringRef S);

  // Is lexer reached EOF ?
  bool atEOF();

  // Go to next token
  llvm::StringRef next();

  // Go to next token and use 'pLexState' for lexing the token.
  llvm::StringRef next(LexState PLexState);

  // Peek next token
  llvm::StringRef peek();

  // Peek next token using the pLexState.
  llvm::StringRef peek(LexState PLexState);

  // Skip current token
  void skip();

  // Consume otken
  bool consume(llvm::StringRef Tok);

  // Expect next token to be expect
  void expect(llvm::StringRef Expect);

  void expectButContinue(llvm::StringRef Expect);

  // Consume label
  bool consumeLabel(llvm::StringRef Tok);

  // Check if s encloses t
  bool encloses(llvm::StringRef S, llvm::StringRef T) const;

  // Get current location of token including filename
  std::string getCurrentLocation() const;

  // Unquote string if quoted
  llvm::StringRef unquote(llvm::StringRef S);

  // Get Line and column information
  llvm::StringRef getLine() const;

  /// Returns true if there are no reported errors that should
  /// end the link abruptly.
  bool diagnose() const;

  /// Move the cursor to the previous token. This can be used to move the cursor
  /// back by at most one token. Consecutive calls to prev, without any next()
  /// calls in between, does not change the cursor position. If there
  /// is no previous token, this function is no-op.
  void prev();

protected:
  // Get current memory buffer
  llvm::MemoryBufferRef getCurrentMB() const;

  size_t getColumnNumber() const;

  size_t computeColumnWidth(llvm::StringRef s, llvm::StringRef e) const;

private:
  // Handle expression splits.
  void maybeSplitExpr();

  llvm::StringRef noteAndSkipNonASCIIUnicodeChars(llvm::StringRef s) const;

  bool isNonASCIIUnicode(char c) const { return c & 0x80; }

  bool isFirstByteOfMultiByteUnicode(char c) const {
    return (c & 0xc0) == 0xc0;
  }

protected:
  struct Buffer {
    // The remaining content to parse and the filename.
    llvm::StringRef S, Filename;
    const char *Begin = nullptr;
    size_t LineNumber = 1;
    Buffer() = default;
    Buffer(llvm::MemoryBufferRef Mb)
        : S(Mb.getBuffer()), Filename(Mb.getBufferIdentifier()),
          Begin(Mb.getBufferStart()) {}
  };

  eld::LinkerConfig &ThisConfig;

  // This changes the tokenization rules. Expression tokenization rules are
  // different than non-expression tokenization rules. The difference in Default
  // and SectionName tokenization rule is that SectionName lex state considers
  // ':' as token separator whereas Default does not.
  LexState LexState = LexState::Default;

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
  bool MInOutputSectEpilogue = false;

  // The current buffer and parent buffers due to INCLUDE.
  Buffer CurBuf;
  llvm::SmallVector<Buffer, 0> Buffers;

  // Used to detect INCLUDE() cycles.
  llvm::DenseSet<llvm::StringRef> ActiveFilenames;

  // The token before the last next().
  llvm::StringRef PrevTok;
  // Rules for what is a token are different when we are in an expression.
  // curTok holds the cached return value of peek() and is invalid when the
  // expression state changes.
  llvm::StringRef CurTok;
  size_t PrevTokLine = 1;
  /// This stores the lex state which was used to tokenize the current token.
  /// Current token term is a little confusing/misleading. Current token here
  /// refers to the token that is returned by the peek() / next() calls. It is
  /// useful to store the current token lex state to know whether the current
  /// token is invalidated and must be recomputed.
  enum LexState CurTokLexState = LexState::Default;
  bool Eof = false;

  // All the memory buffers that need to be parsed.
  std::vector<llvm::MemoryBufferRef> MemoryBuffers;

  ScriptFile &ThisScriptFile;

private:
  // State of the lexer.
  size_t LastLineNumber = 0;
  size_t LastLineNumberOffset = 0;

  size_t MNonFatalErrors = 0;
};

} // namespace v2
} // namespace eld

#endif
