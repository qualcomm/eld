//===- ReproduceTarReader.cpp---------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Support/ReproduceTarReader.h"

#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/PluginAPI/DiagnosticEntry.h"
#include "eld/Support/InputTarReader.h"
#include "llvm/Support/MemoryBuffer.h"

namespace eld {

eld::Expected<ReproduceTarReader>
ReproduceTarReader::create(llvm::StringRef TarPath) {
  auto BufferOrErr = llvm::MemoryBuffer::getFile(TarPath);
  if (!BufferOrErr) {
    std::error_code EC = BufferOrErr.getError();
    return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
        Diag::error_cannot_read_tar_file, {TarPath.str(), EC.message()}));
  }
  return ReproduceTarReader(std::move(*BufferOrErr));
}

eld::Expected<llvm::MemoryBufferRef>
ReproduceTarReader::findFile(llvm::StringRef FileName) const {
  return InputTarReader::findFile(TarBuffer->getBuffer(), FileName);
}

bool ReproduceTarReader::hasFile(llvm::StringRef FileName) const {
  auto MBOrErr = findFile(FileName);
  return !!MBOrErr;
}

eld::Expected<std::string> ReproduceTarReader::readResponseFile() const {
  // response.txt is the canonical reproduced command line captured by
  // --reproduce and consumed by --replay.
  auto MBOrErr = findFile("response.txt");
  if (!MBOrErr)
    return std::move(MBOrErr.error());
  return MBOrErr->getBuffer().str();
}

} // namespace eld
