//===- DynamicLibrary.inc--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <llvm/ADT/Twine.h>
#include <string>
#include <dlfcn.h>

namespace eld {
namespace DynamicLibrary {
void *Load(llvm::StringRef Name) {
  return (void *)dlopen(Name.data(), RTLD_NOW);
}

void *GetFunction(void *Handle, llvm::StringRef FunctionName) {
  return (void *)dlsym(Handle, FunctionName.data());
}

bool Unload(void *Handle) { return dlclose(Handle); }

std::string GetLastError() { return dlerror(); }

}
}
