//===- MergeStringFragment.cpp---------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#include "eld/Fragment/MergeStringFragment.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Core/Module.h"
#include "eld/LayoutMap/LayoutPrinter.h"
#include "eld/Readers/ELFSection.h"

using namespace eld;

MergeStringFragment::MergeStringFragment(ELFSection *O)
    : Fragment(Fragment::MergeString, O, 1) {}

MergeableString *MergeStringFragment::mergeStrings(MergeableString *S,
                                                   OutputSectionEntry *O,
                                                   Module &Module) {
  bool GlobalMerge =
      Module.getConfig().options().shouldGlobalStringMerge() && !S->isAlloc();
  MergeableString *MergedString =
      GlobalMerge ? Module.getMergedNonAllocString(S) : O->getMergedString(S);
  GlobalMerge ? Module.addNonAllocString(S) : O->addString(S);
  if (!MergedString)
    return nullptr;
  S->exclude();
  return MergedString;
}

bool MergeStringFragment::readStrings(LinkerConfig &Config) {
  llvm::StringRef Contents = getOwningSection()->getContents();
  if (Contents.empty())
    return true;
  uint64_t Offset = 0;
  ELFSection *S = getOwningSection();
  while (!Contents.empty()) {
    size_t End = Contents.find('\0');
    if (End == llvm::StringRef::npos) {
      Config.raise(Diag::string_not_null_terminated)
          << S->getInputFile()->getInput()->decoratedPath()
          << S->getDecoratedName(Config.options()) << llvm::utohexstr(Offset);
      return false;
    }
    // account for the null character
    uint64_t Size = End + 1;
    llvm::StringRef String = Contents.slice(0, Size);
    Strings.push_back(make<MergeableString>(
        this, String, Offset, std::numeric_limits<uint32_t>::max(), false));
    Contents = Contents.drop_front(Size);
    if (Config.getPrinter()->isVerbose()) {
      Config.raise(Diag::splitting_merge_string_section)
          << S->getInputFile()->getInput()->decoratedPath()
          << S->getDecoratedName(Config.options()) << llvm::utohexstr(Offset)
          << String.data() << 1;
    }
    Offset += Size;
  }
  assert(size() == getOwningSection()->size());
  return true;
}

size_t MergeStringFragment::size() const {
  size_t Size = 0;
  for (const MergeableString *S : Strings)
    Size += S->Exclude ? 0 : S->size();
  return Size;
}

MergeableString *MergeStringFragment::findString(uint64_t Offset) const {
  /// FIXME: This case should ideally assert or error rather than return nullptr
  if (Offset >= getOwningSection()->size())
    return nullptr;
  return llvm::partition_point(Strings, [Offset](MergeableString *S) {
    return S->InputOffset <= Offset;
  })[-1];
}

eld::Expected<void> MergeStringFragment::emit(MemoryRegion &Region, Module &M) {
  uint64_t Size = size();
  if (!Size)
    return {};
  [[maybe_unused]] uint64_t Emitted = 0;
  uint8_t *Buf = Region.begin() + getOffset(M.getConfig().getDiagEngine());
  for (MergeableString *S : Strings) {
    if (S->Exclude)
      continue;
    std::memcpy(Buf, S->String.begin(), S->size());
    Emitted += S->size();
    Buf += S->size();
  }
  assert(Emitted == Size);
  return {};
}

void MergeStringFragment::copyData(void *Dest, uint64_t Bytes,
                                   uint64_t Offset) const {
  MergeableString *S = findString(Offset);
  assert(S);
  uint64_t OffsetInString = Offset - S->InputOffset;
  uint64_t Size = std::min((uint64_t)Bytes, S->size() - OffsetInString);
  std::memcpy(Dest, S->String.begin() + OffsetInString, Size);
}

void MergeStringFragment::setOffset(uint32_t Offset) {
  Fragment::setOffset(Offset);
  assignOutputOffsets();
}

void MergeStringFragment::assignOutputOffsets() {
  assert(hasOffset());
  uint32_t Offset = getOffset();
  for (MergeableString *S : Strings) {
    if (S->Exclude)
      continue;
    S->OutputOffset = Offset;
    Offset += S->size();
  }
}

bool MergeableString::isAlloc() const {
  return Fragment->getOwningSection()->isAlloc();
}