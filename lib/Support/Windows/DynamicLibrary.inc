//===- DynamicLibrary.inc--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <llvm/ADT/SmallVector.h>
#include <string>
#include <strsafe.h>
#include <windows.h>

namespace llvm {
namespace sys {
namespace windows {
std::error_code UTF8ToUTF16(StringRef Utf8, SmallVectorImpl<wchar_t> &Utf16);
}
}
}

namespace eld {
namespace DynamicLibrary {
void *Load(llvm::StringRef Name) {
  llvm::SmallVector<wchar_t, MAX_PATH> FileNameUnicode;
  if (std::error_code ec =
          llvm::sys::windows::UTF8ToUTF16(Name, FileNameUnicode))
    return nullptr;
  return LoadLibraryW(FileNameUnicode.data());
}

void *GetFunction(void *Handle, llvm::StringRef FunctionName) {
  return (void *)GetProcAddress((HMODULE)Handle, FunctionName.data());
}

bool Unload(void *Handle) { return FreeLibrary((HMODULE)Handle); }

std::string GetLastError() {
  // Get the error message, if any.
  DWORD ErrorMessageID = ::GetLastError();
  if (ErrorMessageID == 0)
    return std::string(); // No error message has been recorded

  LPSTR MessageBuffer = nullptr;
  size_t size = FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, ErrorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPSTR)&MessageBuffer, 0, nullptr);

  std::string Message(MessageBuffer, size);

  // Free the buffer.
  LocalFree(MessageBuffer);

  return Message;
}
}
}
