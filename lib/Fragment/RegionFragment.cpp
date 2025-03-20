//===- RegionFragment.cpp--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

//===- RegionFragment.cpp -------------------------------------------------===//
//===----------------------------------------------------------------------===//

#include "eld/Fragment/RegionFragment.h"
#include "eld/Core/Module.h"
#include "llvm/Support/Endian.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// RegionFragment
//===----------------------------------------------------------------------===//
RegionFragment::RegionFragment(llvm::StringRef PRegion, ELFSection *O,
                               Fragment::Type K, uint32_t Align)
    : Fragment(K, O, Align), FragmentRegion(std::move(PRegion)) {}

RegionFragment::~RegionFragment() {}

size_t RegionFragment::size() const {
  if (isNull())
    return 0;
  return FragmentRegion.size();
}

eld::Expected<void> RegionFragment::emit(MemoryRegion &Mr, Module &M) {
  if (!size())
    return {};

  uint8_t *Out = Mr.begin() + getOffset(M.getConfig().getDiagEngine());
  memcpy(Out, FragmentRegion.begin(), size());
  // Fill with padding value as requested by the user.
  if (this->paddingSize() == 0)
    return {};
  Out = Mr.begin() + getOffset(M.getConfig().getDiagEngine()) -
        this->paddingSize();
  std::optional<uint64_t> OptPaddingValue = M.getFragmentPaddingValue(this);
  if (!OptPaddingValue)
    return {};
  uint64_t PaddingValue = *OptPaddingValue;
  uint32_t ValueSize = Fragment::getPaddingValueSize(PaddingValue);
  if (PaddingValue == 0)
    return {};
  uint64_t FillPaddingSize = this->paddingSize();
  // pattern fillment
  size_t NumTiles = FillPaddingSize / ValueSize;
  for (size_t I = 0; I != NumTiles; ++I) {
    switch (ValueSize) {
    case 1:
      llvm::support::endian::write<uint8_t, llvm::endianness::big>(
          Out + I * ValueSize, PaddingValue);
      break;
    case 2:
      llvm::support::endian::write16be(Out + I * ValueSize, PaddingValue);
      break;
    case 4:
      llvm::support::endian::write32be(Out + I * ValueSize, PaddingValue);
      break;
    case 8:
      llvm::support::endian::write64be(Out + I * ValueSize, PaddingValue);
      break;
    }
  }
  return {};
}

void RegionFragment::copyData(void *PDest, uint32_t PNBytes,
                              uint64_t POffset) const {
  std::memcpy(PDest, this->getRegion().begin() + POffset, PNBytes);
}

template <typename T> bool RegionFragment::setContent(T Val) {
  if (size() < sizeof(T))
    return false;
  std::memcpy((void *)FragmentRegion.data(), &Val, sizeof(T));
  return true;
}

template bool RegionFragment::setContent<uint32_t>(uint32_t);
template bool RegionFragment::setContent<uint64_t>(uint64_t);
