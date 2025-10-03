//===- ScriptParser.h------------------------------------------------------===//
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
#ifndef ELD_SCRIPTPARSER_SCRIPTPARSER_H
#define ELD_SCRIPTPARSER_SCRIPTPARSER_H

#include "eld/Script/Assignment.h"
#include "eld/Script/InputSectDesc.h"
#include "eld/Script/OutputSectDesc.h"
#include "eld/Script/VersionScript.h"
#include "eld/ScriptParser/ScriptLexer.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/MemoryBufferRef.h"
#include <optional>

namespace eld {
class Expression;
namespace v2 {

class ScriptLexer;

class ScriptParser final : ScriptLexer {
public:
  ScriptParser(eld::LinkerConfig &Config, eld::ScriptFile &ScriptFile);

  void readLinkerScript();

  void readVersionScript();

  void readDynamicList();

  void parse();

  void readExternList();

private:
  /// read ENTRY linker script command
  void readEntry();

  Expression *readAssert();

  /// Parses an assignment command.
  ///
  /// Assignment commands consists of:
  /// - symbol [op]= expression;
  /// - HIDDEN(symbol [op]= expression);
  /// - PROVIDE(symbol = expression);
  /// - PROVIDE_HIDDEN(symbol = expression);
  bool readAssignment(llvm::StringRef Tok);

  /// This is an operator-precedence parser to parse a linker
  /// script expression.
  Expression *readExpr();

  /// This is a part of the operator-precedence parser. This function
  /// assumes that the remaining token stream starts with an operator.
  Expression *readExpr1(Expression *Lhs, int MinPrec);

  /// Returns operator precedence.
  int precedence(llvm::StringRef Op);

  /// Creates a new expression object: 'l op r'
  Expression &combine(llvm::StringRef Op, Expression &L, Expression &R);

  /// Reads an expression until a binary operator is found.
  Expression *readPrimary();

  Expression *readParenExpr(bool SetParen);

  /// Reads a string literal enclosed within parenthesis.
  llvm::StringRef readParenLiteral();

  /// Reads 'CONSTANT (COMMONPAGESIZE)' and 'CONSTANT (MAXPAGESIZE)' commands.
  Expression *readConstant();

  /// Parses Tok as an integer. It recognizes hexadecimal (prefixed with
  /// "0x" or suffixed with "H") and decimal numbers. Decimal numbers may
  /// have "K" (Ki) or "M" (Mi) suffixes.
  std::optional<uint64_t> parseInt(llvm::StringRef Tok) const;

  bool isValidSymbolName(llvm::StringRef Name);

  bool readSymbolAssignment(llvm::StringRef Tok,
                            Assignment::Type Type = Assignment::Type::DEFAULT);

  Expression *readTernary(Expression *Cond);

  void readProvideHidden(llvm::StringRef Tok);

  /// Parses SECTIONS command.
  ///
  /// \code
  /// SECTIONS {
  ///   sections-command
  ///   sections-command
  ///   ...
  /// }
  /// \endcode
  ///
  /// A sections-command can be one of:
  /// - ENTRY command
  /// - symbol assignment
  /// - Output section description
  ///
  /// \note We do not support overlay description yet.
  // TODO: Currently only symbol assigment is supported in SECTIONS
  // command.
  void readSections();

  /// Reads INPUT(...) and GROUP(...) command.
  void readInputOrGroup(bool IsInputCmd);

  void addFile(llvm::StringRef Name);

  /// Reads AS_NEEDED(...) subcommand.
  void readAsNeeded();

  void readOutput();

  void readOutputSectionDescription(llvm::StringRef Tok);

  void readInputSectionDescription(llvm::StringRef Tok);

  InputSectDesc::Spec readInputSectionDescSpec(llvm::StringRef Tok);

  /// Reads output section description prologue. It currently supports reading
  /// output section VMA, type and permissions.
  OutputSectDesc::Prolog readOutputSectDescPrologue();

  /// Reads output section type and permissions. Returns false if the token tok
  /// does not point to output section type keyword. Otherwise returns true.
  bool readOutputSectTypeAndPermissions(OutputSectDesc::Prolog &Prologue,
                                        llvm::StringRef Tok);

  std::optional<OutputSectDesc::Type> readOutputSectType(llvm::StringRef Tok);

  std::optional<uint32_t> readOutputSectPermissions(llvm::StringRef Tok);

  void readPhdrs();

  std::optional<uint32_t> readPhdrType(llvm::StringRef Tok) const;

  OutputSectDesc::Epilog readOutputSectDescEpilogue();

  void readNoCrossRefs();

  bool readPluginDirective(llvm::StringRef Tok);

  void readSearchDir();

  void readOutputArch();

  void readMemory();

  StrToken *readMemoryAttributes();

  Expression *readMemoryAssignment(std::vector<llvm::StringRef> Names);

  void readExtern();

  void readRegionAlias();

  bool readOutputSectionData(llvm::StringRef Tok);

  std::optional<WildcardPattern::SortPolicy> readSortPolicy();

  WildcardPattern::SortPolicy
  computeSortPolicy(WildcardPattern::SortPolicy OuterSortPolicy,
                    std::optional<WildcardPattern::SortPolicy> InnerSortPolicy =
                        std::nullopt);

  WildcardPattern *readWildcardPattern();

  PluginCmd *readOutputSectionPluginDirective();

  void readFill();

  ExcludeFiles *readExcludeFile();

  bool readInclude(llvm::StringRef Tok);

  llvm::StringRef readParenName();

  void readOutputFormat();

  void readVersion();

  void readVersionScriptCommand();

  void readVersionDeclaration(llvm::StringRef VerStr);

  // Reads a list of symbols, e.g. "{ global: foo; bar; local: *; };".
  void readVersionSymbols(VersionScriptNode &VSN);

  void readVersionExtern(VersionScriptNode &VSN);

  /// Returns true if the pattern is a valid file pattern.
  ///
  /// This API takes into account that Pat is parsed in LexState::Default.
  /// Hence, it does not check if Pat contains any invalid character such as
  /// '{' in between the pattern, as this case, would not be possible with
  /// LexState::Default.
  bool isValidFilePattern(llvm::StringRef Pat);

  /// Returns true if the pattern is a valid file pattern.
  ///
  /// This API takes into account that Pat is parsed in LexState::Default.
  /// Hence, it does not check if Pat contains any invalid character such as
  /// '{' in between the pattern, as this case, would not be possible with
  /// LexState::Default.
  bool isValidSectionPattern(llvm::StringRef Pat);

  StrToken *readName(llvm::StringRef Name);
};
} // namespace v2
} // namespace eld

#endif
