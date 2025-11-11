//===- GNUVerSymFragment.h----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef ELD_FRAGMENT_GNUVERSYMFRAGMENT_H
#define ELD_FRAGMENT_GNUVERSYMFRAGMENT_H

#include "eld/Fragment/Fragment.h"
#include "llvm/ADT/StringRef.h"
#include <vector>

namespace eld {
class ELFSEction;

/** \class VerSymFragment
 *  \brief VerSymFragment is a kind of Fragment containing input memory region
 */
// Region fragment expression
class GNUVerSymFragment : public Fragment {
public:
  GNUVerSymFragment(ELFSection *S, const std::vector<ResolveInfo*> &DynSyms);

  static bool classof(const Fragment *F) {
    return F->getKind() == Fragment::Type::GNUVerSym;
  }

  eld::Expected<void> emit(MemoryRegion &Mr, Module &M) override;

  size_t size() const override;

protected:
  const std::vector<ResolveInfo*> &DynamicSymbols;
};

} // namespace eld

#endif
