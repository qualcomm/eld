//===- VersionScript.cpp---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Script/VersionScript.h"
#include "eld/Script/ScriptSymbol.h"
#include "eld/Script/SymbolContainer.h"
#include "eld/Support/Memory.h"
#include "eld/SymbolResolver/NamePool.h"

using namespace eld;

// -----------------------VersionScript -------------------------------
VersionScript::VersionScript(InputFile *Inp) : MInputFile(Inp) {}

VersionScriptNode *VersionScript::createVersionScriptNode() {
  MCurrentNode = eld::make<VersionScriptNode>();
  VersionScriptNodes.push_back(MCurrentNode);
  return MCurrentNode;
}

void VersionScript::setExternLanguage(eld::StrToken *Language) {
  MCurrentNode->setExternLanguage(Language);
}

void VersionScript::resetExternLanguage() {
  MCurrentNode->resetExternLanguage();
}

void VersionScript::addSymbol(eld::ScriptSymbol *S) {
  MCurrentNode->addSymbol(S);
}

void VersionScript::dump(
    llvm::raw_ostream &Ostream,
    std::function<std::string(const Input *)> GetDecoratedPath) const {
  if (VersionScriptNodes.empty() || !MInputFile)
    return;
  Ostream << "\nVersion Script file\n";
  Ostream << GetDecoratedPath(MInputFile->getInput());
  for (const VersionScriptNode *VersionNodes : VersionScriptNodes)
    VersionNodes->dump(Ostream, GetDecoratedPath);
}

// -----------------------VersionScriptNode -------------------------------
VersionScriptNode::VersionScriptNode() {}

VersionScriptBlock *VersionScriptNode::switchToGlobal() {
  if (MLocal && !MGlobal) {
    MHasErrorDuringParsing = true;
    return nullptr;
  }
  if (MHasErrorDuringParsing)
    return nullptr;
  if (MGlobal)
    return MCurrentBlock = MGlobal;
  MGlobal = eld::make<GlobalVersionScriptBlock>(this);
  MCurrentBlock = MGlobal;
  return MGlobal;
}

void VersionScriptNode::dump(
    llvm::raw_ostream &Ostream,
    std::function<std::string(const Input *)> GetDecoratedPath) const {
  if (MGlobal)
    MGlobal->dump(Ostream, GetDecoratedPath);
  if (MLocal)
    MLocal->dump(Ostream, GetDecoratedPath);
}

VersionScriptBlock *VersionScriptNode::switchToLocal() {
  if (MLocal)
    return MCurrentBlock = MLocal;
  MLocal = eld::make<LocalVersionScriptBlock>(this);
  MCurrentBlock = MLocal;
  return MLocal;
}

void VersionScriptNode::addSymbol(eld::ScriptSymbol *S) {
  if (MHasErrorDuringParsing)
    return;
  if (!MCurrentBlock)
    switchToGlobal();
  MCurrentBlock->addSymbol(S, VersionScriptLanguage);
  S->activate();
}

bool VersionScriptNode::isAnonymous() const { return (Name == nullptr); }

bool VersionScriptNode::hasDependency() const {
  return (MDependency != nullptr);
}
// --------------------------------VersionScriptBlock -------------------

void VersionScriptBlock::addSymbol(eld::ScriptSymbol *S,
                                   eld::StrToken *Language) {
  if (!Language) {
    VersionSymbol *V =
        eld::make<VersionSymbol>(VersionSymbol::VersionSymbolKind::Simple, S);
    V->setBlock(this);
    ThisSymbols.push_back(V);
    return;
  }
  VersionSymbol *V = eld::make<VersionSymbol>(
      VersionSymbol::VersionSymbolKind::Extern, S, Language);
  V->setBlock(this);
  ThisSymbols.push_back(V);
}

void VersionScriptBlock::dump(
    llvm::raw_ostream &Ostream,
    std::function<std::string(const Input *)> GetDecoratedPath) const {
  for (VersionSymbol *Sym : ThisSymbols)
    Sym->dump(Ostream, GetDecoratedPath);
}
// -----------------------GlobalVersionScriptBlock --------------------------
void GlobalVersionScriptBlock::dump(
    llvm::raw_ostream &Ostream,
    std::function<std::string(const Input *)> GetDecoratedPath) const {
  Ostream << "\nGlobal:";
  VersionScriptBlock::dump(Ostream, GetDecoratedPath);
}

// -----------------------LocalVersionScriptBlock --------------------------
void LocalVersionScriptBlock::dump(
    llvm::raw_ostream &Ostream,
    std::function<std::string(const Input *)> GetDecoratedPath) const {
  Ostream << "\nLocal:";
  VersionScriptBlock::dump(Ostream, GetDecoratedPath);
}

// -----------------------VersionScriptSymbol -------------------------------
bool VersionSymbol::isGlobal() const { return VersionScriptBlock->isGlobal(); }

bool VersionSymbol::isLocal() const { return VersionScriptBlock->isLocal(); }

void VersionSymbol::dump(
    llvm::raw_ostream &Ostream,
    std::function<std::string(const Input *)> GetDecoratedPath) const {
  if (!ThisSymbol)
    return;
  Ostream << "\n#<Pattern: "
          << ThisSymbol->getSymbolContainer()->getWildcardPatternAsString()
          << ">\n";
  ThisSymbol->getSymbolContainer()->dump(Ostream, GetDecoratedPath);
}

bool VersionSymbol::matched(const ResolveInfo &R, const NamePool &NP) const {
  auto *VB = getBlock();
  VersionScriptNode *VN = VB->getNode();
  if (!VN->isAnonymous()) {
    llvm::StringRef NodeVerName = VN->getName();
    llvm::StringRef SymVerName = R.getVersionName();
    bool shouldAllowUnverSymbol = true;
    if (SymVerName.empty()) {
      if (NP.findInfo(R.getName().str() + "@" + NodeVerName.str()) ||
          NP.findInfo(R.getName().str() + "@@" + NodeVerName.str()))
        shouldAllowUnverSymbol = false;
    }
    if (SymVerName.empty()) {
      if (!shouldAllowUnverSymbol)
        return false;
    } else {
      if (NodeVerName != SymVerName)
        return false;
    }
  }
  ScriptSymbol *symbolPattern = getSymbolPattern();
  return symbolPattern->matched(R);
}