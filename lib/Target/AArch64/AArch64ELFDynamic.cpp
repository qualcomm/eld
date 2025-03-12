//===- AArch64ELFDynamic.cpp-----------------------------------------------===//
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

#include "AArch64ELFDynamic.h"
#include "AArch64LDBackend.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Target/ELFFileFormat.h"

using namespace eld;

AArch64ELFDynamic::AArch64ELFDynamic(GNULDBackend &pParent,
                                     LinkerConfig &pConfig)
    : ELFDynamic(pParent, pConfig) {}

AArch64ELFDynamic::~AArch64ELFDynamic() {}

void AArch64ELFDynamic::reserveTargetEntries() {
  reserveOne(llvm::ELF::DT_RELACOUNT);
}

void AArch64ELFDynamic::applyTargetEntries() {
  uint32_t relaCount = 0;
  for (auto &R : m_Backend.getRelaDyn()->getRelocations()) {
    if (R->type() == llvm::ELF::R_AARCH64_RELATIVE)
      relaCount++;
  }
  applyOne(llvm::ELF::DT_RELACOUNT, relaCount);
}
