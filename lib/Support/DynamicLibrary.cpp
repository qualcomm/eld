//===- DynamicLibrary.cpp--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Support/DynamicLibrary.h"
#include "eld/Config/Config.h"

#if defined(ELD_ON_MSVC)
#include "Windows/DynamicLibrary.inc"
#endif
#if defined(ELD_ON_UNIX)
#include "Unix/DynamicLibrary.inc"
#endif

namespace eld {
namespace DynamicLibrary {
std::string getLibraryName(std::string Name) {
#ifdef ELD_ON_MSVC
  return llvm::Twine(Name + ".dll").str();
#else
  return llvm::Twine("lib" + Name + ".so").str();
#endif
}
} // namespace DynamicLibrary
} // namespace eld
