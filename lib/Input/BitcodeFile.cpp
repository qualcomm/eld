//===- BitcodeFile.cpp-----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Input/BitcodeFile.h"
#include "eld/Diagnostics/DiagnosticPrinter.h"
#include "eld/Input/ArchiveFile.h"
#include "eld/Input/ArchiveMemberInput.h"
#include "eld/PluginAPI/LinkerPlugin.h"
#include "eld/Support/MsgHandling.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Object/ELF.h"
#include "llvm/Support/xxhash.h"

using namespace eld;

BitcodeFile::BitcodeFile(Input *I, DiagnosticEngine *PDiagEngine)
    : ObjectFile(I, InputFile::BitcodeFileKind, PDiagEngine) {
  if (I->getSize())
    Contents = I->getFileContents();
  DiagEngine = PDiagEngine;
}

BitcodeFile::~BitcodeFile() {}

/// Helper function to create a LTO module from a file.
bool BitcodeFile::createLTOInputFile(const std::string &PModuleID) {

  ModuleID = PModuleID;

  llvm::Expected<std::unique_ptr<llvm::lto::InputFile>> IFOrErr =
      llvm::lto::InputFile::create(llvm::MemoryBufferRef(Contents, ModuleID));
  if (!IFOrErr) {
    DiagEngine->raise(Diag::fatal_cannot_make_module)
        << getInput()->decoratedPath() << llvm::toString(IFOrErr.takeError());
    return false;
  }

  LTOInputFile = std::move(*IFOrErr);

  inferObjectInfo();

  return true;
}

std::unique_ptr<llvm::lto::InputFile> BitcodeFile::takeLTOInputFile() {
  return std::move(LTOInputFile);
}

bool BitcodeFile::canReleaseMemory() const {
  Input *I = getInput();
  if (!I->isArchiveMember())
    return true;
  ArchiveMemberInput *AMI = llvm::dyn_cast<eld::ArchiveMemberInput>(I);
  if (AMI->getArchiveFile()->isAlreadyReleased())
    return false;
  if (AMI->getArchiveFile()->isBitcodeArchive())
    return true;
  return false;
}

void BitcodeFile::releaseMemory(bool IsVerbose) {
  assert(!LTOInputFile);
  Input *I = getInput();
  if (DiagEngine->getPrinter()->isVerbose())
    DiagEngine->raise(Diag::release_memory_bitcode) << I->decoratedPath();
  if (I->getInputType() != Input::ArchiveMember) {
    I->releaseMemory(IsVerbose);
    return;
  }
  ArchiveMemberInput *AMI = llvm::dyn_cast<eld::ArchiveMemberInput>(I);
  // Some one already destroyed it!
  if (AMI->getArchiveFile()->isAlreadyReleased())
    return;
  AMI->getArchiveFile()->releaseMemory(IsVerbose);
}

bool BitcodeFile::createPluginModule(plugin::LinkerPlugin &Plugin,
                                     uint64_t ModuleHash) {
  plugin::LTOModule *Module =
      Plugin.CreateLTOModule(plugin::BitcodeFile(*this), ModuleHash);
  if (!Module)
    return false;
  PluginModule = Module;
  return true;
}

void BitcodeFile::setInputSectionForSymbol(const ResolveInfo &R, Section &S) {
  InputSectionForSymbol[&R] = &S;
}

Section *BitcodeFile::getInputSectionForSymbol(const ResolveInfo &R) const {
  auto It = InputSectionForSymbol.find(&R);
  return It != InputSectionForSymbol.end() ? It->second : nullptr;
}

uint16_t BitcodeFile::inferMachine(const llvm::Triple &t) const {
  switch (t.getArch()) {
  case llvm::Triple::aarch64:
    return llvm::ELF::EM_AARCH64;
  case llvm::Triple::arm:
  case llvm::Triple::thumb:
    return llvm::ELF::EM_ARM;
  case llvm::Triple::hexagon:
    return llvm::ELF::EM_HEXAGON;
  case llvm::Triple::riscv32:
  case llvm::Triple::riscv64:
    return llvm::ELF::EM_RISCV;
  case llvm::Triple::x86_64:
    return llvm::ELF::EM_X86_64;
  default:
    DiagEngine->raise(Diag::fatal_unsupported_bit_code_file)
        << t.getArchName() << getInput()->decoratedPath();
    break;
  }
  return llvm::ELF::EM_NONE;
}

ObjectFile::ELFKind BitcodeFile::inferELFKind(const llvm::Triple &t) const {
  if (t.isLittleEndian())
    return t.isArch64Bit() ? ObjectFile::ELFKind::ELF64LEKind
                           : ObjectFile::ELFKind::ELF32LEKind;
  return t.isArch64Bit() ? ObjectFile::ELFKind::ELF64BEKind
                         : ObjectFile::ELFKind::ELF32BEKind;
}

uint8_t BitcodeFile::inferOSABI(const llvm::Triple &t) const {
  return llvm::ELF::ELFOSABI_NONE;
}

void BitcodeFile::inferObjectInfo() {
  llvm::Triple LTOObjTriple(LTOInputFile->getTargetTriple());
  setELFKind(inferELFKind(LTOObjTriple));
  setMachine(inferMachine(LTOObjTriple));
  setOSABI(inferOSABI(LTOObjTriple));
}
