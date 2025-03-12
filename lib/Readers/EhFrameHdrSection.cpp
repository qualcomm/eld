//===- EhFrameHdrSection.cpp-----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//


#include "eld/Readers/EhFrameHdrSection.h"
#include "eld/Fragment/EhFrameFragment.h"

using namespace eld;

EhFrameHdrSection::EhFrameHdrSection(std::string Name, uint32_t pType,
                                     uint32_t pFlag, uint32_t entSize,
                                     uint64_t pSize)
    : ELFSection(Section::Kind::EhFrameHdr, LDFileFormat::EhFrameHdr, Name,
                 pFlag, entSize, /*AddrAlign=*/0, pType, /*Info=*/0,
                 /*Link=*/nullptr, pSize, /*PAddr=*/0) {}

void EhFrameHdrSection::addCIE(std::vector<CIEFragment *> &C) {
  for (auto &CIE : C) {
    m_CIEs.push_back(CIE);
    ++NumCIE;
    NumFDE += CIE->getNumFDE();
  }
}
