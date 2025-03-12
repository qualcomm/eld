//===- ARMEXIDXSection.h---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_READERS_ARM_EXIDX_SECTION_H
#define ELD_READERS_ARM_EXIDX_SECTION_H

#include "eld/Readers/ELFSection.h"
#include <cstdint>

namespace eld {

class DiagnosticEngine;
class RegionFragment;

struct EXIDXEntry {
  uint32_t InputOffset = -1;
  Fragment *Fragment = nullptr;
};

class ARMEXIDXSection : public ELFSection {
  llvm::SmallVector<EXIDXEntry, 0> Entries;

public:
  explicit ARMEXIDXSection(const std::string &Name, uint32_t Flag,
                           uint32_t EntSize, uint32_t Size, uint64_t PAddr)
      : ELFSection(LDFileFormat::Target, Name, Flag, EntSize, /*AddrAlign=*/0,
                   llvm::ELF::SHT_ARM_EXIDX, /*Info=*/0, /*Link=*/nullptr, Size,
                   PAddr) {}

  void addEntry(EXIDXEntry E) { Entries.push_back(E); }
  EXIDXEntry getEntry(uint32_t Offset) const;

  static bool classof(const ELFSection *S) { return S->isEXIDX(); }
};
} // namespace eld
#endif
