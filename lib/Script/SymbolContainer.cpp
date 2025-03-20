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

SymbolContainer::SymbolContainer(StrToken &Token) : MStrToken(Token) {}

void SymbolContainer::addResolveInfo(const ResolveInfo *Info) {
  MMatchedSymbols.push_back(Info);
}

void SymbolContainer::dump(
    llvm::raw_ostream &Ostream,
    std::function<std::string(const Input *)> GetDecoratedPath) const {
  for (const ResolveInfo *Sym : MMatchedSymbols)
    Ostream << "#\t" << Sym->name() << "\t"
            << GetDecoratedPath(Sym->resolvedOrigin()->getInput()) << "\n";
}

llvm::StringRef SymbolContainer::getWildcardPatternAsString() const {
  return MStrToken.name();
}
