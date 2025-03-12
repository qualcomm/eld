//===- RegionFragment.h----------------------------------------------------===//
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

#ifndef ELD_FRAGMENT_REGIONFRAGMENT_H
#define ELD_FRAGMENT_REGIONFRAGMENT_H

#include "eld/Fragment/Fragment.h"
#include "llvm/ADT/StringRef.h"

namespace eld {
class LinkerConfig;

/** \class RegionFragment
 *  \brief RegionFragment is a kind of Fragment containing input memory region
 */
// Region fragment expression
class RegionFragment : public Fragment {
public:
  RegionFragment(llvm::StringRef pRegion, ELFSection *O, Fragment::Type T,
                 uint32_t Align = 1);

  ~RegionFragment();

  const llvm::StringRef getRegion() const { return m_Region; }
  llvm::StringRef getRegion() { return m_Region; }

  static bool classof(const Fragment *F) {
    return F->getKind() == Fragment::Region;
  }

  void setRegion(llvm::StringRef Region) { m_Region = Region; }

  static bool classof(const RegionFragment *) { return true; }

  template <typename T> bool setContent(T val);

  size_t size() const override;

  virtual eld::Expected<void> emit(MemoryRegion &mr, Module &M) override;

  virtual void copyData(void *pDest, uint32_t pNBytes, uint64_t pOffset) const;

protected:
  llvm::StringRef m_Region;
};

} // namespace eld

#endif
