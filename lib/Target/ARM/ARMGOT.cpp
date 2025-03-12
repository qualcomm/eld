//===- ARMGOT.cpp----------------------------------------------------------===//
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
#include "ARMGOT.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Readers/Relocation.h"

using namespace eld;

// GOTPLT0
ARMGOTPLT0 *ARMGOTPLT0::Create(ELFSection *O, ResolveInfo *R) {
  ARMGOTPLT0 *G = make<ARMGOTPLT0>(O, R);

  if (!R)
    return G;

  // Create a relocation and point to the ResolveInfo.
  Relocation *r1 = nullptr;

  r1 = Relocation::Create(llvm::ELF::R_ARM_ABS32, 32, make<FragmentRef>(*G, 0),
                          0);
  r1->setSymInfo(R);

  O->addRelocation(r1);

  return G;
}

ARMGOTPLTN *ARMGOTPLTN::Create(ELFSection *O, ResolveInfo *R, Fragment *PLT) {
  ARMGOTPLTN *G = make<ARMGOTPLTN>(O, R);
  // If the symbol is IRELATIVE, the PLT slot contains the relative symbol
  // value. No need to fill the GOT slot with PLT0.
  // No PLT0 for immediate binding.
  if (PLT) {
    // Fill GOT PLT slots with address of PLT0.
    FragmentRef *PLT0FragRef = make<FragmentRef>(*PLT, 0);
    Relocation *r0 = Relocation::Create(llvm::ELF::R_ARM_ABS32, 32,
                                        make<FragmentRef>(*G, 0), 0);
    O->addRelocation(r0);
    r0->modifyRelocationFragmentRef(PLT0FragRef);
  }
  return G;
}
