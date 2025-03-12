//===- TemplateRelocationFunctions.h---------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
#ifndef TEMPLATE_RELOCATION_FUNCTIONS_H
#define TEMPLATE_RELOCATION_FUNCTIONS_H

#include "TemplateLLVMExtern.h"
#include "TemplateRelocator.h"

namespace eld {

class Relocation;

struct RelocationDescription;

TemplateRelocator::Result applyNone(Relocation &pEntry,
                                    TemplateRelocator &pParent,
                                    RelocationDescription &RelocDesc);
TemplateRelocator::Result applyAbs(Relocation &pEntry,
                                   TemplateRelocator &pParent,
                                   RelocationDescription &RelocDesc);
TemplateRelocator::Result applyRel(Relocation &pEntry,
                                   TemplateRelocator &pParent,
                                   RelocationDescription &RelocDesc);
TemplateRelocator::Result applyHILO(Relocation &pEntry,
                                    TemplateRelocator &pParent,
                                    RelocationDescription &RelocDesc);
TemplateRelocator::Result applyJumpOrCall(Relocation &pEntry,
                                          TemplateRelocator &pParent,
                                          RelocationDescription &RelocDesc);
TemplateRelocator::Result applyAlign(Relocation &pEntry,
                                     TemplateRelocator &pParent,
                                     RelocationDescription &RelocDesc);
TemplateRelocator::Result applyRelax(Relocation &pEntry,
                                     TemplateRelocator &pParent,
                                     RelocationDescription &RelocDesc);
TemplateRelocator::Result applyGPRel(Relocation &pEntry,
                                     TemplateRelocator &pParent,
                                     RelocationDescription &RelocDesc);
TemplateRelocator::Result unsupported(Relocation &pEntry,
                                      TemplateRelocator &pParent,
                                      RelocationDescription &RelocDesc);

typedef Relocator::Result (*ApplyFunctionType)(
    eld::Relocation &pReloc, eld::TemplateRelocator &pParent,
    RelocationDescription &pRelocDesc);

struct RelocationDescription {
  // The application function for the relocation.
  const ApplyFunctionType func;
  // The Relocation type, this is just kept for convenience when writing new
  // handlers for relocations.
  const unsigned int type;
  // If the user specified, the relocation to be force verified, the relocation
  // is verified for alignment, truncation errors(only for relocations that take
  // in non signed values, signed values are bound to exceed the number of
  // bits).
  bool forceVerify;
};

struct RelocationDescription RelocDesc[] = {
    /* {  func =         &applyNone
            ,  type =        llvm::ELF::
            ,  forceVerify =  false
    } */
};

} // namespace eld

#endif // TEMPLATE_RELOCATION_FUNCTIONS_H
