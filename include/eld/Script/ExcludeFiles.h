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
    Spec(WildcardPattern *A, WildcardPattern *F) : m_Archive(A), m_File(F) {}
    WildcardPattern *m_Archive;
    WildcardPattern *m_File;
  };

  explicit ExcludePattern(WildcardPattern *Archive = nullptr,
                          WildcardPattern *File = nullptr)
      : m_Spec(Archive, File) {}

  bool isArchive() { return m_Spec.m_Archive != nullptr; }
  bool isFile() { return m_Spec.m_File != nullptr; }
  bool isFileInArchive() { return isArchive() && isFile(); }
  WildcardPattern *archive() { return m_Spec.m_Archive; }
  WildcardPattern *file() { return m_Spec.m_File; }

  ~ExcludePattern() {}
  Spec m_Spec;
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
  const_iterator begin() const { return m_ExcludeFiles.begin(); }
  iterator begin() { return m_ExcludeFiles.begin(); }
  const_iterator end() const { return m_ExcludeFiles.end(); }
  iterator end() { return m_ExcludeFiles.end(); }

  const_reference front() const { return m_ExcludeFiles.front(); }
  reference front() { return m_ExcludeFiles.front(); }
  const_reference back() const { return m_ExcludeFiles.back(); }
  reference back() { return m_ExcludeFiles.back(); }

  bool empty() const { return m_ExcludeFiles.empty(); }

  uint32_t size() const { return m_ExcludeFiles.size(); }

  void push_back(ExcludePattern *pPattern);

  ~ExcludeFiles();

private:
  ExcludeFileList m_ExcludeFiles;
};

} // namespace eld

#endif
