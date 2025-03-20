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
  explicit ArchiveMemberInput(DiagnosticEngine *DiagEngine,
                              ArchiveFile *ArchiveFile, llvm::StringRef Name,
                              MemoryArea *Data, off_t ChildOffset);

  ~ArchiveMemberInput() {}

  static bool classof(const Input *I) {
    return I->getInputType() == Input::ArchiveMember;
  }

  static Input *create(DiagnosticEngine *DiagEngine, ArchiveFile *ArchiveFile,
                       llvm::StringRef Name, MemoryArea *Data,
                       off_t ChildOffset);

  // -----  memory area  ----- //
  llvm::StringRef getFileContents() const;

  std::string decoratedPath(bool ShowAbsolute = false) const override {
    if (ShowAbsolute)
      return llvm::Twine(getResolvedPath().getFullPath() +
                         llvm::Twine("(" + Name + ")"))
          .str();
    return llvm::Twine(getResolvedPath().native() +
                       llvm::Twine("(" + Name + ")"))
        .str();
  }

  std::string
  getDecoratedRelativePath(const std::string &Basepath) const override;

  // Returns Archive and ArchiveMember Name.
  std::pair<std::string, std::string>
  decoratedPathPair(bool ShowAbsolute = false) const override {
    if (ShowAbsolute)
      return std::make_pair(getResolvedPath().getFullPath(), Name);
    return std::make_pair(getResolvedPath().native(), Name);
  }

  llvm::StringRef getMemberName() const { return Name; }

  off_t getChildOffset() const { return ChildOffset; }

  bool noExport() const;

  ArchiveFile *getArchiveFile() const { return AF; }

  void setArchiveFile(ArchiveFile *A) { AF = A; }

  std::string getMemberAbsPath() const;

private:
  void setMemberName(llvm::StringRef N) { Name = N.str(); }

protected:
  ArchiveFile *AF = nullptr;
  const off_t ChildOffset;
};

} // namespace eld

#endif // ELD_INPUT_ARCHIVEMEMBERINPUT_H
