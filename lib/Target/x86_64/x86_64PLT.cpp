//===- x86_64PLT.cpp-------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#include "x86_64PLT.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Readers/Relocation.h"
#include "eld/Support/Memory.h"

using namespace eld;

// PLT0
x86_64PLT0 *x86_64PLT0::Create(eld::IRBuilder &I, x86_64GOT *G, ELFSection *O,
                               ResolveInfo *R, bool BindNow) {
  // No need of PLT0 when binding now.
  if (BindNow)
    return nullptr;
  x86_64PLT0 *P = make<x86_64PLT0>(G, I, O, R, 16, 16);
  O->addFragmentAndUpdateSize(P);

  // Create a relocation and point to the GOT.
  Relocation *r1 = nullptr;
  Relocation *r2 = nullptr;

  std::string name = "__gotplt0__";
  // create LDSymbol for the stub
  LDSymbol *symbol = I.addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
      O->getInputFile(), name, ResolveInfo::NoType, ResolveInfo::Define,
      ResolveInfo::Local,
      8, // size
      0, // value
      make<FragmentRef>(*G, 0), ResolveInfo::Internal,
      true /* isPostLTOPhase */);
  symbol->setShouldIgnore(false);

  r1 = Relocation::Create(llvm::ELF::R_X86_64_JUMP_SLOT, 64,
                          make<FragmentRef>(*P, 0), 0);
  r1->setSymInfo(symbol->resolveInfo());
  r2 = Relocation::Create(llvm::ELF::R_X86_64_JUMP_SLOT, 64,
                          make<FragmentRef>(*P, 8), 4);
  r2->setSymInfo(symbol->resolveInfo());
  O->addRelocation(r1);
  O->addRelocation(r2);

  return P;
}

// PLTN
x86_64PLTN *x86_64PLTN::Create(eld::IRBuilder &I, x86_64GOT *G, ELFSection *O,
                               ResolveInfo *R, bool BindNow) {
  x86_64PLTN *P = make<x86_64PLTN>(G, I, O, R, 16, 16);
  O->addFragmentAndUpdateSize(P);

  // Create a relocation and point to the GOT.
  Relocation *r1 = nullptr;
  Relocation *r2 = nullptr;
  std::string name = "__gotpltn_for_" + std::string(R->name());
  // create LDSymbol for the stub
  LDSymbol *symbol = I.addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
      O->getInputFile(), name, ResolveInfo::NoType, ResolveInfo::Define,
      ResolveInfo::Local,
      8, // size
      0, // value
      make<FragmentRef>(*G, 0), ResolveInfo::Internal,
      true /* isPostLTOPhase */);
  symbol->setShouldIgnore(false);
  r1 = Relocation::Create(llvm::ELF::R_X86_64_JUMP_SLOT, 64,
                          make<FragmentRef>(*P, 0), 0);
  r1->setSymInfo(symbol->resolveInfo());
  r2 = Relocation::Create(llvm::ELF::R_X86_64_JUMP_SLOT, 64,
                          make<FragmentRef>(*P, 8), 8);
  r2->setSymInfo(symbol->resolveInfo());
  O->addRelocation(r1);
  O->addRelocation(r2);

  // No PLT0 for immediate binding.
  if (BindNow)
    return P;

  Fragment *F = *(O->getFragmentList().begin());
  FragmentRef *PLT0FragRef = make<FragmentRef>(*F, 0);
  Relocation *r3 = Relocation::Create(llvm::ELF::R_X86_64_JUMP_SLOT, 64,
                                      make<FragmentRef>(*G, 0), 0);
  O->addRelocation(r3);
  r3->modifyRelocationFragmentRef(PLT0FragRef);
  return P;
}
