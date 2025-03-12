//===- Utils.h-------------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#ifndef ELD_SUPPORT_UTILS_H
#define ELD_SUPPORT_UTILS_H

#include <string>

namespace eld {
namespace utility {
const std::string toHex(uint64_t number);
} // namespace utility
} // namespace eld

#endif
