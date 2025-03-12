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
StringFragment::StringFragment(std::string pString, ELFSection *O)
    : Fragment(Fragment::String, O), m_String(pString) {}

StringFragment::~StringFragment() {}

size_t StringFragment::size() const {
  if (isNull())
    return 0;
  // Include \0.
  return (m_String.size() + 1);
}

eld::Expected<void> StringFragment::emit(MemoryRegion &mr, Module &M) {
  uint8_t *out = mr.begin() + getOffset(M.getConfig().getDiagEngine());
  memcpy(out, m_String.c_str(), size());

  // Fill with padding value as requested by the user.
  if (this->paddingSize() == 0)
    return {};
  out = mr.begin() + getOffset(M.getConfig().getDiagEngine()) -
        this->paddingSize();
  uint64_t paddingValue = *M.getFragmentPaddingValue(this);
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
