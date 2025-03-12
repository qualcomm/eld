//===- HexagonDepDefines.h-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#ifndef HEXAGON_DEP_DEFINES_H
#define HEXAGON_DEP_DEFINES_H

#include <cstdint>

typedef struct {
  uint32_t insnMask;
  uint32_t insnCmpMask;
  uint32_t insnBitMask;
  bool isDuplex;
} HexagonInstruction;

/* The structure mirrors the information for relocations specified in the
 * Hexagon ABI Document. Any changes or additions should be reflected in the ABI
 * document, this structure is shared between the Linker and LLVM. The exact
 * copy of the structure is defined in HexagonRelocationFunctions.h */
typedef struct RelocationDescription {
  const char *Name;
  const uint32_t Type;
  const uint32_t EffectiveBits;
  const uint32_t BitMask;
  const uint32_t Alignment;
  const uint32_t Shift;
  const bool VerifyRange;
  const bool VerifyAlignment;
  const bool IsSigned;
} HexagonRelocationInfo;

#endif
