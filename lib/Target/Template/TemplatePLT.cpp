//===- TemplatePLT.cpp-----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "TemplatePLT.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Readers/Relocation.h"
#include "eld/Support/Memory.h"

using namespace eld;

// PLT0
TemplatePLT0 *TemplatePLT0::Create(eld::IRBuilder &I, TemplateGOT *G,
                                   ELFSection *O, ResolveInfo *R) {
  TemplatePLT0 *P = make<TemplatePLT0>(G, I, O, R, 4, sizeof(hexagon_plt0));

  return P;
}

// PLTN
TemplatePLTN *TemplatePLTN::Create(eld::IRBuilder &I, TemplateGOT *G,
                                   ELFSection *O, ResolveInfo *R) {
  TemplatePLTN *P = make<TemplatePLTN>(G, I, O, R, 4, sizeof(hexagon_plt1));

  return P;
}
