//===- x86_64PLTGot.cpp----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "x86_64PLTGot.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Readers/Relocation.h"
#include "eld/Support/Memory.h"

using namespace eld;

// Create a compact .plt.got stub in section O.
//
// The stub contains one link-time R_X86_64_PC32 relocation (r1) that patches
// bytes 2-5 of the jmpq instruction to hold the PC-relative displacement to
// GotSlot (the symbol's .got entry). GotSlot is filled at startup by
// R_X86_64_GLOB_DAT so no dynamic relocation is needed on the stub itself.
x86_64PLTGot *x86_64PLTGot::Create(eld::IRBuilder &I, x86_64GOT *GotSlot,
                                   ELFSection *O, ResolveInfo *R) {
  x86_64PLTGot *P = make<x86_64PLTGot>(GotSlot, O, R);
  O->addFragmentAndUpdateSize(P);

  // r1: patch jmpq displacement (bytes 2-5) to point to the .got slot.
  // Addend -4 accounts for the PC being at the end of the 6-byte instruction.
  Relocation *r1 = Relocation::Create(llvm::ELF::R_X86_64_PC32, 32,
                                      make<FragmentRef>(*P, 2), -4);
  r1->modifyRelocationFragmentRef(make<FragmentRef>(*GotSlot, 0));
  O->addRelocation(r1);

  return P;
}
