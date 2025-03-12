//===- RISCVGOT.cpp--------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "RISCVGOT.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Readers/Relocation.h"

using namespace eld;

RISCVGOT *RISCVGOT::Create(ELFSection *O, ResolveInfo *R, bool is32bit) {
  if (is32bit)
    return make<RISCVTGOT<uint32_t, 4, 4>>(GOT::Regular, O, R);
  return make<RISCVTGOT<uint64_t, 8, 8>>(GOT::Regular, O, R);
}

RISCVGOT *RISCVGOT::CreateGOT0(ELFSection *O, ResolveInfo *R, bool is32bit) {
  RISCVGOT *G = nullptr;
  if (is32bit)
    G = make<RISCVTGOT<uint32_t, 4, 4>>(GOT::Regular, O, R);
  else
    G = make<RISCVTGOT<uint64_t, 8, 8>>(GOT::Regular, O, R);
  if (!R)
    return G;
  // Create a relocation and point to the ResolveInfo.
  Relocation *r1 = nullptr;
  if (is32bit)
    r1 = Relocation::Create(llvm::ELF::R_RISCV_32, 32, make<FragmentRef>(*G, 0),
                            0);
  else
    r1 = Relocation::Create(llvm::ELF::R_RISCV_64, 64, make<FragmentRef>(*G, 0),
                            0);
  r1->setSymInfo(R);
  O->addRelocation(r1);
  return G;
}

RISCVGOT *RISCVGOT::CreateGOTPLT0(ELFSection *O, ResolveInfo *R, bool is32bit) {
  RISCVGOT *G = nullptr;
  if (is32bit)
    G = make<RISCVGOTPLT0<uint32_t>>(O, R);
  else
    G = make<RISCVGOTPLT0<uint64_t>>(O, R);
  return G;
}

RISCVGOT *RISCVGOT::CreateGOTPLTN(ELFSection *O, ResolveInfo *R, bool is32bit) {
  RISCVGOT *G = nullptr;
  if (is32bit)
    G = make<RISCVGOTPLTN<uint32_t, 4, 4>>(O, R);
  else
    G = make<RISCVGOTPLTN<uint64_t, 8, 8>>(O, R);
  return G;
}

RISCVGOT *RISCVGOT::CreateGD(ELFSection *O, ResolveInfo *R, bool is32bit) {
  if (is32bit)
    return make<RISCVGDGOT<uint32_t, 4, 4>>(O, R);
  return make<RISCVGDGOT<uint64_t, 8, 8>>(O, R);
}

RISCVGOT *RISCVGOT::CreateLD(ELFSection *O, ResolveInfo *R, bool is32bit) {
  if (is32bit)
    return make<RISCVLDGOT<uint32_t, 4, 4>>(O, R);
  return make<RISCVLDGOT<uint64_t, 8, 8>>(O, R);
}

RISCVGOT *RISCVGOT::CreateIE(ELFSection *O, ResolveInfo *R, bool is32bit) {
  if (is32bit)
    return make<RISCVIEGOT<uint32_t, 4, 4>>(O, R);
  return make<RISCVIEGOT<uint64_t, 8, 8>>(O, R);
}
