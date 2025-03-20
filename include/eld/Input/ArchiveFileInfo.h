//===- ArchiveFileInfo.h---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#ifndef ELD_INPUT_ARCHIVEFILEINFO_H
#define ELD_INPUT_ARCHIVEFILEINFO_H
#include "eld/Input/ArchiveFile.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"
#include <vector>

namespace eld {
/// ArchiveFileInfo stores archive file information that can be reused
// when an archive file is read multiple times. For instance, if an archive file
// is repeated in the link command, then it is read multiple times.
struct ArchiveFileInfo {
  ArchiveFileInfo() = default;
  ~ArchiveFileInfo() = default;
  llvm::DenseMap<off_t, Input *> LazyLoadMemberMap;
  std::vector<Input *> Members;
  ArchiveFile::SymTabType SymTab;
};
} // namespace eld
#endif
