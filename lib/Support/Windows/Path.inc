//===- Path.inc------------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <windows.h>
#undef max
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/Twine.h>
#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/Windows/WindowsSupport.h>
#include <llvm/Support/WindowsError.h>

namespace llvm {
namespace sys {
namespace path {
std::error_code widenPath(const Twine &Path8, SmallVectorImpl<wchar_t> &Path16);
}
} // namespace sys
} // namespace llvm

std::string Path::getFullPath(const std::string &Path) {
  WCHAR RealPath[1024];
  llvm::SmallVector<wchar_t, MAX_PATH> FileName;
  llvm::sys::windows::widenPath(llvm::Twine(Path.c_str()), FileName);
  GetFullPathNameW(FileName.data(), 1024, RealPath, 0);
  std::wstring AbsPath = RealPath;
  return std::string(AbsPath.begin(), AbsPath.end());
}
