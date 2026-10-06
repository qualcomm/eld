//===- GNUPropertyFragment.h-----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_FRAGMENT_GNU_PROPERTY_FRAGMENT_H
#define ELD_FRAGMENT_GNU_PROPERTY_FRAGMENT_H

#include "eld/Fragment/TargetFragment.h"
#include "eld/Readers/ELFSection.h"
#include <cstdint>
#include <string>

namespace eld {

class GNULDBackend;

class GNUPropertyFragment : public TargetFragment {
public:
  GNUPropertyFragment(ELFSection *O, uint32_t FeatureAndType, bool Is64);

  virtual ~GNUPropertyFragment();

  virtual const std::string name() const override;

  /// 0x20 for ELF64, 0x1c for ELF32, 0 when no feature bit survives.
  virtual size_t size() const override;

  static bool classof(const Fragment *F) {
    const auto *TF = llvm::dyn_cast<TargetFragment>(F);
    return TF &&
           TF->targetFragmentKind() == TargetFragment::Kind::NoteGNUProperty;
  }

  static bool classof(const GNUPropertyFragment *) { return true; }

  virtual eld::Expected<void> emit(MemoryRegion &mr, Module &M) override;

  bool updateInfo(GNULDBackend *G) override;

  virtual void dump(llvm::raw_ostream &OS) override;

  bool updateInfo(uint32_t Features);

  void resetFlag(uint32_t F) { FeatureSet &= ~F; }

private:
  /// Size of the note descriptor: pr_type + pr_datasz + data, padded to the
  /// property alignment (8 for ELF64, 4 for ELF32).
  uint32_t descSize() const { return Is64 ? 16 : 12; }

  uint32_t FeatureSet = 0;
  uint32_t FeatureAndType;
  bool Is64;
};

} // namespace eld

#endif
