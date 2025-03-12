//===- HexagonGOT.cpp------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "HexagonGOT.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Readers/Relocation.h"

using namespace eld;

// GOTPLT0
HexagonGOTPLT0 *HexagonGOTPLT0::Create(ELFSection *O, ResolveInfo *R) {
  HexagonGOTPLT0 *G = make<HexagonGOTPLT0>(O, R);

  if (!R)
    return G;

  // Create a relocation and point to the ResolveInfo.
  Relocation *r1 = nullptr;

  r1 = Relocation::Create(llvm::ELF::R_HEX_32, 32, make<FragmentRef>(*G, 0), 0);
  r1->setSymInfo(R);

  O->addRelocation(r1);

  return G;
}

HexagonGOTPLTN *HexagonGOTPLTN::Create(ELFSection *O, ResolveInfo *R,
                                       Fragment *PLT) {
  HexagonGOTPLTN *G = make<HexagonGOTPLTN>(O, R);
  if (PLT) {
    FragmentRef *PLTFragRef = make<FragmentRef>(*PLT, 0);
    Relocation *r3 = Relocation::Create(llvm::ELF::R_HEX_32, 32,
                                        make<FragmentRef>(*G, 0), 0);
    O->addRelocation(r3);
    r3->modifyRelocationFragmentRef(PLTFragRef);
  }
  return G;
}
