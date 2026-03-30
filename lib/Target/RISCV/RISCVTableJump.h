//===- RISCVTableJump.h---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_TARGET_RISCV_TABLEJUMP_H
#define ELD_TARGET_RISCV_TABLEJUMP_H

#include "eld/Fragment/TargetFragment.h"
#include "llvm/ADT/DenseMap.h"

namespace eld {

class ELFSection;
class ResolveInfo;
class RISCVLDBackend;

struct RISCVTableJumpEntry {
  int Saved = 0;
  int Index = -1;
};

// Implements the Zcmt table jump section (.riscv.jvt) used by table jump
// relaxation (cm.jt/cm.jalt).
class RISCVTableJumpFragment final : public TargetFragment {
public:
  RISCVTableJumpFragment(RISCVLDBackend &B, ELFSection *O);

  size_t size() const override;
  eld::Expected<void> emit(MemoryRegion &Mr, Module &M) override;

  void scanTableJumpEntries(ELFSection &Sec);
  void finalizeContents();

  int getCMJTEntryIndex(const ResolveInfo *Sym) const;
  int getCMJALTEntryIndex(const ResolveInfo *Sym) const;

private:
  void writeTo(uint8_t *Buf);

  static constexpr size_t MaxCMJTEntrySize = 32;
  static constexpr size_t MaxCMJALTEntrySize = 224;
  static constexpr size_t StartCMJALTEntryIdx = 32;

  RISCVLDBackend &Backend;
  llvm::DenseMap<const ResolveInfo *, RISCVTableJumpEntry> CMJTCandidates;
  llvm::DenseMap<const ResolveInfo *, RISCVTableJumpEntry> CMJALTCandidates;
  size_t ThisSize = 0;
};

} // namespace eld

#endif // ELD_TARGET_RISCV_TABLEJUMP_H
