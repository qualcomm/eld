//===- ARMELFDynamic.cpp---------------------------------------------------===//
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
#include "ARMELFDynamic.h"
#include "ARMLDBackend.h"
#include "eld/Target/ELFFileFormat.h"
#include "llvm/BinaryFormat/ELF.h"

using namespace eld;

ARMELFDynamic::ARMELFDynamic(GNULDBackend &pParent, LinkerConfig &pConfig)
    : ELFDynamic(pParent, pConfig) {}

ARMELFDynamic::~ARMELFDynamic() {}

void ARMELFDynamic::reserveTargetEntries() {
  reserveOne(llvm::ELF::DT_RELCOUNT);
}

void ARMELFDynamic::applyTargetEntries() {
  uint32_t relaCount = 0;
  for (auto &R : m_Backend.getRelaDyn()->getRelocations())
    if (R->type() == llvm::ELF::R_ARM_RELATIVE)
      relaCount++;
  applyOne(llvm::ELF::DT_RELCOUNT, relaCount);
}
