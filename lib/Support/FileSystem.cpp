//===- FileSystem.cpp------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Support/FileSystem.h"
#include "eld/Config/Config.h"
#include "eld/Support/Path.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/MemoryBuffer.h"
#include <vector>

/// Load Input files into a vector
/// \param Lines The vector to load file contents into
std::error_code
eld::sys::fs::loadFileContents(llvm::StringRef FilePath,
                               std::vector<std::string> &Lines) {
  Lines.clear();
  // Map in file list file.
  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> MB =
      llvm::MemoryBuffer::getFileOrSTDIN(FilePath);
  if (std::error_code EC = MB.getError())
    return EC;
  llvm::StringRef Buffer = MB->get()->getBuffer();
  while (!Buffer.empty()) {
    // Split off each line in the file.
    std::pair<llvm::StringRef, llvm::StringRef> LineAndRest =
        Buffer.split('\n');
    llvm::StringRef Line = LineAndRest.first;
    // Normalize Windows line endings for line-based parsers.
    Line = Line.rtrim('\r');
    Lines.push_back(Line.str());
    Buffer = LineAndRest.second;
  }
  return std::error_code();
}
