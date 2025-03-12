//===- TemplateRelocationInfo.h--------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

//===- TemplateRelocationInfo.h - Relocation Application
// Helper-------------===//
//
// (c) 2020 Qualcomm Innovation Center, Inc. All rights reserved.
//
//===----------------------------------------------------------------------===//
#ifndef TEMPLATE_RELOCATION_INFO_H
#define TEMPLATE_RELOCATION_INFO_H

#include "llvm/BinaryFormat/ELF.h"

namespace llvm {
namespace Template {

extern "C" {
const RelocationInfo Relocs[] = {};
} // extern "C"
} // namespace Template
} // namespace llvm

#endif
