//===- Flavor.h------------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_DRIVER_FLAVOR_H
#define ELD_DRIVER_FLAVOR_H

#include "eld/Support/Defines.h"

enum Flavor {
  Invalid,
  Hexagon, // Hexagon
  ARM,     // ARM
  AArch64, // AArch64
  RISCV32, // RISCV32
  RISCV64, // RISCV64
  x86_64   // x86_64
};

#endif
