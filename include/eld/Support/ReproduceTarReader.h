//===- ReproduceTarReader.h-----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SUPPORT_REPRODUCETARREADER_H
#define ELD_SUPPORT_REPRODUCETARREADER_H

#include "eld/PluginAPI/Expected.h"

#include <memory>
#include <string>

namespace llvm {
class MemoryBuffer;
class StringRef;
} // namespace llvm

namespace eld {

/// \class ReproduceTarReader
/// \brief Reads files directly from a reproduce tarball in memory.
class ReproduceTarReader {
public:
  static eld::Expected<ReproduceTarReader> create(llvm::StringRef TarPath);

  eld::Expected<std::unique_ptr<llvm::MemoryBuffer>>
  findFile(llvm::StringRef FileName) const;

  bool hasFile(llvm::StringRef FileName) const;

  eld::Expected<std::string> readResponseFile() const;

private:
  explicit ReproduceTarReader(std::unique_ptr<llvm::MemoryBuffer> TarBuffer)
      : TarBuffer(std::move(TarBuffer)) {}

  std::unique_ptr<llvm::MemoryBuffer> TarBuffer;
};

} // namespace eld

#endif
