//===- HexagonPLT.cpp------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "HexagonPLT.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Readers/Relocation.h"
#include "eld/Support/Memory.h"

using namespace eld;

// PLT0
HexagonPLT0 *HexagonPLT0::Create(eld::IRBuilder &I, HexagonGOT *G,
                                 ELFSection *O, ResolveInfo *R) {
  HexagonPLT0 *P = make<HexagonPLT0>(G, I, O, R, 4, sizeof(hexagon_plt0));
  O->addFragmentAndUpdateSize(P);

  // Create a relocation and point to the GOT.
  Relocation *r1 = nullptr;
  Relocation *r2 = nullptr;

  std::string name = "__gotplt0__";
  // create LDSymbol for the stub
  LDSymbol *symbol = I.AddSymbol<IRBuilder::Force, IRBuilder::Resolve>(
      O->getInputFile(), name, ResolveInfo::NoType, ResolveInfo::Define,
      ResolveInfo::Local,
      4, // size
      0, // value
      make<FragmentRef>(*G, 0), ResolveInfo::Internal,
      true /* isPostLTOPhase */);
  symbol->setShouldIgnore(false);

  r1 = Relocation::Create(llvm::ELF::R_HEX_B32_PCREL_X, 32,
                          make<FragmentRef>(*P, 0), 0);
  r1->setSymInfo(symbol->resolveInfo());
  r2 = Relocation::Create(llvm::ELF::R_HEX_6_PCREL_X, 32,
                          make<FragmentRef>(*P, 4), 4);
  r2->setSymInfo(symbol->resolveInfo());
  O->addRelocation(r1);
  O->addRelocation(r2);

  return P;
}

// PLTN
HexagonPLTN *HexagonPLTN::Create(eld::IRBuilder &I, HexagonGOT *G,
                                 ELFSection *O, ResolveInfo *R) {
  HexagonPLTN *P = make<HexagonPLTN>(G, I, O, R, 4, sizeof(hexagon_plt1));
  O->addFragmentAndUpdateSize(P);

  // Create a relocation and point to the GOT.
  Relocation *r1 = nullptr;
  Relocation *r2 = nullptr;
  std::string name = "__gotpltn_for_" + std::string(R->name());
  // create LDSymbol for the stub
  LDSymbol *symbol = I.AddSymbol<IRBuilder::Force, IRBuilder::Resolve>(
      O->getInputFile(), name, ResolveInfo::NoType, ResolveInfo::Define,
      ResolveInfo::Local,
      4, // size
      0, // value
      make<FragmentRef>(*G, 0), ResolveInfo::Internal,
      true /* isPostLTOPhase */);
  symbol->setShouldIgnore(false);
  r1 = Relocation::Create(llvm::ELF::R_HEX_B32_PCREL_X, 32,
                          make<FragmentRef>(*P, 0), 0);
  r1->setSymInfo(symbol->resolveInfo());
  r2 = Relocation::Create(llvm::ELF::R_HEX_6_PCREL_X, 32,
                          make<FragmentRef>(*P, 4), 4);
  r2->setSymInfo(symbol->resolveInfo());
  O->addRelocation(r1);
  O->addRelocation(r2);
  return P;
}
