//===- TarWriter.h---------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_PLUGINAPI_TARWRITER_H
#define ELD_PLUGINAPI_TARWRITER_H

#include "Defines.h"
#include "PluginADT.h"
#include <mutex>

namespace llvm {
class TarWriter;
}

namespace eld::plugin {

class LinkerWrapper;

/// A utility class for creating tar archives
class DLL_A_EXPORT TarWriter {
  explicit TarWriter(std::unique_ptr<llvm::TarWriter> TW);

public:
  // Disable copy operations.
  TarWriter(const TarWriter &) = delete;
  TarWriter &operator=(const TarWriter &) = delete;

  // Movable.
  TarWriter(TarWriter &&) noexcept;
  TarWriter &operator=(TarWriter &&) noexcept;

  /// Adds MemoryBuffer contents to tar.
  /// After tar is updated, the buffer is destroyed.
  /// NOTE: This function is not thread safe.
  eld::Expected<void> addBufferToTar(plugin::MemoryBuffer &buffer) const;

  ~TarWriter();

private:
  std::unique_ptr<llvm::TarWriter> m_TW;
  friend class LinkerWrapper;
};

} // namespace eld::plugin
#endif
