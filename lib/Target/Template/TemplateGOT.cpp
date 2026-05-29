//===- TemplateGOT.cpp-----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "TemplateGOT.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Readers/Relocation.h"

using namespace eld;

// GOTPLT0
TemplateGOTPLT0 *TemplateGOTPLT0::Create(ELFSection *O, ResolveInfo *R) {
  TemplateGOTPLT0 *G = make<TemplateGOTPLT0>(O, R);

  return G;
}

TemplateGOTPLTN *TemplateGOTPLTN::Create(ELFSection *O, ResolveInfo *R,
                                         Fragment *PLT) {
  TemplateGOTPLTN *G = make<TemplateGOTPLTN>(O, R);

  return G;
}
