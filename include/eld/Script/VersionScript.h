//===- VersionScript.h-----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SCRIPT_VERSIONSCRIPT_H
#define ELD_SCRIPT_VERSIONSCRIPT_H

#include "eld/Script/ScriptSymbol.h"
#include <string>
namespace eld {
class StrToken;
class InputFile;
class VersionScriptBlock;
class VersionScriptNode;
class NamePool;

/*
There are fundamentally the following classes used to model version
scripts :-

- VersionSymbol (represents a versioned symbol)
- VersionScriptBlock (the versioned symbol is housed in a block)
  - LocalVersionScriptBlock
  - GlobalVersionScriptBlock
- VersionScriptNode (Version Script Node houses a local and a global block)
- VersionScript (Houses version script nodes)
*/

class VersionSymbol {
public:
  typedef enum : uint8_t { Simple, Extern } VersionSymbolKind;

public:
  explicit VersionSymbol(VersionSymbolKind K, ScriptSymbol *P)
      : ScriptFileKind(K), ThisSymbol(P) {}

  explicit VersionSymbol(VersionSymbolKind K, ScriptSymbol *P,
                         eld::StrToken *Language)
      : ScriptFileKind(K), ThisSymbol(P), VersionScriptLanguage(Language) {}

  void setBlock(VersionScriptBlock *B) { VersionScriptBlock = B; }

  VersionScriptBlock *getBlock() const { return VersionScriptBlock; }

  ScriptSymbol *getSymbolPattern() const { return ThisSymbol; }

  bool isGlobal() const;

  bool isLocal() const;

  void dump(llvm::raw_ostream &Ostream,
            std::function<std::string(const Input *)> GetDecoratedPath) const;

  /// Returns true if the symbol R matches the version symbol.
  ///
  /// A symbol R matches the version symbol when both the conditions are satisfied:
  /// - The symbol version name is the same as the version node.
  /// - The symbol non-versioned name matches the version symbol pattern.
  ///
  /// There is an exception to this rule. When a symbol does not have any
  /// versioned names, for example, there is foo, but no foo@V1 or foo@@V1
  /// is present in the link. In this case, the symbol version name
  /// does not need to match the version node. It is sufficient that
  /// the symbol non-versioned name matches the version symbol pattern.
  bool matched(const ResolveInfo &R, const NamePool &NP) const;

protected:
  VersionSymbolKind ScriptFileKind;
  ScriptSymbol *ThisSymbol = nullptr;
  eld::StrToken *VersionScriptLanguage = nullptr;
  eld::VersionScriptBlock *VersionScriptBlock = nullptr;
};

class VersionScriptBlock {
public:
  typedef enum : uint8_t {
    Local,
    Global,
  } BlockKind;

public:
  VersionScriptBlock(BlockKind K, class VersionScriptNode *N)
      : ScriptFileKind(K), VersionScriptNode(N) {}

  void addSymbol(eld::ScriptSymbol *S, eld::StrToken *Language);

  bool isLocal() const { return ScriptFileKind == Local; }

  bool isGlobal() const { return ScriptFileKind == Global; }

  void setNode(VersionScriptNode *N) { VersionScriptNode = N; }

  VersionScriptNode *getNode() const { return VersionScriptNode; }

  const std::vector<VersionSymbol *> &getSymbols() const { return ThisSymbols; }

  std::vector<VersionSymbol *> &getSymbols() { return ThisSymbols; }

  virtual void
  dump(llvm::raw_ostream &Ostream,
       std::function<std::string(const Input *)> GetDecoratedPath) const;

  virtual ~VersionScriptBlock() {}

protected:
  std::vector<VersionSymbol *> ThisSymbols;
  VersionScriptBlock::BlockKind ScriptFileKind = Global;
  VersionScriptNode *VersionScriptNode = nullptr;
};

class LocalVersionScriptBlock : public VersionScriptBlock {
public:
  LocalVersionScriptBlock(class VersionScriptNode *N)
      : VersionScriptBlock(VersionScriptBlock::BlockKind::Local, N) {}
  void dump(llvm::raw_ostream &Ostream,
            std::function<std::string(const Input *)> GetDecoratedPath)
      const override;
};

class GlobalVersionScriptBlock : public VersionScriptBlock {
public:
  GlobalVersionScriptBlock(class VersionScriptNode *N)
      : VersionScriptBlock(VersionScriptBlock::BlockKind::Global, N) {}
  void dump(llvm::raw_ostream &Ostream,
            std::function<std::string(const Input *)> GetDecoratedPath)
      const override;
};

class VersionScriptNode {
public:
  VersionScriptNode();

  VersionScriptBlock *switchToGlobal();

  VersionScriptBlock *switchToLocal();

  VersionScriptBlock *getCurrentBlock() const { return MCurrentBlock; }

  void setExternLanguage(eld::StrToken *Language) {
    VersionScriptLanguage = Language;
  }

  void resetExternLanguage() { VersionScriptLanguage = nullptr; }

  void addSymbol(eld::ScriptSymbol *S);

  void setName(eld::StrToken *Name) { this->Name = Name; }

  llvm::StringRef getName() const {
    return Name->name();
  }

  void setDependency(eld::StrToken *Dependency) { MDependency = Dependency; }

  VersionScriptBlock *getLocalBlock() const { return MLocal; }

  VersionScriptBlock *getGlobalBlock() const { return MGlobal; }

  bool isAnonymous() const;

  bool hasDependency() const;

  bool hasError() const { return MHasErrorDuringParsing; }

  void dump(llvm::raw_ostream &Ostream,
            std::function<std::string(const Input *)> GetDecoratedPath) const;

private:
  VersionScriptBlock *MLocal = nullptr;
  VersionScriptBlock *MGlobal = nullptr;
  VersionScriptBlock *MCurrentBlock = nullptr;
  eld::StrToken *VersionScriptLanguage = nullptr;
  eld::StrToken *Name = nullptr;
  eld::StrToken *MDependency = nullptr;
  bool MHasErrorDuringParsing = false;
};

class VersionScript {
public:
  VersionScript(InputFile *Inp);

  VersionScriptNode *createVersionScriptNode();

  void addSymbol(eld::ScriptSymbol *S);

  void setExternLanguage(eld::StrToken *Language);

  void resetExternLanguage();

  VersionScriptNode *getCurrentNode() const { return MCurrentNode; }

  const std::vector<const VersionScriptNode *> &getNodes() const {
    return VersionScriptNodes;
  }

  InputFile *getInputFile() const { return MInputFile; }

  void dump(llvm::raw_ostream &Ostream,
            std::function<std::string(const Input *)> GetDecoratedPath) const;

private:
  InputFile *MInputFile = nullptr;
  VersionScriptNode *MCurrentNode = nullptr;
  std::vector<const VersionScriptNode *> VersionScriptNodes;
};
} // namespace eld
#endif
