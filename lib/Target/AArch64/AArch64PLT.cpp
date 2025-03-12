//===- AArch64PLT.cpp------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "AArch64PLT.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Readers/Relocation.h"
#include "eld/Support/Memory.h"

using namespace eld;

// PLT0
AArch64PLT0 *AArch64PLT0::Create(eld::IRBuilder &I, AArch64GOT *G,
                                 ELFSection *O, ResolveInfo *R) {
  AArch64PLT0 *P = (make<AArch64PLT0>(G, I, O, R, 4, sizeof(aarch64_plt0)));

  std::string name = "__gotplt0__";
  // create LDSymbol for the stub
  LDSymbol *symbol = I.AddSymbol<IRBuilder::Force, IRBuilder::Resolve>(
      O->getInputFile(), name, ResolveInfo::NoType, ResolveInfo::Define,
      ResolveInfo::Local,
      4, // size
      0, // value
      make<FragmentRef>(*G, 0), ResolveInfo::Default,
      true /* isPostLTOPhase */);
  symbol->setShouldIgnore(false);
  Relocation *r1 = nullptr;
  Relocation *r2 = nullptr;
  Relocation *r3 = nullptr;
  r1 = Relocation::Create(llvm::ELF::R_AARCH64_ADR_PREL_PG_HI21_NC, 32,
                          make<FragmentRef>(*P, 0x4), 0x10);
  r1->setSymInfo(symbol->resolveInfo());
  r2 = Relocation::Create(llvm::ELF::R_AARCH64_LDST64_ABS_LO12_NC, 32,
                          make<FragmentRef>(*P, 0x8), 0x10);
  r2->setSymInfo(symbol->resolveInfo());
  r3 = Relocation::Create(llvm::ELF::R_AARCH64_ADD_ABS_LO12_NC, 32,
                          make<FragmentRef>(*P, 0xc), 0x10);
  r3->setSymInfo(symbol->resolveInfo());
  O->addRelocation(r1);
  O->addRelocation(r2);
  O->addRelocation(r3);
  return P;
}

// PLTN
AArch64PLTN *AArch64PLTN::Create(eld::IRBuilder &I, AArch64GOT *G,
                                 ELFSection *O, ResolveInfo *R) {
  AArch64PLTN *P = (make<AArch64PLTN>(G, I, O, R, 4, sizeof(aarch64_plt1)));

  std::string name = "__gotpltn_for_" + std::string(R->name());
  // create LDSymbol for the stub
  LDSymbol *symbol = I.AddSymbol<IRBuilder::Force, IRBuilder::Resolve>(
      O->getInputFile(), name, ResolveInfo::NoType, ResolveInfo::Define,
      ResolveInfo::Local,
      4, // size
      0, // value
      make<FragmentRef>(*G, 0), ResolveInfo::Default,
      true /* isPostLTOPhase */);
  symbol->setShouldIgnore(false);
  Relocation *r1 = nullptr;
  Relocation *r2 = nullptr;
  Relocation *r3 = nullptr;
  r1 = Relocation::Create(llvm::ELF::R_AARCH64_ADR_PREL_PG_HI21_NC, 32,
                          make<FragmentRef>(*P, 0), 0);
  r1->setSymInfo(symbol->resolveInfo());
  r2 = Relocation::Create(llvm::ELF::R_AARCH64_LDST64_ABS_LO12_NC, 32,
                          make<FragmentRef>(*P, 4), 0);
  r2->setSymInfo(symbol->resolveInfo());
  r3 = Relocation::Create(llvm::ELF::R_AARCH64_ADD_ABS_LO12_NC, 32,
                          make<FragmentRef>(*P, 8), 0);
  r3->setSymInfo(symbol->resolveInfo());
  O->addRelocation(r1);
  O->addRelocation(r2);
  O->addRelocation(r3);

  return P;
}
