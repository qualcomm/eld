//===- InputTarReader.h----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SUPPORT_INPUTTARREADER_H
#define ELD_SUPPORT_INPUTTARREADER_H

#include "eld/PluginAPI/Expected.h"
#include "llvm/ADT/StringMap.h"

#include <memory>
#include <string>
#include <vector>

namespace llvm {
class MemoryBuffer;
class raw_ostream;
class StringRef;
} // namespace llvm

namespace eld {

class LinkerConfig;

/// \class InputTarReader
/// \brief Reads a tar archive into memory as a name->contents map.
///
/// This is intentionally small and self-contained so callers can inspect a tar
/// without writing extracted files to disk.
class InputTarReader {
public:
  struct EntryInfo {
    std::string Name;
    size_t Offset = 0;
    size_t Size = 0;
  };

  using FileMap = llvm::StringMap<std::string>;

  /// Return true if \p Data starts with a tar header block, recognized by its
  /// checksum.
  static bool isTarArchive(llvm::StringRef Data);

  /// Parse tar bytes directly from memory.
  static eld::Expected<FileMap> untar(llvm::StringRef TarData);

  /// Read a tar file from disk and parse it into memory.
  static eld::Expected<FileMap> untarFile(llvm::StringRef TarPath);

  /// Return all tar entry names in archive order.
  static eld::Expected<std::vector<std::string>>
  listEntryNames(llvm::StringRef TarData);

  /// Return all regular-file tar entries with payload offsets and sizes.
  static eld::Expected<std::vector<EntryInfo>>
  listRegularFileEntries(llvm::StringRef TarData);

  /// Read a tar file from disk and return all entry names.
  static eld::Expected<std::vector<std::string>>
  listEntryNamesFile(llvm::StringRef TarPath);

  /// Emit all tar entry names, one per line.
  static eld::Expected<void> emitEntryNames(llvm::StringRef TarData,
                                            llvm::raw_ostream &OS);

  /// Read a tar file from disk and emit all entry names, one per line.
  static eld::Expected<void> emitEntryNamesFile(llvm::StringRef TarPath,
                                                llvm::raw_ostream &OS);

  /// Find a regular file in tar data and return a buffer for its contents.
  ///
  /// The returned buffer owns its identifier, but its contents point into
  /// TarData, which must outlive the returned buffer.
  static eld::Expected<std::unique_ptr<llvm::MemoryBuffer>>
  findFile(llvm::StringRef TarData, llvm::StringRef FileName);

  /// Emit tar entry names through ELD diagnostics when trace=untar is enabled.
  static eld::Expected<void> traceEntryNames(llvm::StringRef TarData,
                                             LinkerConfig &Config);

  /// Read tar file from disk and emit entry names through ELD diagnostics when
  /// trace=untar is enabled.
  static eld::Expected<void> traceEntryNamesFile(llvm::StringRef TarPath,
                                                 LinkerConfig &Config);
};

} // namespace eld

#endif
