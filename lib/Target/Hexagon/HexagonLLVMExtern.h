//===- HexagonLLVMExtern.h-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef HEXAGON_LLVM_EXTERN_H
#define HEXAGON_LLVM_EXTERN_H

#include <cstdint>

/* The structure mirrors the information for relocations specified in the
 * Hexagon ABI Document. Any changes or additions should be reflected in the
 * ABI
 * document, this structure is shared between the Linker and LLVM. The exact
 * copy of the structure is defined in HexagonDepDefines.h inside LLVM */

typedef struct {
  const char *Name;
  const uint32_t Type;
  const uint32_t EffectiveBits;
  const uint32_t BitMask;
  const uint32_t Alignment;
  const uint32_t Shift;
  const bool VerifyRange;
  const bool VerifyAlignment;
  const bool IsSigned;
} RelocationInfo;

namespace llvm {
namespace Hexagon {
extern "C" {
uint32_t doReloc(uint32_t RelocType, uint32_t Instruction, uint32_t Value);

bool verifyRange(uint32_t RelocType, uint32_t Value);

bool verifyAlignment(uint32_t RelocType, uint32_t Value);

bool isTruncated(uint32_t pRelocType, uint32_t Value);

extern const RelocationInfo Relocs[];
}
} // namespace Hexagon
} // namespace llvm

#endif
