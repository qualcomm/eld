//===- VersionScript.cpp---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Script/VersionScript.h"
#include "eld/Script/ScriptSymbol.h"
#include "eld/Script/SymbolContainer.h"
#include "eld/Support/Memory.h"

using namespace eld;

// -----------------------VersionScript -------------------------------
VersionScript::VersionScript(InputFile *inp) : m_InputFile(inp) {}

VersionScriptNode *VersionScript::createVersionScriptNode() {
  m_CurrentNode = eld::make<VersionScriptNode>();
  m_Nodes.push_back(m_CurrentNode);
  return m_CurrentNode;
}

void VersionScript::setExternLanguage(eld::StrToken *Language) {
  m_CurrentNode->setExternLanguage(Language);
}

void VersionScript::resetExternLanguage() {
  m_CurrentNode->resetExternLanguage();
}

void VersionScript::addSymbol(eld::ScriptSymbol *S) {
  m_CurrentNode->addSymbol(S);
}

void VersionScript::dump(
    llvm::raw_ostream &ostream,
    std::function<std::string(const Input *)> getDecoratedPath) const {
  if (m_Nodes.empty() || !m_InputFile)
    return;
  ostream << "\nVersion Script file\n";
  ostream << getDecoratedPath(m_InputFile->getInput());
  for (const VersionScriptNode *versionNodes : m_Nodes)
    versionNodes->dump(ostream, getDecoratedPath);
}

// -----------------------VersionScriptNode -------------------------------
VersionScriptNode::VersionScriptNode() {}

VersionScriptBlock *VersionScriptNode::switchToGlobal() {
  if (m_Local && !m_Global) {
    m_HasErrorDuringParsing = true;
    return nullptr;
  }
  if (m_HasErrorDuringParsing)
    return nullptr;
  if (m_Global)
    return m_CurrentBlock = m_Global;
  m_Global = eld::make<GlobalVersionScriptBlock>(this);
  m_CurrentBlock = m_Global;
  return m_Global;
}

void VersionScriptNode::dump(
    llvm::raw_ostream &ostream,
    std::function<std::string(const Input *)> getDecoratedPath) const {
  if (m_Global)
    m_Global->dump(ostream, getDecoratedPath);
  if (m_Local)
    m_Local->dump(ostream, getDecoratedPath);
}

VersionScriptBlock *VersionScriptNode::switchToLocal() {
  if (m_Local)
    return m_CurrentBlock = m_Local;
  m_Local = eld::make<LocalVersionScriptBlock>(this);
  m_CurrentBlock = m_Local;
  return m_Local;
}

void VersionScriptNode::addSymbol(eld::ScriptSymbol *S) {
  if (m_HasErrorDuringParsing)
    return;
  if (!m_CurrentBlock)
    switchToGlobal();
  m_CurrentBlock->addSymbol(S, m_Language);
  S->activate();
}

bool VersionScriptNode::isAnonymous() const { return (m_Name == nullptr); }

bool VersionScriptNode::hasDependency() const {
  return (m_Dependency != nullptr);
}
// --------------------------------VersionScriptBlock -------------------

void VersionScriptBlock::addSymbol(eld::ScriptSymbol *S,
                                   eld::StrToken *Language) {
  if (!Language) {
    VersionSymbol *V =
        eld::make<VersionSymbol>(VersionSymbol::VersionSymbolKind::Simple, S);
    V->setBlock(this);
    m_Symbols.push_back(V);
    return;
  }
  VersionSymbol *V = eld::make<VersionSymbol>(
      VersionSymbol::VersionSymbolKind::Extern, S, Language);
  V->setBlock(this);
  m_Symbols.push_back(V);
}

void VersionScriptBlock::dump(
    llvm::raw_ostream &ostream,
    std::function<std::string(const Input *)> getDecoratedPath) const {
  for (VersionSymbol *sym : m_Symbols)
    sym->dump(ostream, getDecoratedPath);
}
// -----------------------GlobalVersionScriptBlock --------------------------
void GlobalVersionScriptBlock::dump(
    llvm::raw_ostream &ostream,
    std::function<std::string(const Input *)> getDecoratedPath) const {
  ostream << "\nGlobal:";
  VersionScriptBlock::dump(ostream, getDecoratedPath);
}

// -----------------------LocalVersionScriptBlock --------------------------
void LocalVersionScriptBlock::dump(
    llvm::raw_ostream &ostream,
    std::function<std::string(const Input *)> getDecoratedPath) const {
  ostream << "\nLocal:";
  VersionScriptBlock::dump(ostream, getDecoratedPath);
}

// -----------------------VersionScriptSymbol -------------------------------
bool VersionSymbol::isGlobal() const { return m_Block->isGlobal(); }

bool VersionSymbol::isLocal() const { return m_Block->isLocal(); }

void VersionSymbol::dump(
    llvm::raw_ostream &ostream,
    std::function<std::string(const Input *)> getDecoratedPath) const {
  if (!m_Symbol)
    return;
  ostream << "\n#<Pattern: "
          << m_Symbol->getSymbolContainer()->getWildcardPatternAsString()
          << ">\n";
  m_Symbol->getSymbolContainer()->dump(ostream, getDecoratedPath);
}
