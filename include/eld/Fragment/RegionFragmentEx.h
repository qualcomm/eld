//===- RegionFragmentEx.h--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_FRAGMENT_REGIONFRAGMENTEX_H
#define ELD_FRAGMENT_REGIONFRAGMENTEX_H

#include "eld/Fragment/Fragment.h"
#include "eld/Fragment/RelaxPlan.h"
#include "eld/Readers/Relocation.h"
#include "llvm/ADT/StringRef.h"
#include <vector>

namespace eld {
class LinkerConfig;

/** \class RegionFragmentEx
 *  \brief RegionFragmentEx is a kind of Fragment containing input memory region
 *         that has capability of deleting, inserting, replacing and aligning.
 */
class RegionFragmentEx : public Fragment {
public:
  RegionFragmentEx(const char *Data, size_t Sz, ELFSection *O = nullptr,
                   uint32_t Align = 1);

  ~RegionFragmentEx();

  static bool classof(const Fragment *F) {
    return F->getKind() == Fragment::RegionFragmentEx;
  }

  const llvm::StringRef getRegion() const {
    return llvm::StringRef(Data, Size);
  }
  llvm::StringRef getRegion() { return llvm::StringRef(Data, Size); }

  static bool classof(const RegionFragmentEx *) { return true; }

  bool replaceInstruction(uint32_t Offset, Relocation *Reloc, uint8_t *Instr,
                          uint8_t Size);

  void addRequiredNops(uint32_t Offset, uint32_t NumNopsToAdd);

  size_t size() const override;

  virtual eld::Expected<void> emit(MemoryRegion &Mr, Module &M) override;

  void copyData(void *PDest, uint32_t PNBytes, uint64_t POffset) const;

  virtual void addSymbol(ResolveInfo *R) override;

  // Records a pending deletion; nothing is written to the buffer until
  // commitRelaxEdits(). NopBytes is ALIGN-only, for reporting if undone.
  void recordDelete(Relocation *Reloc, uint32_t DeleteOffset,
                    uint32_t DeleteBytes, uint32_t NopBytes = 0);

  // Attaches the relocation-type change (and, if the instruction is being
  // rewritten rather than fully deleted, the new encoding) to the edit
  // already recorded for Reloc via recordDelete(). InstrOffset is the
  // rewritten instruction's own offset, not the deletion's.
  void attachDecision(Relocation *Reloc, Relocation::Type NewType,
                      std::optional<Relocation::DWord> NewInstr = std::nullopt,
                      uint32_t InstrOffset = 0, uint8_t NewInstrSize = 0);

  // Records a pending in-place rewrite with no deletion (e.g. a GP-relative
  // operand patch).
  void
  recordRewrite(Relocation *Reloc, uint32_t InstrOffset,
                Relocation::Type NewType, Relocation::DWord NewInstr,
                uint8_t NewInstrSize,
                std::optional<Relocation::Address> NewAddend = std::nullopt);

  // Abandons the edit recorded for Reloc, if any. Nothing to restore --
  // nothing was ever applied.
  void dropEdit(const Relocation *Reloc);

  RelaxEdit *pendingEdit(const Relocation *Reloc) { return Plan.find(Reloc); }

  // Maps an original (pre-relaxation) offset to its current output-relative
  // offset, given whatever deletions are pending.
  uint32_t mapOffset(uint32_t OrigOff) const {
    return OrigOff - Plan.shiftAt(OrigOff);
  }

  // Applies every pending edit and compacts the buffer. No-op if empty.
  void commitRelaxEdits();

protected:
  std::vector<ResolveInfo *> Symbols;
  const char *Data;
  size_t Size;

private:
  RelaxPlan Plan;
};

} // namespace eld

#endif
