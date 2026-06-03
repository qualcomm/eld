//===- TemplateLLVMExtern.h------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef TEMPLATE_LLVM_EXTERN_H
#define TEMPLATE_LLVM_EXTERN_H

#include <cstdint>

// Here you can add information about relocations. Look at other architectures
// to see what kind of information they record.
typedef struct {
  const char *Name;
  const uint32_t Type;
  uint32_t Size;
} RelocationInfo;

// Here you can add helper functions that do various things based on the
// relocation information. Look at other architectures to see what kind of
// helpers they implement.
namespace llvm {
namespace Template {
extern "C" {
uint32_t doReloc(uint32_t RelocType, uint32_t Instruction, uint32_t Value);

extern const RelocationInfo Relocs[];
}
} // namespace Template
} // namespace llvm

#endif
