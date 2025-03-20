//===- StringFragment.cpp--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#include "eld/Fragment/StringFragment.h"
#include "eld/Core/Module.h"
#include "llvm/Support/Endian.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// StringFragment
//===----------------------------------------------------------------------===//
StringFragment::StringFragment(std::string PString, ELFSection *O)
    : Fragment(Fragment::String, O), FragmentString(PString) {}

StringFragment::~StringFragment() {}

size_t StringFragment::size() const {
  if (isNull())
    return 0;
  // Include \0.
  return (FragmentString.size() + 1);
}

eld::Expected<void> StringFragment::emit(MemoryRegion &Mr, Module &M) {
  uint8_t *Out = Mr.begin() + getOffset(M.getConfig().getDiagEngine());
  memcpy(Out, FragmentString.c_str(), size());

  // Fill with padding value as requested by the user.
  if (this->paddingSize() == 0)
    return {};
  Out = Mr.begin() + getOffset(M.getConfig().getDiagEngine()) -
        this->paddingSize();
  uint64_t PaddingValue = *M.getFragmentPaddingValue(this);
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
