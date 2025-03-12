//===- StringRefUtils.cpp--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#include "eld/Support/StringRefUtils.h"
#include "llvm/Demangle/Demangle.h"

namespace eld {
namespace string {

bool isAlpha(char C) {
  return ('a' <= C && C <= 'z') || ('A' <= C && C <= 'Z') || C == '_';
}

bool isAlnum(char C) { return isAlpha(C) || ('0' <= C && C <= '9'); }

// Returns true if S is valid as a C language identifier.
bool isValidCIdentifier(llvm::StringRef S) {
  return !S.empty() && isAlpha(S[0]) &&
         std::all_of(S.begin() + 1, S.end(), isAlnum);
}

// Returns the demangled C++ symbol name for Name.
std::string getDemangledName(llvm::StringRef Name) {
  // itaniumDemangle can be used to demangle strings other than symbol
  // names which do not necessarily start with "_Z". Name can be
  // either a C or C++ symbol. Don't call itaniumDemangle if the name
  // does not look like a C++ symbol name to avoid getting unexpected
  // result for a C symbol that happens to match a mangled type name.
  if (!Name.starts_with("_Z"))
    return Name.str();

  char *Buf = llvm::itaniumDemangle(Name.str().c_str());
  if (!Buf)
    return Name.str();
  std::string S(Buf);
  free(Buf);
  return S;
}

size_t findNull(llvm::StringRef S, size_t EntSize) {
  // Optimize the common case.
  if (EntSize == 1)
    return S.find(0);

  for (unsigned I = 0, N = S.size(); I != N; I += EntSize) {
    const char *B = S.begin() + I;
    if (std::all_of(B, B + EntSize, [](char C) { return C == 0; }))
      return I;
  }
  return llvm::StringRef::npos;
}

size_t findEmpty(llvm::StringRef S, size_t EntSize) {
  // Optimize the common case.
  if (EntSize == 1)
    return S.find(' ');

  for (unsigned I = 0, N = S.size(); I != N; I += EntSize) {
    const char *B = S.begin() + I;
    if (std::all_of(B, B + EntSize, [](char C) { return C == ' '; }))
      return I;
  }
  return llvm::StringRef::npos;
}

} // namespace string
} // namespace eld
