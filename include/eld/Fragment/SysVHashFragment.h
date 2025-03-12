//===- SysVHashFragment.h--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#ifndef ELD_FRAGMENT_SYSVHASHFRAGMENT_H
#define ELD_FRAGMENT_SYSVHASHFRAGMENT_H

#include "eld/Fragment/TargetFragment.h"
#include "llvm/ADT/ArrayRef.h"
#include <string>
#include <vector>

namespace eld {

class LinkerConfig;
class ResolveInfo;

template <class ELFT> class SysVHashFragment : public TargetFragment {
public:
  SysVHashFragment(ELFSection *O, std::vector<ResolveInfo *> &R);

  virtual ~SysVHashFragment();

  /// name - name of this stub
  virtual const std::string name() const override;

  virtual size_t size() const override;

  static bool classof(const Fragment *F) {
    return F->getKind() == Fragment::Target;
  }

  static bool classof(const SysVHashFragment *) { return true; }

  virtual eld::Expected<void> emit(MemoryRegion &mr, Module &M) override;

protected:
  std::vector<ResolveInfo *> &DynamicSymbols;
};

} // namespace eld

#endif
