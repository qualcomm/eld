//===- RISCVELFDynamic.cpp-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "RISCVELFDynamic.h"
#include "RISCVLDBackend.h"
#include "llvm/BinaryFormat/ELF.h"

using namespace eld;

RISCVELFDynamic::RISCVELFDynamic(GNULDBackend &pParent, LinkerConfig &pConfig)
    : ELFDynamic(pParent, pConfig) {}

RISCVELFDynamic::~RISCVELFDynamic() {}

void RISCVELFDynamic::reserveTargetEntries() {
  reserveOne(llvm::ELF::DT_RELACOUNT);
}

void RISCVELFDynamic::applyTargetEntries() {
  uint32_t relaCount = 0;
  for (auto &it : m_Backend.getRelaDyn()->getRelocations()) {
    if ((*it).type() == llvm::ELF::R_RISCV_RELATIVE)
      relaCount++;
  }
  applyOne(llvm::ELF::DT_RELACOUNT, relaCount);
}
