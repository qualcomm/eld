//===- ExcludeFiles.h------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SCRIPT_EXCLUDEFILES_H
#define ELD_SCRIPT_EXCLUDEFILES_H

#include "eld/Config/Config.h"
#include <cstdint>
#include <vector>

namespace eld {

class WildcardPattern;

/** \class StringList
 *  \brief This class defines the interfaces to StringList.
 */

class ExcludePattern {
public:
  struct Spec {
    Spec(WildcardPattern *A, WildcardPattern *F)
        : ArchiveLibraryNamePattern(A), FileNamePattern(F) {}
    WildcardPattern *ArchiveLibraryNamePattern;
    WildcardPattern *FileNamePattern;
  };

  explicit ExcludePattern(WildcardPattern *Archive = nullptr,
                          WildcardPattern *File = nullptr)
      : InputSpec(Archive, File) {}

  bool isArchive() { return InputSpec.ArchiveLibraryNamePattern != nullptr; }
  bool isFile() { return InputSpec.FileNamePattern != nullptr; }
  bool isFileInArchive() { return isArchive() && isFile(); }
  WildcardPattern *archive() { return InputSpec.ArchiveLibraryNamePattern; }
  WildcardPattern *file() { return InputSpec.FileNamePattern; }

  ~ExcludePattern() {}
  Spec InputSpec;
};

class ExcludeFiles {
public:
  typedef std::vector<ExcludePattern *> ExcludeFileList;
  typedef ExcludeFileList::iterator iterator;
  typedef ExcludeFileList::const_iterator const_iterator;
  typedef ExcludeFileList::reference reference;
  typedef ExcludeFileList::const_reference const_reference;

  ExcludeFiles();
  /// Creates an ExcludeFiles object by joining ExcludeFiles EF1 and EF2.
  ExcludeFiles(const ExcludeFiles *EF1, const ExcludeFiles *EF2);
  ExcludeFiles(const ExcludeFiles &) = default;

public:
  const_iterator begin() const { return ExcludeFilesRule.begin(); }
  iterator begin() { return ExcludeFilesRule.begin(); }
  const_iterator end() const { return ExcludeFilesRule.end(); }
  iterator end() { return ExcludeFilesRule.end(); }

  const_reference front() const { return ExcludeFilesRule.front(); }
  reference front() { return ExcludeFilesRule.front(); }
  const_reference back() const { return ExcludeFilesRule.back(); }
  reference back() { return ExcludeFilesRule.back(); }

  bool empty() const { return ExcludeFilesRule.empty(); }

  uint32_t size() const { return ExcludeFilesRule.size(); }

  void pushBack(ExcludePattern *PPattern);

  ~ExcludeFiles();

private:
  ExcludeFileList ExcludeFilesRule;
};

} // namespace eld

#endif
