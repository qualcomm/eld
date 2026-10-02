//===- MergeDataFragment.h-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#ifndef ELD_FRAGMENT_MERGEDATAFRAGMENT_H
#define ELD_FRAGMENT_MERGEDATAFRAGMENT_H

#include "eld/Fragment/Fragment.h"
#include "llvm/ADT/StringRef.h"
#include <limits>

namespace eld {

class LinkerConfig;
class MergeDataFragment;
class OutputSectionEntry;

/// A mergeable fixed-size entry from a SHF_MERGE (non SHF_STRINGS) section.
struct MergeableConstant {
  MergeDataFragment *Fragment;
  llvm::StringRef Data;
  uint32_t InputOffset;
  uint32_t OutputOffset;
  bool Exclude;

  MergeableConstant(MergeDataFragment *F, llvm::StringRef D, uint32_t I,
                    uint32_t O, bool E)
      : Fragment(F), Data(D), InputOffset(I), OutputOffset(O), Exclude(E) {}

  MergeableConstant(const MergeableConstant &) = delete;
  MergeableConstant &operator=(const MergeableConstant &) = delete;
  MergeableConstant(MergeableConstant &&) = default;
  MergeableConstant &operator=(MergeableConstant &&) = default;

  void setExcluded() { Exclude = true; }
  uint64_t getSize() const { return Data.size(); }
  uint32_t getAlignment() const;
  bool hasOutputOffset() const {
    return OutputOffset != std::numeric_limits<uint32_t>::max();
  }
};

class MergeDataFragment : public Fragment {
  llvm::SmallVector<MergeableConstant> Constants;

public:
  explicit MergeDataFragment(ELFSection *O);

  ~MergeDataFragment() {}

  static MergeableConstant &getOrAddMergedConstant(MergeableConstant *C,
                                                   OutputSectionEntry *O);

  bool readConstants(LinkerConfig &Config);

  static bool classof(const Fragment *F) {
    return F->getKind() == Fragment::MergeData;
  }

  size_t size() const override;

  bool isZeroSizedFrag() const override { return Constants.empty(); }

  eld::Expected<void> emit(MemoryRegion &Region, Module &M) override;

  llvm::SmallVectorImpl<MergeableConstant> &getConstants() { return Constants; }

  const llvm::SmallVectorImpl<MergeableConstant> &getConstants() const {
    return Constants;
  }

  const MergeableConstant *getConstantAtOffset(uint64_t Offset) const;

  void copyData(void *Dest, uint64_t Bytes, uint64_t Offset) const;

  void setOffset(uint32_t Offset) override;

private:
  void assignOutputOffsets();
};

} // namespace eld

#endif
