//===- StringUtils.h-------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SUPPORT_STRINGUTILS_H
#define ELD_SUPPORT_STRINGUTILS_H

#include <string>
#include <vector>

namespace eld {
namespace string {
std::string ReplaceString(std::string &subject, const std::string &search,
                          std::string replace);
std::vector<std::string> split(const std::string &s, char seperator);
bool isASCII(const std::string S);

std::string unquote(const std::string &s);
} // namespace string
} // namespace eld

#endif
