//===- SymbolContainer.cpp-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Script/SymbolContainer.h"
#include "eld/Script/StrToken.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "llvm/ADT/StringRef.h"

using namespace eld;

SymbolContainer::SymbolContainer(StrToken &token) : m_StrToken(token) {}

void SymbolContainer::addResolveInfo(const ResolveInfo *info) {
  m_MatchedSymbols.push_back(info);
}

void SymbolContainer::dump(
    llvm::raw_ostream &ostream,
    std::function<std::string(const Input *)> getDecoratedPath) const {
  for (const ResolveInfo *sym : m_MatchedSymbols)
    ostream << "#\t" << sym->name() << "\t"
            << getDecoratedPath(sym->resolvedOrigin()->getInput()) << "\n";
}

llvm::StringRef SymbolContainer::getWildcardPatternAsString() const {
  return m_StrToken.name();
}
