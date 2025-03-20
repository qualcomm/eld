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
FillFragment::FillFragment(Module &M, uint64_t PValue, size_t PSize,
                           ELFSection *O, size_t PAlignment)
    : Fragment(Fragment::Fillment, O, PAlignment), ThisSize(PSize) {
  M.setFragmentPaddingValue(this, PValue);
}

size_t FillFragment::size() const { return ThisSize; }

eld::Expected<void> FillFragment::emit(MemoryRegion &Mr, Module &M) {
  uint8_t *Out = Mr.begin() + getOffset(M.getConfig().getDiagEngine());
  uint64_t PaddingValue = *M.getFragmentPaddingValue(this);
  uint32_t ValueSize = Fragment::getPaddingValueSize(PaddingValue);
  if (!ValueSize)
    return {};
  if (ValueSize == 1) {
    memset(Out, PaddingValue, size());
    return {};
  }

  // pattern fillment
  size_t NumTiles = size() / ValueSize;
  size_t Remainder = size() % ValueSize;
  for (size_t I = 0; I != NumTiles; ++I)
    memcpy(Out + I * ValueSize, &PaddingValue, ValueSize);

  if (Remainder > 0)
    memcpy(Out + NumTiles * ValueSize, &PaddingValue, Remainder);
  return {};
}
