//===- MergeDataFragment.cpp-----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Fragment/MergeDataFragment.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Core/Module.h"
#include "eld/Object/OutputSectionEntry.h"
#include "eld/Readers/ELFSection.h"
#include <algorithm>
#include <cstring>

using namespace eld;

MergeDataFragment::MergeDataFragment(ELFSection *O)
    : Fragment(Fragment::MergeData, O, O->getAddrAlign()) {}

MergeableConstant &
MergeDataFragment::getOrAddMergedConstant(MergeableConstant *C,
                                          OutputSectionEntry *O) {
  MergeableConstant *MergedConstant = O->getMergedConstant(C);
  if (MergedConstant) {
    // C is a duplicate of an existing canonical constant.
    // Mark it excluded from emission but still remember the input constant so
    // relocation fixups can map back from input offsets.
    C->setExcluded();
    O->addConstant(C);
    return *MergedConstant;
  }

  // No reusable canonical constant exists. If an older canonical entry has the
  // same bytes but weaker alignment, retire it and promote C as canonical.
  if (MergeableConstant *Existing = O->findConstant(C))
    Existing->setExcluded();
  O->addConstant(C);
  return *C;
}

bool MergeDataFragment::readConstants(LinkerConfig &Config) {
  ELFSection *S = getOwningSection();
  llvm::StringRef Contents = S->getContents();
  if (Contents.empty())
    return true;

  const uint32_t EntrySize = S->getEntSize();
  if (EntrySize == 0 || (Contents.size() % EntrySize) != 0) {
    Config.raise(Diag::unexpected_section_size)
        << S->size() << S->name().str()
        << S->getInputFile()->getInput()->decoratedPath();
    return false;
  }

  for (uint32_t Off = 0; Off < Contents.size(); Off += EntrySize) {
    llvm::StringRef Data = Contents.slice(Off, Off + EntrySize);
    // OutputOffset is assigned after layout; initialize as "not assigned".
    Constants.emplace_back(this, Data, Off,
                           std::numeric_limits<uint32_t>::max(), false);
  }
  return true;
}

size_t MergeDataFragment::size() const {
  size_t Size = 0;
  for (const MergeableConstant &C : Constants)
    Size += C.Exclude ? 0 : C.getSize();
  return Size;
}

eld::Expected<void> MergeDataFragment::emit(MemoryRegion &Region, Module &M) {
  uint64_t Size = size();
  if (!Size)
    return {};

  [[maybe_unused]] uint64_t Emitted = 0;
  uint8_t *Buf = Region.begin() + getOffset(M.getConfig().getDiagEngine());
  for (const MergeableConstant &C : Constants) {
    if (C.Exclude)
      continue;
    std::memcpy(Buf, C.Data.begin(), C.getSize());
    Emitted += C.getSize();
    Buf += C.getSize();
  }
  assert(Emitted == Size);
  return {};
}

const MergeableConstant *
MergeDataFragment::getConstantAtOffset(uint64_t Offset) const {
  if (Offset >= getOwningSection()->size())
    return nullptr;
  uint32_t EntrySize = getOwningSection()->getEntSize();
  if (EntrySize == 0)
    return nullptr;
  return &Constants[Offset / EntrySize];
}

void MergeDataFragment::copyData(void *Dest, uint64_t Bytes,
                                 uint64_t Offset) const {
  const MergeableConstant *C = getConstantAtOffset(Offset);
  assert(C);
  uint64_t OffsetInConst = Offset - C->InputOffset;
  uint64_t Size = std::min((uint64_t)Bytes, C->getSize() - OffsetInConst);
  std::memcpy(Dest, C->Data.begin() + OffsetInConst, Size);
}

void MergeDataFragment::setOffset(uint32_t Offset) {
  Fragment::setOffset(Offset);
  assignOutputOffsets();
}

void MergeDataFragment::assignOutputOffsets() {
  assert(hasOffset());
  uint32_t Offset = getOffset();
  for (MergeableConstant &C : Constants) {
    if (C.Exclude)
      continue;
    // Constants are emitted contiguously in input order for this fragment.
    C.OutputOffset = Offset;
    Offset += C.getSize();
  }
}

uint32_t MergeableConstant::getAlignment() const {
  return Fragment->alignment();
}
