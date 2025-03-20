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
