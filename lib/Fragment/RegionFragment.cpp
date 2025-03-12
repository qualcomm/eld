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
RegionFragment::RegionFragment(llvm::StringRef pRegion, ELFSection *O,
                               Fragment::Type K, uint32_t Align)
    : Fragment(K, O, Align), m_Region(std::move(pRegion)) {}

RegionFragment::~RegionFragment() {}

size_t RegionFragment::size() const {
  if (isNull())
    return 0;
  return m_Region.size();
}

eld::Expected<void> RegionFragment::emit(MemoryRegion &mr, Module &M) {
  if (!size())
    return {};

  uint8_t *out = mr.begin() + getOffset(M.getConfig().getDiagEngine());
  memcpy(out, m_Region.begin(), size());
  // Fill with padding value as requested by the user.
  if (this->paddingSize() == 0)
    return {};
  out = mr.begin() + getOffset(M.getConfig().getDiagEngine()) -
        this->paddingSize();
  std::optional<uint64_t> optPaddingValue = M.getFragmentPaddingValue(this);
  if (!optPaddingValue)
    return {};
  uint64_t paddingValue = *optPaddingValue;
  uint32_t valueSize = Fragment::getPaddingValueSize(paddingValue);
  if (paddingValue == 0)
    return {};
  uint64_t fillPaddingSize = this->paddingSize();
  // pattern fillment
  size_t numTiles = fillPaddingSize / valueSize;
  for (size_t i = 0; i != numTiles; ++i) {
    switch (valueSize) {
    case 1:
      llvm::support::endian::write<uint8_t, llvm::endianness::big>(
          out + i * valueSize, paddingValue);
      break;
    case 2:
      llvm::support::endian::write16be(out + i * valueSize, paddingValue);
      break;
    case 4:
      llvm::support::endian::write32be(out + i * valueSize, paddingValue);
      break;
    case 8:
      llvm::support::endian::write64be(out + i * valueSize, paddingValue);
      break;
    }
  }
  return {};
}

void RegionFragment::copyData(void *pDest, uint32_t pNBytes,
                              uint64_t pOffset) const {
  std::memcpy(pDest, this->getRegion().begin() + pOffset, pNBytes);
}

template <typename T> bool RegionFragment::setContent(T Val) {
  if (size() < sizeof(T))
    return false;
  std::memcpy((void *)m_Region.data(), &Val, sizeof(T));
  return true;
}

template bool RegionFragment::setContent<uint32_t>(uint32_t);
template bool RegionFragment::setContent<uint64_t>(uint64_t);
