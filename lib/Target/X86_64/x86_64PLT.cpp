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
// Creates PLT0 stub with relocations to reference GOTPLT[1] and GOTPLT[2].
// Note that these relocations get resolved at link time.
// PLT0 is called by all PLT entries to invoke the dynamic linker.
x86_64PLT0 *x86_64PLT0::Create(eld::IRBuilder &I, x86_64GOT *G, ELFSection *O,
                               ResolveInfo *R, bool BindNow) {

  x86_64PLT0 *P = make<x86_64PLT0>(G, I, O, R, 16, 16);
  O->addFragmentAndUpdateSize(P);

  // First instruction: pushq GOTPLT+8(%rip)
  // Patches offset at PLT0+2 to reference GOTPLT[1] (link_map)
  Relocation *r1 = Relocation::Create(llvm::ELF::R_X86_64_PC32, 32,
                                      make<FragmentRef>(*P, 2), -4);
  r1->modifyRelocationFragmentRef(make<FragmentRef>(*G, 8));
  O->addRelocation(r1);

  // Second instruction: jmp *GOTPLT+16(%rip)
  // Patches offset at PLT0+8 to reference GOTPLT[2] (_dl_runtime_resolve)
  Relocation *r2 = Relocation::Create(llvm::ELF::R_X86_64_PC32, 32,
                                      make<FragmentRef>(*P, 8), -4);
  r2->modifyRelocationFragmentRef(make<FragmentRef>(*G, 16));
  O->addRelocation(r2);

  return P;
}

// PLTN
//  Creates PLTN stub with relocations to reference GOTPLTN and PLT0
//  Note that these relocations get resolved at link time.
x86_64PLTN *x86_64PLTN::Create(eld::IRBuilder &I, x86_64GOT *G, ELFSection *O,
                               ResolveInfo *R, bool BindNow) {
  x86_64PLTN *P = make<x86_64PLTN>(G, I, O, R, 16, 16);
  O->addFragmentAndUpdateSize(P);

  // Link GOTPLTN to this PLT entry so it can compute its initial value
  x86_64GOTPLTN *gotplt = llvm::cast<x86_64GOTPLTN>(G);
  gotplt->setPLTEntry(P);

  // First instruction: jmpq *GOTPLTN(%rip)
  // Patches offset at PLTN+2 to reference GOTPLTN entry
  Relocation *r1 = Relocation::Create(llvm::ELF::R_X86_64_PC32, 32,
                                      make<FragmentRef>(*P, 2), -4);
  r1->modifyRelocationFragmentRef(make<FragmentRef>(*G, 0));
  O->addRelocation(r1);

  if (!BindNow) {
    Fragment *PLT0 = *(O->getFragmentList().begin());

    // Third instruction: jmpq PLT0
    // Patches offset at PLTN+12 to reference PLT0 entry
    Relocation *r2 = Relocation::Create(llvm::ELF::R_X86_64_PC32, 32,
                                        make<FragmentRef>(*P, 12), -4);
    r2->modifyRelocationFragmentRef(make<FragmentRef>(*PLT0, 0));
    O->addRelocation(r2);
  }

  return P;
}
