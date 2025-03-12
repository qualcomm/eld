//===- StringRefUtils.h----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SUPPORT_STRINGREFUTILS_H
#define ELD_SUPPORT_STRINGREFUTILS_H

#include "llvm/ADT/StringRef.h"

namespace eld {
namespace string {

enum HashKind : uint8_t { XXHash, Md5, Uuid, Sha1 };

bool isAlpha(char C);

bool isAlnum(char C);

// Returns true if S is valid as a C language identifier.
bool isValidCIdentifier(llvm::StringRef S);

// Returns the demangled C++ symbol name for Name.
std::string getDemangledName(llvm::StringRef Name);

// Find Null terminator in a string with a specific entry size.
size_t findNull(llvm::StringRef s, size_t entSize);

// Find Empty terminator in a string with a specific entry size.
size_t findEmpty(llvm::StringRef s, size_t entSize);

} // namespace string
} // namespace eld

#endif
