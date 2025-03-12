//===- StringUtils.cpp-----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#include "eld/Support/StringUtils.h"
#include "llvm/ADT/STLExtras.h"

namespace eld {
namespace string {
std::string ReplaceString(std::string &Subject, const std::string &Search,
                          std::string Replace) {
  size_t Pos = 0;
  while ((Pos = Subject.find(Search, Pos)) != std::string::npos) {
    Subject.replace(Pos, Search.length(), Replace);
    Pos += Replace.length();
  }
  return Subject;
}

/// \brief Split a string with multiple names seperated by comma.
/// \returns a vector of strings.
std::vector<std::string> split(const std::string &S, char Seperator) {
  std::vector<std::string> Output;
  std::string::size_type PrevPos = 0, Pos = 0;
  while ((Pos = S.find(Seperator, Pos)) != std::string::npos) {
    std::string Substring(S.substr(PrevPos, Pos - PrevPos));
    Output.push_back(Substring);
    PrevPos = ++Pos;
  }
  Output.push_back(S.substr(PrevPos, Pos - PrevPos)); // Last word
  return Output;
}

bool isASCII(const std::string S) {
  return llvm::find_if(S, [](const char &C) -> bool { return !isascii(C); }) ==
         S.end();
}

std::string unquote(const std::string &s) {
  if (s[0] == '"' || s[0] == '\'')
    return s.substr(1, s.length() - 2);
  return s;
}

} // namespace string
} // namespace eld
