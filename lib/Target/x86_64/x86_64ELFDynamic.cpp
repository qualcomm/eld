//===- x86_64ELFDynamic.cpp----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "x86_64ELFDynamic.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Target/ELFFileFormat.h"
#include "eld/Target/GNULDBackend.h"

using namespace eld;

x86_64ELFDynamic::x86_64ELFDynamic(GNULDBackend &pParent, LinkerConfig &pConfig)
    : ELFDynamic(pParent, pConfig) {}

void x86_64ELFDynamic::reserveTargetEntries() {
  // Reserve DT_RELACOUNT to advertise the number of RELATIVE relocations
  // in .rela.dyn for loader optimizations.
  reserveOne(llvm::ELF::DT_RELACOUNT);
}

void x86_64ELFDynamic::applyTargetEntries() {
  // Count RELATIVE relocations for DT_RELACOUNT emission.
  uint32_t relaCount = 0;
  for (auto &R : m_Backend.getRelaDyn()->getRelocations()) {
    if (R->type() == llvm::ELF::R_X86_64_RELATIVE)
      relaCount++;
  }
  applyOne(llvm::ELF::DT_RELACOUNT, relaCount);
}
