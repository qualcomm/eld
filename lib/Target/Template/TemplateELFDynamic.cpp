//===- TemplateELFDynamic.cpp----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "TemplateELFDynamic.h"
#include "TemplateLDBackend.h"
#include "llvm/BinaryFormat/ELF.h"

using namespace eld;

TemplateELFDynamic::TemplateELFDynamic(GNULDBackend &pParent,
                                       LinkerConfig &pConfig)
    : ELFDynamic(pParent, pConfig) {}

TemplateELFDynamic::~TemplateELFDynamic() {}

void TemplateELFDynamic::reserveTargetEntries() {}

void TemplateELFDynamic::applyTargetEntries() {}
