//===-
//DriverFlavor.h------------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_DRIVER_FLAVOR_H
#define ELD_DRIVER_FLAVOR_H

#include "eld/Support/Defines.h"

enum DriverFlavor {
  Invalid,
  Hexagon,         // Hexagon
  ARM_AArch64,     // ARM, AArch64
  RISCV32_RISCV64, // RISCV32
  x86_64,          // x86_64
  Unknown          // Unknown
};

#endif
