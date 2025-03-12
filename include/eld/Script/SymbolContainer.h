//===- SymbolContainer.h---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_OBJECT_SYMBOLCONTAINER_H
#define ELD_OBJECT_SYMBOLCONTAINER_H

#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/raw_ostream.h"

namespace eld {

class StrToken;
class ResolveInfo;
class Input;

class SymbolContainer {
public:
  explicit SymbolContainer(StrToken &token);

  void addResolveInfo(const ResolveInfo *info);

  llvm::StringRef getWildcardPatternAsString() const;

  bool isEmpty() { return m_MatchedSymbols.empty(); }

  void dump(llvm::raw_ostream &ostream,
            std::function<std::string(const Input *)> getDecoratedPath) const;

private:
  const StrToken &m_StrToken;
  llvm::SmallVector<const ResolveInfo *> m_MatchedSymbols;
};

} // end namespace eld

#endif