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
// Population of .dynamic address in done directly by getContent()
x86_64GOTPLT0 *x86_64GOTPLT0::Create(ELFSection *O, Module *M) {
  x86_64GOTPLT0 *G = make<x86_64GOTPLT0>(O, M);
  return G;
}

x86_64GOTPLTN *x86_64GOTPLTN::Create(ELFSection *O, ResolveInfo *R) {
  x86_64GOTPLTN *G = make<x86_64GOTPLTN>(O, R);
  return G;
}
