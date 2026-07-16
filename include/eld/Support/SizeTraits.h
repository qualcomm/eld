//===- SizeTraits.h--------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#ifndef ELD_SUPPORT_SIZETRAITS_H
#define ELD_SUPPORT_SIZETRAITS_H

#include "llvm/Support/DataTypes.h"

namespace eld {

/// SizeTraits - maps a target bit class (32 or 64, as returned by
/// TargetOptions::bitclass()) to the integer types wide enough to hold an
/// address/offset/word for that bit class.
template <unsigned BitClass> struct SizeTraits;

template <> struct SizeTraits<32> {
  typedef uint32_t Offset;
};

template <> struct SizeTraits<64> {
  typedef uint64_t Offset;
};

} // namespace eld

#endif
