//===- TemplateRelocationInfo.h--------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#ifndef TEMPLATE_RELOCATION_INFO_H
#define TEMPLATE_RELOCATION_INFO_H

#include "llvm/BinaryFormat/ELF.h"

namespace llvm {
namespace Template {

extern "C" {
const RelocationInfo Relocs[1] = {{}};
} // extern "C"
} // namespace Template
} // namespace llvm

#endif
