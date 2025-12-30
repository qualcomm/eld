//===- GNUVerDefFragment.h----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===-------------------------------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===-------------------------------------------------------------------------===//
#ifndef ELD_FRAGMENT_GNUVERDEFFRAGMENT_H
#define ELD_FRAGMENT_GNUVERDEFFRAGMENT_H

#include "eld/Fragment/Fragment.h"
#include <cstdint>
#include <vector>

namespace eld {
class DiagnosticEngine;
class ELFFileFormat;

/// .gnu.version_d(SHT_GNU_verdef) section contains the symbol version
/// definitions for the symbols that are defined by the module.
class GNUVerDefFragment : public Fragment {
public:
  GNUVerDefFragment(ELFSection *S);

  static bool classof(const Fragment *F) {
    return F->getKind() == Fragment::Type::GNUVerDef;
  }

  template <class ELFT>
  eld::Expected<void> computeVersionDefs(Module &M, ELFFileFormat *FileFormat,
                                         DiagnosticEngine &DE);

  eld::Expected<void> emit(MemoryRegion &Mr, Module &M) override;

  size_t size() const override;

  template <class ELFT> eld::Expected<void> emitImpl(uint8_t *Buf, Module &M);

  size_t defCount() const { return VersionDefs.size(); }

public:
  struct VerDefInfo {
    uint16_t VersionID = 0;
    uint32_t VersionNameOffset = 0;
    uint32_t VersionNameHash = 0;
  };

protected:
  std::vector<VerDefInfo> VersionDefs;
  size_t VerDefEntrySize = 0;
  size_t VerdAuxEntrySize = 0;
};

} // namespace eld

#endif // ELD_FRAGMENT_GNUVERDEFFRAGMENT_H
