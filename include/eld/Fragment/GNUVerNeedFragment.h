//===- GNUVerNeedFragment.h----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===--------------------------------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===--------------------------------------------------------------------------===//

#ifndef ELD_FRAGMENT_GNUVERNEEDFRAGMENT_H
#define ELD_FRAGMENT_GNUVERNEEDFRAGMENT_H

#include "eld/Fragment/Fragment.h"
#include "llvm/ADT/StringRef.h"

namespace eld {
class DiagnosticEngine;
class ELFSection;
class InputFile;
class ELFFileFormat;

/** \class VerNeedFragment
 *  \brief VerNeedFragment is a kind of Fragment containing input memory region
 */
// Region fragment expression
class GNUVerNeedFragment : public Fragment {
public:
  GNUVerNeedFragment(ELFSection *S);

  static bool classof(const Fragment *F) {
    return F->getKind() == Fragment::Type::GNUVerNeed;
  }

  template <class ELFT>
  eld::Expected<void>
  computeVersionNeeds(const std::vector<InputFile *> &DynamicObjectFiles,
                      ELFFileFormat *FileFormat, DiagnosticEngine &DE);

  eld::Expected<void> emit(MemoryRegion &Mr, Module &M) override;

  size_t size() const override;

  template <class ELFT>
  eld::Expected<void> emitImpl(uint8_t *Buf, Module &M);

  size_t needCount() { return VersionNeeds.size(); }

public:
  struct VernAuxInfo {
    uint32_t VersionNameOffset = 0;
    uint32_t VersionID = 0;
    uint32_t VersionNameHash = 0;
  };

  struct VerNeedInfo {
    uint32_t SONameOffset = 0;
    std::vector<VernAuxInfo> Vernauxs;
  };

protected:
  std::vector<VerNeedInfo> VersionNeeds;
  size_t VerNeedEntrySize = 0;
  size_t VernAuxEntrySize = 0;
};

} // namespace eld

#endif
