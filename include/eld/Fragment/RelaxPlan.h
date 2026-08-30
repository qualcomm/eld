//===- RelaxPlan.h---------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_FRAGMENT_RELAXPLAN_H
#define ELD_FRAGMENT_RELAXPLAN_H

#include "eld/Readers/Relocation.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include <cstdint>
#include <optional>
#include <vector>

namespace eld {

/// A single deferred relaxation, keyed to the relocation's original
/// (pre-relaxation) offset.
struct RelaxEdit {
  RelaxEdit() = default;
  RelaxEdit(Relocation *Reloc, uint32_t OrigOffset, uint32_t DeleteOffset,
            uint32_t DeleteBytes, uint32_t NopBytes = 0,
            std::optional<Relocation::Type> NewType = std::nullopt,
            std::optional<Relocation::DWord> NewInstr = std::nullopt,
            uint8_t NewInstrSize = 0,
            std::optional<Relocation::Address> NewAddend = std::nullopt)
      : Reloc(Reloc), OrigOffset(OrigOffset), DeleteOffset(DeleteOffset),
        DeleteBytes(DeleteBytes), NopBytes(NopBytes), NewType(NewType),
        NewInstr(NewInstr), NewInstrSize(NewInstrSize), NewAddend(NewAddend) {}

  Relocation *Reloc = nullptr;
  uint32_t OrigOffset = 0;

  uint32_t DeleteOffset = 0; // 0 DeleteBytes == pure rewrite, no deletion
  uint32_t DeleteBytes = 0;
  uint32_t NopBytes = 0; // ALIGN only

  // Applied at commit; nullopt means unchanged.
  std::optional<Relocation::Type> NewType;
  std::optional<Relocation::DWord> NewInstr;
  uint8_t NewInstrSize = 0;
  std::optional<Relocation::Address> NewAddend;
  bool Active = true;
};

/// Deferred edits for one fragment, keyed by relocation.
class RelaxPlan {
public:
  RelaxPlan() = default;

  void addEdit(RelaxEdit E);
  void removeEdit(const Relocation *R);
  void clear();

  // Sum of DeleteBytes over edits with DeleteOffset < Off.
  uint32_t shiftAt(uint32_t OrigOff) const;
  uint32_t totalShift() const { return TotalShift; }

  // True if [DeleteOffset, DeleteOffset + DeleteBytes) would overlap an
  // already-recorded edit's delete range.
  bool wouldOverlap(uint32_t DeleteOffset, uint32_t DeleteBytes) const;

  RelaxEdit *find(const Relocation *R);
  llvm::ArrayRef<RelaxEdit> edits() const { return Edits; }
  bool empty() const { return ByReloc.empty(); }

private:
  void rebuildDerived();

  std::vector<RelaxEdit> Edits; // sorted by DeleteOffset; removals are lazy
  std::vector<uint32_t> PrefixShift; // shift before Edits[i]
  llvm::DenseMap<const Relocation *, unsigned> ByReloc;
  uint32_t TotalShift = 0;
};

} // namespace eld

#endif
