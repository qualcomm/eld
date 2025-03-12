//===- AArch64GOT.cpp------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "AArch64GOT.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Readers/Relocation.h"
#include "eld/Support/Memory.h"

using namespace eld;

// GOTPLT0
AArch64GOTPLT0 *AArch64GOTPLT0::Create(ELFSection *O, ResolveInfo *R) {
  AArch64GOTPLT0 *G = make<AArch64GOTPLT0>(O, R);

  if (!R)
    return G;

  // Create a relocation and point to the ResolveInfo.
  Relocation *r1 = nullptr;

  r1 = Relocation::Create(llvm::ELF::R_AARCH64_ABS64, 64,
                          make<FragmentRef>(*G, 0), 0);
  r1->setSymInfo(R);

  O->addRelocation(r1);

  return G;
}

AArch64GOTPLTN *AArch64GOTPLTN::Create(ELFSection *O, ResolveInfo *R,
                                       Fragment *PLT) {
  AArch64GOTPLTN *G = make<AArch64GOTPLTN>(O, R);

  // If the symbol is IRELATIVE, the PLT slot contains the relative symbol
  // value. No need to fill the GOT slot with PLT0.
  if (PLT) {
    FragmentRef *PLTFragRef = make<FragmentRef>(*PLT, 0);
    Relocation *r = Relocation::Create(llvm::ELF::R_AARCH64_ABS64, 64,
                                       make<FragmentRef>(*G, 0), 0);
    O->addRelocation(r);
    r->modifyRelocationFragmentRef(PLTFragRef);
  }
  return G;
}
