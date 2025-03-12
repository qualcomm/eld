//===- HexagonELFDynamic.cpp-----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "HexagonELFDynamic.h"
#include "HexagonLDBackend.h"
#include "eld/Target/ELFFileFormat.h"
#include "llvm/BinaryFormat/ELF.h"

using namespace eld;

HexagonELFDynamic::HexagonELFDynamic(GNULDBackend &pParent,
                                     LinkerConfig &pConfig)
    : ELFDynamic(pParent, pConfig) {}

HexagonELFDynamic::~HexagonELFDynamic() {}

void HexagonELFDynamic::reserveTargetEntries() {
  // DT_HEXAGON_VER
  reserveOne(eld::DT_HEXAGON_VER);
  reserveOne(llvm::ELF::DT_RELACOUNT);
}

void HexagonELFDynamic::applyTargetEntries() {
  applyOne(eld::DT_HEXAGON_VER, 0x3);
  uint32_t relaCount = 0;
  for (auto &it : m_Backend.getRelaDyn()->getRelocations()) {
    if ((*it).type() == llvm::ELF::R_HEX_RELATIVE)
      relaCount++;
  }
  applyOne(llvm::ELF::DT_RELACOUNT, relaCount);
}
