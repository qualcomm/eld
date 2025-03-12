//===- HexagonAttributeFragment.h------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#include "eld/Fragment/TargetFragment.h"
#include "llvm/ADT/DenseMap.h"

#ifndef ELD_HEXAGON_ATTRIBUTE_FRAGMENT_H
#define ELD_HEXAGON_ATTRIBUTE_FRAGMENT_H

namespace eld {
class ObjectFile;
class HexagonAttributeFragment : public TargetFragment {
public:
  HexagonAttributeFragment(ELFSection *O);

  ~HexagonAttributeFragment();

  size_t size() const override;

  llvm::ArrayRef<uint8_t> getContent() const override;

  static bool classof(const Fragment *F);

  eld::Expected<void> emit(MemoryRegion &R, Module &M) override;

  bool update(ELFSection &S, DiagnosticEngine &E, ObjectFile &O,
              bool AddFeatures);

private:
  static constexpr llvm::StringRef Vendor = "hexagon";
  llvm::DenseMap<unsigned, unsigned> Attrs;
  // format version + vendor string + file tag
  uint64_t Size = 5 + Vendor.size() + 1 + 5;
};
} // namespace eld

#endif
