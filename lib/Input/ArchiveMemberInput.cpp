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

ArchiveMemberInput::ArchiveMemberInput(DiagnosticEngine *diagEngine,
                                       ArchiveFile *archiveFile,
                                       llvm::StringRef name, MemoryArea *data,
                                       off_t childOffset)
    : Input(name.str(), diagEngine, Input::ArchiveMember),
      m_ArchiveFile(archiveFile), m_ChildOffset(childOffset) {
  m_ResolvedPath = archiveFile->getInput()->getResolvedPath();
  m_FileName = m_ResolvedPath->native();
  setMemberName(name);
  setMemArea(data);
  // Update the Hash values.
  m_ResolvedPathHash = llvm::hash_combine(m_ResolvedPath->native());
  m_MemberNameHash = llvm::hash_combine(m_Name);
}

Input *ArchiveMemberInput::Create(DiagnosticEngine *diagEngine,
                                  ArchiveFile *archiveFile,
                                  llvm::StringRef name, MemoryArea *data,
                                  off_t childOffset) {
  ArchiveMemberInput *ARMember = make<ArchiveMemberInput>(
      diagEngine, archiveFile, name, data, childOffset);
  InputFile *In = InputFile::Create(ARMember, diagEngine);
  switch (archiveFile->getArchiveKind()) {
  case ArchiveFile::Unknown:
    if (In->isObjectFile())
      archiveFile->setArchiveKind(ArchiveFile::ELF);
    else
      archiveFile->setArchiveKind(ArchiveFile::Bitcode);
    break;
  case ArchiveFile::Bitcode:
    if (In->isObjectFile())
      archiveFile->setArchiveKind(ArchiveFile::Mixed);
    break;
  case ArchiveFile::ELF:
    if (In->isBitcode())
      archiveFile->setArchiveKind(ArchiveFile::Mixed);
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

bool ArchiveMemberInput::noExport() const { return m_ArchiveFile->noExport(); }

std::string ArchiveMemberInput::getDecoratedRelativePath(
    const std::string &basepath) const {
  std::error_code ec;
  std::filesystem::path p =
      std::filesystem::relative(getResolvedPath().getFullPath(), basepath, ec);
  if (ec || !std::filesystem::exists(basepath)) {
    m_DiagEngine->raise(diag::warn_unable_to_compute_relpath)
        << getResolvedPath().native() << basepath;
    return decoratedPath(/*showAbsolute=*/false);
  }

  if (!m_ArchiveFile->isThin())
    return p.string() + "(" + m_Name + ")";

  std::filesystem::path memAbsPath = getMemberAbsPath();

  std::filesystem::path memRelativePath =
      std::filesystem::relative(memAbsPath, basepath, ec);
  if (ec) {
    m_DiagEngine->raise(diag::warn_unable_to_compute_relpath)
        << getResolvedPath().native() << basepath;
    return p.string() + "(" + m_Name + ")";
  }
  return p.string() + "(" + memRelativePath.string() + ")";
}

std::string ArchiveMemberInput::getMemberAbsPath() const {
  std::filesystem::path archiveFileDirPath =
      std::filesystem::path(
          m_ArchiveFile->getInput()->getResolvedPath().getFullPath())
          .parent_path();
  std::filesystem::path memberPath(m_Name);
  if (!memberPath.is_absolute())
    memberPath = archiveFileDirPath / memberPath;
  return memberPath.string();
}
