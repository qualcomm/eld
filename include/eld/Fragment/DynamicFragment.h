//===- DynamicFragment.h---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_FRAGMENT_DYNAMICFRAGMENT_H
#define ELD_FRAGMENT_DYNAMICFRAGMENT_H

#include "eld/Fragment/Fragment.h"

namespace eld {

class ELFDynamic;
class ELFSection;

/// DynamicFragment owns the .dynamic section content.
/// It delegates size() and emit() to ELFDynamic.
class DynamicFragment : public Fragment {
public:
  DynamicFragment(ELFSection *S, ELFDynamic &Dynamic, uint32_t Align = 1);

  static bool classof(const Fragment *F) {
    return F->getKind() == Fragment::Type::Dynamic;
  }

  ELFDynamic &getDynamic() const { return m_Dynamic; }

  size_t size() const override;
  eld::Expected<void> emit(MemoryRegion &Mr, Module &M) override;

private:
  ELFDynamic &m_Dynamic;
};

} // namespace eld

#endif
