//===- RISCVILUT.h--------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_TARGET_RISCV_ILUT_H
#define ELD_TARGET_RISCV_ILUT_H

#include "eld/Fragment/TargetFragment.h"
#include "llvm/ADT/DenseMap.h"
#include <map>
#include <vector>

namespace eld {

class ELFSection;
class Fragment;
class RISCVLDBackend;

// Materializes the Xqccmi instruction lookup table and records the source
// instruction ranges that can be replaced by qc.cm.ilut.
class RISCVILUTFragment final : public TargetFragment {
public:
  RISCVILUTFragment(RISCVLDBackend &B, ELFSection *O);

  size_t size() const override;
  eld::Expected<void> emit(MemoryRegion &Mr, Module &M) override;
  void dump(llvm::raw_ostream &OS) override;

  void scanCodeSection(ELFSection &Sec);
  void finalizeContents();

  bool hasEntries() const { return !Entries.empty(); }
  unsigned getDEC() const { return DEC; }

  // Return the ILUT index and source size for a selected source range.
  bool getRelaxation(const Fragment *F, uint64_t Offset, unsigned &Index,
                     unsigned &SourceSize) const;

  // Rewrite all selected source ranges. Ranges are applied from high to low
  // offsets within each fragment because deletion shifts later references.
  bool applyRelaxations();

private:
  struct Entry {
    std::vector<uint8_t> Bytes;
    unsigned Size = 0;
  };

  struct Use {
    const Fragment *F = nullptr;
    uint64_t Offset = 0;
    unsigned SourceSize = 0;
    unsigned EntryIndex = 0;
  };

  void writeTo(uint8_t *Buf);

  std::vector<Entry> Entries;
  std::map<std::pair<const Fragment *, uint64_t>, Use> Uses;
  unsigned DEC = 0;
  size_t ThisSize = 0;
  bool Applied = false;
};

} // namespace eld

#endif // ELD_TARGET_RISCV_ILUT_H
