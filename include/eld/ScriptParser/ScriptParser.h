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
#ifndef ELD_SCRIPT_PARSER_V2_H
#define ELD_SCRIPT_PARSER_V2_H

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
  ScriptParser(eld::LinkerConfig &Config, eld::ScriptFile &scriptFile);

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
  bool readAssignment();

  /// This is an operator-precedence parser to parse a linker
  /// script expression.
  Expression *readExpr();

  /// This is a part of the operator-precedence parser. This function
  /// assumes that the remaining token stream starts with an operator.
  Expression *readExpr1(Expression *lhs, int minPrec);

  /// Returns operator precedence.
  int precedence(llvm::StringRef op);

  /// Creates a new expression object: 'l op r'
  Expression &combine(llvm::StringRef op, Expression &l, Expression &r);

  /// Reads an expression until a binary operator is found.
  Expression *readPrimary();

  Expression *readParenExpr(bool setParen);

  /// Reads a string literal enclosed within parenthesis.
  llvm::StringRef readParenLiteral();

  /// Reads 'CONSTANT (COMMONPAGESIZE)' and 'CONSTANT (MAXPAGESIZE)' commands.
  Expression *readConstant();

  /// Parses Tok as an integer. It recognizes hexadecimal (prefixed with
  /// "0x" or suffixed with "H") and decimal numbers. Decimal numbers may
  /// have "K" (Ki) or "M" (Mi) suffixes.
  std::optional<uint64_t> parseInt(llvm::StringRef tok) const;

  bool isValidSymbolName(llvm::StringRef name);

  bool readSymbolAssignment(llvm::StringRef tok,
                            Assignment::Type type = Assignment::Type::DEFAULT);

  Expression *readTernary(Expression *cond);

  void readProvideHidden(llvm::StringRef tok);

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
  void readInputOrGroup(bool isInputCmd);

  void addFile(llvm::StringRef name);

  /// Reads AS_NEEDED(...) subcommand.
  void readAsNeeded();

  void readOutput();

  void readOutputSectionDescription(llvm::StringRef outSectName);

  void readInputSectionDescription(llvm::StringRef tok);

  InputSectDesc::Spec readInputSectionDescSpec(llvm::StringRef tok);

  /// Reads output section description prologue. It currently supports reading
  /// output section VMA, type and permissions.
  OutputSectDesc::Prolog readOutputSectDescPrologue();

  /// Reads output section type and permissions. Returns false if the token tok
  /// does not point to output section type keyword. Otherwise returns true.
  bool readOutputSectTypeAndPermissions(OutputSectDesc::Prolog &prologue,
                                        llvm::StringRef tok);

  std::optional<OutputSectDesc::Type> readOutputSectType(llvm::StringRef tok);

  std::optional<uint32_t> readOutputSectPermissions(llvm::StringRef tok);

  void readPhdrs();

  std::optional<uint32_t> readPhdrType(llvm::StringRef tok) const;

  OutputSectDesc::Epilog readOutputSectDescEpilogue();

  void readNoCrossRefs();

  bool readPluginDirective();

  void readSearchDir();

  void readOutputArch();

  void readMemory();

  StrToken *readMemoryAttributes();

  Expression *readMemoryAssignment(std::vector<llvm::StringRef> names);

  void readExtern();

  void readRegionAlias();

  bool readOutputSectionData();

  std::optional<WildcardPattern::SortPolicy> readSortPolicy();

  WildcardPattern::SortPolicy
  computeSortPolicy(WildcardPattern::SortPolicy outerSortPolicy,
                    std::optional<WildcardPattern::SortPolicy> innerSortPolicy =
                        std::nullopt);

  WildcardPattern *readWildcardPattern();

  PluginCmd *readOutputSectionPluginDirective();

  void readFill();

  ExcludeFiles *readExcludeFile();

  bool readInclude();

  llvm::StringRef readParenName();

  void readOutputFormat();

  void readVersion();

  void readVersionScriptCommand();

  void readVersionDeclaration(llvm::StringRef verStr);

  // Reads a list of symbols, e.g. "{ global: foo; bar; local: *; };".
  void readVersionSymbols(VersionScriptNode &VSN);

  void readVersionExtern(VersionScriptNode &VSN);
};
} // namespace v2
} // namespace eld

#endif
