//===- ArchiveMemberInput.h------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
#ifndef ELD_INPUT_ARCHIVEMEMBERINPUT_H
#define ELD_INPUT_ARCHIVEMEMBERINPUT_H

#include "eld/Diagnostics/MsgHandler.h"
#include "eld/Input/Input.h"
#include <sys/types.h>

namespace eld {

class ArchiveFile;
class DiagnosticEngine;
class DiagnosticPrinter;

class ArchiveMemberInput : public Input {
public:
  explicit ArchiveMemberInput(DiagnosticEngine *diagEngine,
                              ArchiveFile *archiveFile, llvm::StringRef name,
                              MemoryArea *data, off_t childOffset);

  ~ArchiveMemberInput() {}

  static bool classof(const Input *I) {
    return I->getInputType() == Input::ArchiveMember;
  }

  static Input *Create(DiagnosticEngine *diagEngine, ArchiveFile *archiveFile,
                       llvm::StringRef name, MemoryArea *data,
                       off_t childOffset);

  // -----  memory area  ----- //
  llvm::StringRef getFileContents() const;

  std::string decoratedPath(bool showAbsolute = false) const override {
    if (showAbsolute)
      return llvm::Twine(getResolvedPath().getFullPath() +
                         llvm::Twine("(" + m_Name + ")"))
          .str();
    return llvm::Twine(getResolvedPath().native() +
                       llvm::Twine("(" + m_Name + ")"))
        .str();
  }

  std::string
  getDecoratedRelativePath(const std::string &basepath) const override;

  // Returns Archive and ArchiveMember Name.
  std::pair<std::string, std::string>
  decoratedPathPair(bool showAbsolute = false) const override {
    if (showAbsolute)
      return std::make_pair(getResolvedPath().getFullPath(), m_Name);
    return std::make_pair(getResolvedPath().native(), m_Name);
  }

  llvm::StringRef getMemberName() const { return m_Name; }

  off_t getChildOffset() const { return m_ChildOffset; }

  bool noExport() const;

  ArchiveFile *getArchiveFile() const { return m_ArchiveFile; }

  void setArchiveFile(ArchiveFile *A) { m_ArchiveFile = A; }

  std::string getMemberAbsPath() const;

private:
  void setMemberName(llvm::StringRef Name) { m_Name = Name.str(); }

protected:
  ArchiveFile *m_ArchiveFile = nullptr;
  const off_t m_ChildOffset;
};

} // namespace eld

#endif // ELD_INPUT_ARCHIVEMEMBERINPUT_H
