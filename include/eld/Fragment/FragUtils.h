//===- FragUtils.h---------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_FRAGMENT_FRAGUTILS_H
#define ELD_FRAGMENT_FRAGUTILS_H

#include "eld/Fragment/Fragment.h"
#include "llvm/ADT/StringRef.h"

namespace eld {
llvm::StringRef getRegionFromFragment(const Fragment *F);
} // namespace eld

#endif
