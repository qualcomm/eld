//===- DynamicLibrary.h----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SUPPORT_DYNAMICLIBRARY_H
#define ELD_SUPPORT_DYNAMICLIBRARY_H

#include "llvm/Support/FileSystem.h"
#include <system_error>

namespace eld {
namespace DynamicLibrary {
void *Load(llvm::StringRef name);

void *GetFunction(void *H, llvm::StringRef Name);

bool Unload(void *H);

std::string GetLastError();

std::string getLibraryName(std::string Name);
} // namespace DynamicLibrary
} // namespace eld
#endif
