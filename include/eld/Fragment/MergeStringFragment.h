//===- MergeStringFragment.h-----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#ifndef ELD_FRAGMENT_MERGE_STRING_FRAGMENT_H
#define ELD_FRAGMENT_MERGE_STRING_FRAGMENT_H

#include "eld/Fragment/Fragment.h"
#include "llvm/ADT/StringRef.h"

namespace eld {

class LinkerConfig;
class DiagnosticEngine;
class MergeStringFragment;
class Module;
class OutputSectionEntry;

/// A MergeableString is a null terminated string that is part of an input merge
/// string section that may be merged with identical strings destined for the
/// same output section.

/// TODO: we can possibly reduce the size of MergeableString by 16 bytes by
/// removing String member. We can get the string by using the input offset of
/// this string and the input offset of the next string. Fragment Pointer could
/// also be removed.
struct MergeableString {
  MergeStringFragment *Fragment;
  llvm::StringRef String;
  uint32_t InputOffset;
  uint32_t OutputOffset;
  bool Exclude;
  MergeableString(MergeStringFragment *F, llvm::StringRef S, uint32_t I,
                  uint32_t O, bool E)
      : Fragment(F), String(S), InputOffset(I), OutputOffset(O), Exclude(E) {}
  void exclude() { Exclude = true; }
  uint64_t size() const { return String.size(); }
  bool hasOutputOffset() const {
    return OutputOffset != std::numeric_limits<uint32_t>::max();
  }
  bool isAlloc() const;
};

/// MergeStringFrgament is a Fragment that manages MergeableStrings of a
/// LDFileFormat::MergeStr input section.
class MergeStringFragment : public Fragment {
  std::vector<MergeableString *> Strings;

public:
  MergeStringFragment(ELFSection *O);

  ~MergeStringFragment() {}

  /// merge String S into output section O, or globally in M if it is
  /// a non-alloc string. Return the string that S was merged with or nullptr if
  /// S is unique.
  static MergeableString *mergeStrings(MergeableString *S,
                                       OutputSectionEntry *O, Module &M);

  bool readStrings(LinkerConfig &Config);

  static bool classof(const Fragment *F) {
    return F->getKind() == Fragment::MergeString;
  }

  size_t size() const override;

  bool isZeroSizedFrag() const override { return Strings.empty(); }

  eld::Expected<void> emit(MemoryRegion &Region, Module &M) override;

  std::vector<MergeableString *> &getStrings() { return Strings; }

  const std::vector<MergeableString *> &getStrings() const { return Strings; }

  MergeableString *findString(uint64_t Offset) const;

  void copyData(void *Dest, uint64_t Bytes, uint64_t Offset) const;

  void setOffset(uint32_t Offset) override;

private:
  /// After this fragment has been given an output offset this function will be
  /// called and set the output offset of every string owned by this fragment
  void assignOutputOffsets();
};

} // namespace eld

#endif
