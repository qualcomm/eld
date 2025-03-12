//===- MemoryRegion.h------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SUPPORT_MEMORYREGION_H
#define ELD_SUPPORT_MEMORYREGION_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/DataTypes.h"

namespace eld {

typedef llvm::ArrayRef<uint8_t> ConstMemoryRegion;
typedef llvm::MutableArrayRef<uint8_t> MemoryRegion;

} // namespace eld

#endif
