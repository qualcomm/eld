//===- ArchiveMemberInput.cpp----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Input/ArchiveMemberInput.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Diagnostics/MsgHandler.h"
#include "eld/Input/ArchiveFile.h"
#include "eld/Support/Memory.h"
#include <filesystem>
#include <system_error>

using namespace eld;

ArchiveMemberInput::ArchiveMemberInput(DiagnosticEngine *DiagEngine,
                                       ArchiveFile *ArchiveFile,
                                       llvm::StringRef Name, MemoryArea *Data,
                                       off_t ChildOffset)
    : Input(Name.str(), DiagEngine, Input::ArchiveMember), AF(ArchiveFile),
      ChildOffset(ChildOffset) {
  ResolvedPath = ArchiveFile->getInput()->getResolvedPath();
  FileName = ResolvedPath->native();
  setMemberName(Name);
  setMemArea(Data);
  // Update the Hash values.
  ResolvedPathHash = llvm::hash_combine(ResolvedPath->native());
  MemberNameHash = llvm::hash_combine(Name);
}

Input *ArchiveMemberInput::create(DiagnosticEngine *DiagEngine,
                                  ArchiveFile *ArchiveFile,
                                  llvm::StringRef Name, MemoryArea *Data,
                                  off_t ChildOffset) {
  ArchiveMemberInput *ARMember = make<ArchiveMemberInput>(
      DiagEngine, ArchiveFile, Name, Data, ChildOffset);
  InputFile *In = InputFile::create(ARMember, DiagEngine);
  switch (ArchiveFile->getArchiveKind()) {
  case ArchiveFile::Unknown:
    if (In->isObjectFile())
      ArchiveFile->setArchiveKind(ArchiveFile::ELF);
    else
      ArchiveFile->setArchiveKind(ArchiveFile::Bitcode);
    break;
  case ArchiveFile::Bitcode:
    if (In->isObjectFile())
      ArchiveFile->setArchiveKind(ArchiveFile::Mixed);
    break;
  case ArchiveFile::ELF:
    if (In->isBitcode())
      ArchiveFile->setArchiveKind(ArchiveFile::Mixed);
    break;
  default:
    break;
  }
  ARMember->setInputFile(std::move(In));
  return ARMember;
}

llvm::StringRef ArchiveMemberInput::getFileContents() const {
  return getMemoryBufferRef().getBuffer();
}

bool ArchiveMemberInput::noExport() const { return AF->noExport(); }

std::string ArchiveMemberInput::getDecoratedRelativePath(
    const std::string &Basepath) const {
  std::error_code Ec;
  std::filesystem::path P =
      std::filesystem::relative(getResolvedPath().getFullPath(), Basepath, Ec);
  if (Ec || !std::filesystem::exists(Basepath)) {
    DiagEngine->raise(Diag::warn_unable_to_compute_relpath)
        << getResolvedPath().native() << Basepath;
    return decoratedPath(/*showAbsolute=*/false);
  }

  if (!AF->isThin())
    return P.string() + "(" + Name + ")";

  std::filesystem::path MemAbsPath = getMemberAbsPath();

  std::filesystem::path MemRelativePath =
      std::filesystem::relative(MemAbsPath, Basepath, Ec);
  if (Ec) {
    DiagEngine->raise(Diag::warn_unable_to_compute_relpath)
        << getResolvedPath().native() << Basepath;
    return P.string() + "(" + Name + ")";
  }
  return P.string() + "(" + MemRelativePath.string() + ")";
}

std::string ArchiveMemberInput::getMemberAbsPath() const {
  std::filesystem::path ArchiveFileDirPath =
      std::filesystem::path(AF->getInput()->getResolvedPath().getFullPath())
          .parent_path();
  std::filesystem::path MemberPath(Name);
  if (!MemberPath.is_absolute())
    MemberPath = ArchiveFileDirPath / MemberPath;
  return MemberPath.string();
}
