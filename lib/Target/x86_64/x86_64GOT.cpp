//===- x86_64GOT.cpp-------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#include "x86_64GOT.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Readers/Relocation.h"

using namespace eld;

// GOTPLT0
x86_64GOTPLT0 *x86_64GOTPLT0::Create(ELFSection *O, ResolveInfo *R) {
  x86_64GOTPLT0 *G = make<x86_64GOTPLT0>(O, R);

  if (!R)
    return G;

  // Create a relocation and point to the ResolveInfo.
  Relocation *r1 = nullptr;

  r1 = Relocation::Create(llvm::ELF::R_X86_64_JUMP_SLOT, 64,
                          make<FragmentRef>(*G, 0), 0);
  r1->setSymInfo(R);

  O->addRelocation(r1);

  return G;
}
