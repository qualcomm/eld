//===- TemplateRelocationCompute.cpp---------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
// This provides functionality to the linker and tools that need to process
// relocations and apply them.
//===----------------------------------------------------------------------===//
#include "TemplateLLVMExtern.h"
#include "TemplateRelocationInfo.h"

namespace llvm {
namespace Template {

extern "C" {

uint32_t doReloc(uint32_t RelocType, uint32_t Instruction, uint32_t Value) {
  return 0;
}

} // extern "C"
} // namespace Template
} // namespace llvm
