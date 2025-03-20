//===- SymbolContainer.h---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SCRIPT_SYMBOLCONTAINER_H
#define ELD_SCRIPT_SYMBOLCONTAINER_H

#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/raw_ostream.h"

namespace eld {

class StrToken;
class ResolveInfo;
class Input;

class SymbolContainer {
public:
  explicit SymbolContainer(StrToken &Token);

  void addResolveInfo(const ResolveInfo *Info);

  llvm::StringRef getWildcardPatternAsString() const;

  bool isEmpty() { return MMatchedSymbols.empty(); }

  void dump(llvm::raw_ostream &Ostream,
            std::function<std::string(const Input *)> GetDecoratedPath) const;

private:
  const StrToken &MStrToken;
  llvm::SmallVector<const ResolveInfo *> MMatchedSymbols;
};

} // end namespace eld

#endif