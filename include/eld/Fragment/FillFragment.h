//===- FillFragment.h------------------------------------------------------===//
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
#ifndef ELD_FRAGMENT_FILLFRAGMENT_H
#define ELD_FRAGMENT_FILLFRAGMENT_H

#include "eld/Fragment/Fragment.h"

namespace eld {

class ELFSection;
class LinkerConfig;

class FillFragment : public Fragment {
public:
  FillFragment(Module &M, uint64_t PValue, size_t PSize,
               ELFSection *O = nullptr, size_t PAlignment = 1);

  static bool classof(const Fragment *F) {
    return F->getKind() == Fragment::Fillment;
  }

  static bool classof(const FillFragment *) { return true; }

  size_t size() const override;

  virtual eld::Expected<void> emit(MemoryRegion &Mr, Module &M) override;

private:
  /// ThisSize - The number of bytes to insert.
  size_t ThisSize;
};

} // namespace eld

#endif
