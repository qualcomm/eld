//===- InputFile.cpp-------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Input/InputFile.h"
#include "eld/Input/ArchiveFile.h"
#include "eld/Input/ArchiveMemberInput.h"
#include "eld/Input/BinaryFile.h"
#include "eld/Input/BitcodeFile.h"
#include "eld/Input/ELFDynObjectFile.h"
#include "eld/Input/ELFExecutableFile.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/Input/LinkerScriptFile.h"
#include "eld/Input/SymDefFile.h"
#include "eld/Support/Memory.h"
#include "llvm/BinaryFormat/Magic.h"
#include "llvm/Support/FileSystem.h"

using namespace eld;

/// \return Type of Input File
/// \param S Magic number of File used to identify it
/// If input file is not an ELF file, bitcode file, or archive,
/// It will be treated as a linker script
InputFile::Kind InputFile::GetInputFileKind(llvm::StringRef S) {
  auto magic = llvm::identify_magic(S);
  if (magic == llvm::file_magic::elf_relocatable)
    return InputFile::ELFObjFileKind;
  else if (magic == llvm::file_magic::elf_shared_object)
    return InputFile::ELFDynObjFileKind;
  else if (magic == llvm::file_magic::elf_executable)
    return InputFile::ELFExecutableFileKind;
  else if (magic == llvm::file_magic::bitcode)
    return InputFile::BitcodeFileKind;
  else if (magic == llvm::file_magic::archive)
    return InputFile::GNUArchiveFileKind;
  else if (S.starts_with("#<SYMDEFS"))
    return InputFile::ELFSymDefFileKind;
  return InputFile::GNULinkerScriptKind;
}

/// Create an Embedded file.
InputFile *InputFile::CreateEmbedded(Input *I, llvm::StringRef S,
                                     DiagnosticEngine *DiagEngine) {
  InputFile::Kind K = GetInputFileKind(S);
  InputFile *EmbeddedFile = nullptr;
  switch (K) {
  case InputFile::ELFObjFileKind:
    EmbeddedFile = make<ELFObjectFile>(I, DiagEngine);
    break;
  case InputFile::ELFExecutableFileKind:
    EmbeddedFile = make<ELFExecutableFile>(I, DiagEngine);
    break;
  case InputFile::ELFDynObjFileKind:
    EmbeddedFile = make<ELFDynObjectFile>(I, DiagEngine);
    break;
  case InputFile::BitcodeFileKind:
    EmbeddedFile = make<BitcodeFile>(I, DiagEngine);
    break;
  case InputFile::GNUArchiveFileKind:
    EmbeddedFile = make<ArchiveFile>(I, DiagEngine);
    break;
  case InputFile::ELFSymDefFileKind:
    EmbeddedFile = make<SymDefFile>(I, DiagEngine);
    break;
  case InputFile::GNULinkerScriptKind:
    EmbeddedFile = make<LinkerScriptFile>(I, DiagEngine);
    break;
  default:
    return nullptr;
  }
  EmbeddedFile->setContents(S);
  return std::move(EmbeddedFile);
}

/// Create an Input File.
InputFile *InputFile::Create(Input *I, DiagnosticEngine *diagEngine) {
  llvm::StringRef S;
  if (I->getSize())
    S = I->getFileContents();
  bool isBinaryFile =
      I->getAttribute().isBinary() && I->getInputType() == Input::Type::Default;
  InputFile::Kind K =
      (isBinaryFile ? InputFile::Kind::BinaryFileKind : GetInputFileKind(S));
  return InputFile::Create(I, K, diagEngine);
}

InputFile *InputFile::Create(Input *I, Kind K, DiagnosticEngine *DiagEngine) {
  switch (K) {
  case InputFile::ELFObjFileKind:
    return make<ELFObjectFile>(I, DiagEngine);
  case InputFile::ELFExecutableFileKind:
    return make<ELFExecutableFile>(I, DiagEngine);
  case InputFile::ELFDynObjFileKind:
    return make<ELFDynObjectFile>(I, DiagEngine);
  case InputFile::BitcodeFileKind:
    return make<BitcodeFile>(I, DiagEngine);
  case InputFile::GNUArchiveFileKind:
    return make<ArchiveFile>(I, DiagEngine);
  case InputFile::ELFSymDefFileKind:
    return make<SymDefFile>(I, DiagEngine);
  case InputFile::GNULinkerScriptKind:
    return make<LinkerScriptFile>(I, DiagEngine);
  case InputFile::BinaryFileKind:
    return make<BinaryFile>(I, DiagEngine);
  default:
    return nullptr;
  }
}

const char *InputFile::getCopyForWrite(uint32_t S, uint32_t E) {
  llvm::StringRef Region = getSlice(S, E);
  const char *Buf = getUninitBuffer(Region.size());
  std::memcpy((void *)Buf, Region.data(), Region.size());
  return Buf;
}

void InputFile::setUsed(bool Used) {
  std::lock_guard<std::mutex> Guard(Mutex);
  Input *I = getInput();
  if (I->isArchiveMember()) {
    ArchiveMemberInput *AMI = llvm::dyn_cast<eld::ArchiveMemberInput>(I);
    AMI->getArchiveFile()->setUsed(Used);
  }
  m_Used = true;
}

bool InputFile::isUsed() {
  std::lock_guard<std::mutex> Guard(Mutex);
  return m_Used;
}

bool InputFile::isInternal() const {
  return m_Kind == InternalInputKind || (m_Input && m_Input->isInternal());
}
