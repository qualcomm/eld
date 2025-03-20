//===- ARMPLT.cpp----------------------------------------------------------===//
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
#include "ARMPLT.h"
#include "ARMRelocationFunctions.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Readers/Relocation.h"
#include "eld/Support/Memory.h"

using namespace eld;

// PLT0
ARMPLT0 *ARMPLT0::Create(eld::IRBuilder &I, ARMGOT *G, ELFSection *O,
                         ResolveInfo *R) {
  ARMPLT0 *P = make<ARMPLT0>(G, I, O, R, 4, sizeof(arm_plt0));

  std::string name = "__gotplt0__";
  // create LDSymbol for the stub
  LDSymbol *symbol = I.addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
      O->getInputFile(), name, ResolveInfo::NoType, ResolveInfo::Define,
      ResolveInfo::Local,
      4, // size
      0, // value
      make<FragmentRef>(*G, 0), ResolveInfo::Internal,
      true /* isPostLTOPhase */);
  symbol->setShouldIgnore(false);

  // ARM PLT0 is very simple.
  // At Offset 16, Deposit the value of S + A - P (GOT0 - .)
  Relocation *r1 = nullptr;
  r1 = Relocation::Create(llvm::ELF::R_ARM_REL32, 32,
                          make<FragmentRef>(*P, 0x10), 0x0);
  r1->setSymInfo(symbol->resolveInfo());
  O->addRelocation(r1);
  return P;
}

// PLTN
ARMPLTN *ARMPLTN::Create(eld::IRBuilder &I, ARMGOT *G, ELFSection *O,
                         ResolveInfo *R) {
  ARMPLTN *P = make<ARMPLTN>(G, I, O, R, 4, sizeof(arm_plt1));

  std::string name = "__gotpltn_for_" + std::string(R->name());
  // create LDSymbol for the stub
  LDSymbol *symbol = I.addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
      O->getInputFile(), name, ResolveInfo::NoType, ResolveInfo::Define,
      ResolveInfo::Local,
      4, // size
      0, // value
      make<FragmentRef>(*G, 0), ResolveInfo::Internal,
      true /* isPostLTOPhase */);
  symbol->setShouldIgnore(false);

  Relocation *r1 = nullptr;
  Relocation *r2 = nullptr;
  Relocation *r3 = nullptr;
  r1 =
      Relocation::Create(R_ARM_ADD_PREL_20_8, 32, make<FragmentRef>(*P, 0), -8);
  r1->setSymInfo(symbol->resolveInfo());
  r2 =
      Relocation::Create(R_ARM_ADD_PREL_12_8, 32, make<FragmentRef>(*P, 4), -4);
  r2->setSymInfo(symbol->resolveInfo());
  r3 = Relocation::Create(R_ARM_LDR_PREL_12, 32, make<FragmentRef>(*P, 8), 0);
  r3->setSymInfo(symbol->resolveInfo());
  O->addRelocation(r1);
  O->addRelocation(r2);
  O->addRelocation(r3);

  return P;
}
