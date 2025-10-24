//===- x86_64ELFDynamic.cpp----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "x86_64ELFDynamic.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Target/ELFFileFormat.h"
#include "eld/Target/GNULDBackend.h"

using namespace eld;

x86_64ELFDynamic::x86_64ELFDynamic(GNULDBackend &pParent, LinkerConfig &pConfig)
    : ELFDynamic(pParent, pConfig) {}

void x86_64ELFDynamic::reserveTargetEntries() {}

void x86_64ELFDynamic::applyTargetEntries() {}
