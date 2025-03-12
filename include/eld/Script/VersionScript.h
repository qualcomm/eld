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
      : m_Kind(K), m_Symbol(P) {}

  explicit VersionSymbol(VersionSymbolKind K, ScriptSymbol *P,
                         eld::StrToken *Language)
      : m_Kind(K), m_Symbol(P), m_Language(Language) {}

  void setBlock(VersionScriptBlock *B) { m_Block = B; }

  VersionScriptBlock *getBlock() const { return m_Block; }

  ScriptSymbol *getSymbolPattern() const { return m_Symbol; }

  bool isGlobal() const;

  bool isLocal() const;

  void dump(llvm::raw_ostream &ostream,
            std::function<std::string(const Input *)> getDecoratedPath) const;

protected:
  VersionSymbolKind m_Kind;
  ScriptSymbol *m_Symbol = nullptr;
  eld::StrToken *m_Language = nullptr;
  eld::VersionScriptBlock *m_Block = nullptr;
};

class VersionScriptBlock {
public:
  typedef enum : uint8_t {
    Local,
    Global,
  } BlockKind;

public:
  VersionScriptBlock(BlockKind K, VersionScriptNode *N)
      : m_Kind(K), m_Node(N) {}

  void addSymbol(eld::ScriptSymbol *S, eld::StrToken *Language);

  bool isLocal() const { return m_Kind == Local; }

  bool isGlobal() const { return m_Kind == Global; }

  void setNode(VersionScriptNode *N) { m_Node = N; }

  VersionScriptNode *getNode() const { return m_Node; }

  const std::vector<VersionSymbol *> &getSymbols() const { return m_Symbols; }

  std::vector<VersionSymbol *> &getSymbols() { return m_Symbols; }

  virtual void
  dump(llvm::raw_ostream &ostream,
       std::function<std::string(const Input *)> getDecoratedPath) const;

  virtual ~VersionScriptBlock() {}

protected:
  std::vector<VersionSymbol *> m_Symbols;
  VersionScriptBlock::BlockKind m_Kind = Global;
  VersionScriptNode *m_Node = nullptr;
};

class LocalVersionScriptBlock : public VersionScriptBlock {
public:
  LocalVersionScriptBlock(VersionScriptNode *N)
      : VersionScriptBlock(VersionScriptBlock::BlockKind::Local, N) {}
  void dump(llvm::raw_ostream &ostream,
            std::function<std::string(const Input *)> getDecoratedPath)
      const override;
};

class GlobalVersionScriptBlock : public VersionScriptBlock {
public:
  GlobalVersionScriptBlock(VersionScriptNode *N)
      : VersionScriptBlock(VersionScriptBlock::BlockKind::Global, N) {}
  void dump(llvm::raw_ostream &ostream,
            std::function<std::string(const Input *)> getDecoratedPath)
      const override;
};

class VersionScriptNode {
public:
  VersionScriptNode();

  VersionScriptBlock *switchToGlobal();

  VersionScriptBlock *switchToLocal();

  VersionScriptBlock *getCurrentBlock() const { return m_CurrentBlock; }

  void setExternLanguage(eld::StrToken *Language) { m_Language = Language; }

  void resetExternLanguage() { m_Language = nullptr; }

  void addSymbol(eld::ScriptSymbol *S);

  void setName(eld::StrToken *Name) { m_Name = Name; }

  void setDependency(eld::StrToken *Dependency) { m_Dependency = Dependency; }

  VersionScriptBlock *getLocalBlock() const { return m_Local; }

  VersionScriptBlock *getGlobalBlock() const { return m_Global; }

  bool isAnonymous() const;

  bool hasDependency() const;

  bool hasError() const { return m_HasErrorDuringParsing; }

  void dump(llvm::raw_ostream &ostream,
            std::function<std::string(const Input *)> getDecoratedPath) const;

private:
  VersionScriptBlock *m_Local = nullptr;
  VersionScriptBlock *m_Global = nullptr;
  VersionScriptBlock *m_CurrentBlock = nullptr;
  eld::StrToken *m_Language = nullptr;
  eld::StrToken *m_Name = nullptr;
  eld::StrToken *m_Dependency = nullptr;
  bool m_HasErrorDuringParsing = false;
};

class VersionScript {
public:
  VersionScript(InputFile *inp);

  VersionScriptNode *createVersionScriptNode();

  void addSymbol(eld::ScriptSymbol *S);

  void setExternLanguage(eld::StrToken *Language);

  void resetExternLanguage();

  VersionScriptNode *getCurrentNode() const { return m_CurrentNode; }

  const std::vector<const VersionScriptNode *> &getNodes() const {
    return m_Nodes;
  }

  InputFile *getInputFile() const { return m_InputFile; }

  void dump(llvm::raw_ostream &ostream,
            std::function<std::string(const Input *)> getDecoratedPath) const;

private:
  InputFile *m_InputFile = nullptr;
  VersionScriptNode *m_CurrentNode = nullptr;
  std::vector<const VersionScriptNode *> m_Nodes;
};
} // namespace eld
#endif
