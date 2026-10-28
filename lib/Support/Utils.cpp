//===- Utils.cpp-----------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#include "eld/Support/Utils.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/raw_ostream.h"
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

bool isHostWindows() {
#if defined(_WIN32)
  return true;
#else
  return false;
#endif
}

std::string serializeGNUResponseFileArgument(llvm::StringRef Value) {
  std::string Serialized;
  llvm::raw_string_ostream OS(Serialized);
  llvm::sys::printArg(OS, Value, /*Quote=*/true);
  return OS.str();
}

// Based on the LLVM quoteSingleArg function.
std::string serializeWindowsResponseFileArgument(llvm::StringRef Value) {
  std::string Quoted = "\"";
  while (!Value.empty()) {
    // Consume a consecutive run of backslashes. Its length determines how it
    // must be escaped when the next character is a quote or the delimiter.
    size_t FirstNonBackslash = Value.find_first_not_of('\\');
    size_t BackslashCount = FirstNonBackslash;
    if (FirstNonBackslash == llvm::StringRef::npos) {
      // The remaining value consists only of backslashes. They all precede
      // the closing quote, so each one must be doubled.
      BackslashCount = Value.size();
      Quoted.append(BackslashCount * 2, '\\');
      break;
    }

    if (Value[FirstNonBackslash] == '"') {
      // An embedded quote needs one extra backslash after doubling the
      // preceding run, so the quote is treated as a literal character.
      Quoted.append(BackslashCount * 2 + 1, '\\');
      Quoted.push_back('"');
    } else {
      // Backslashes before ordinary characters are literal and need no
      // escaping.
      Quoted.append(BackslashCount, '\\');
      Quoted.push_back(Value[FirstNonBackslash]);
    }

    Value = Value.drop_front(FirstNonBackslash + 1);
  }
  Quoted += "\"";
  return Quoted;
}
} // namespace utility
} // namespace eld
