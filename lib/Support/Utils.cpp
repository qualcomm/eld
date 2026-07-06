//===- Utils.cpp-----------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#include "eld/Support/Utils.h"
#include <sstream>

namespace eld {
namespace utility {
const std::string toHex(uint64_t Number) {
  std::stringstream Ss;
  Ss << std::hex << Number;
  std::string HexNumber = Ss.str();
  return HexNumber;
}

bool isNullDevice(const std::string &Path) {
  return Path == "/dev/null" || Path == "NUL";
}
} // namespace utility
} // namespace eld
