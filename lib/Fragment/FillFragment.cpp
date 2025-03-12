//===- FillFragment.cpp----------------------------------------------------===//
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

#include "eld/Fragment/FillFragment.h"
#include "eld/Core/Module.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// FillFragment
//===----------------------------------------------------------------------===//
FillFragment::FillFragment(Module &M, uint64_t pValue, size_t pSize,
                           ELFSection *O, size_t pAlignment)
    : Fragment(Fragment::Fillment, O, pAlignment), m_Size(pSize) {
  M.setFragmentPaddingValue(this, pValue);
}

size_t FillFragment::size() const { return m_Size; }

eld::Expected<void> FillFragment::emit(MemoryRegion &mr, Module &M) {
  uint8_t *out = mr.begin() + getOffset(M.getConfig().getDiagEngine());
  uint64_t paddingValue = *M.getFragmentPaddingValue(this);
  uint32_t valueSize = Fragment::getPaddingValueSize(paddingValue);
  if (!valueSize)
    return {};
  if (valueSize == 1) {
    memset(out, paddingValue, size());
    return {};
  }

  // pattern fillment
  size_t numTiles = size() / valueSize;
  size_t remainder = size() % valueSize;
  for (size_t i = 0; i != numTiles; ++i)
    memcpy(out + i * valueSize, &paddingValue, valueSize);

  if (remainder > 0)
    memcpy(out + numTiles * valueSize, &paddingValue, remainder);
  return {};
}
