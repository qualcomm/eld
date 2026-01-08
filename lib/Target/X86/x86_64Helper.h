//===- x86_64Helper.h------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "llvm/Support/DataTypes.h"

namespace {

// Get sign of instruction.
template <typename T> int getSign(T Instruction) {
  return (-((Instruction >> 31) & 1));
}

// Apply Encoding depending on word sizes

template <typename T> T encode8(T Result) {
  Result = Result & 0xFF;
  return Result;
}
template <typename T> T encode16(T Result) {
  Result = Result & 0xFFFF;
  return Result;
}
template <typename T> T encode32(T Result) {
  Result = Result & 0xFFFFFFFF;
  return Result;
}

template <typename T> T encode64(T Result) {
  Result = Result & 0xFFFFFFFFFFFFFFFF;
  return Result;
}
} // namespace
