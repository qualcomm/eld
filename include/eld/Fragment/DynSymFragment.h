//===- DynSymFragment.h----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_FRAGMENT_DYNSYMFRAGMENT_H
#define ELD_FRAGMENT_DYNSYMFRAGMENT_H

#include "eld/Fragment/Fragment.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include <vector>

namespace eld {

/// DynSymFragment holds the content of the .dynsym section.
class DynSymFragment : public Fragment {
public:
  DynSymFragment(ELFSection *S, const std::vector<ResolveInfo *> &DynSyms,
                 bool Is32Bits);

  static bool classof(const Fragment *F) {
    return F->getKind() == Fragment::Type::DynSym;
  }

  size_t size() const override;

  eld::Expected<void> emit(MemoryRegion &Mr, Module &M) override;

private:
  const std::vector<ResolveInfo *> &DynamicSymbols;
  bool Is32Bits = false;
};

} // namespace eld

#endif // ELD_FRAGMENT_DYNSYMFRAGMENT_H
