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

BitcodeFile::BitcodeFile(Input *I, DiagnosticEngine *diagEngine)
    : ObjectFile(I, InputFile::BitcodeFileKind, diagEngine) {
  if (I->getSize())
    Contents = I->getFileContents();
  m_DiagEngine = diagEngine;
}

BitcodeFile::~BitcodeFile() {}

/// Helper function to create a LTO module from a file.
bool BitcodeFile::createLTOInputFile(const std::string &ModuleID) {

  m_ModuleID = ModuleID;

  llvm::Expected<std::unique_ptr<llvm::lto::InputFile>> IFOrErr =
      llvm::lto::InputFile::create(llvm::MemoryBufferRef(Contents, m_ModuleID));
  if (!IFOrErr) {
    m_DiagEngine->raise(diag::fatal_cannot_make_module)
        << getInput()->decoratedPath() << llvm::toString(IFOrErr.takeError());
    return false;
  }

  m_LTOInputFile = std::move(*IFOrErr);
  return true;
}

std::unique_ptr<llvm::lto::InputFile> BitcodeFile::takeLTOInputFile() {
  return std::move(m_LTOInputFile);
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

void BitcodeFile::releaseMemory(bool isVerbose) {
  assert(!m_LTOInputFile);
  Input *I = getInput();
  if (m_DiagEngine->getPrinter()->isVerbose())
    m_DiagEngine->raise(diag::release_memory_bitcode) << I->decoratedPath();
  if (I->getInputType() != Input::ArchiveMember) {
    I->releaseMemory(isVerbose);
    return;
  }
  ArchiveMemberInput *AMI = llvm::dyn_cast<eld::ArchiveMemberInput>(I);
  // Some one already destroyed it!
  if (AMI->getArchiveFile()->isAlreadyReleased())
    return;
  AMI->getArchiveFile()->releaseMemory(isVerbose);
}

bool BitcodeFile::CreatePluginModule(plugin::LinkerPlugin &Plugin,
                                     uint64_t ModuleHash) {
  plugin::LTOModule *Module =
      Plugin.CreateLTOModule(plugin::BitcodeFile(*this), ModuleHash);
  if (!Module)
    return false;
  m_PluginModule = Module;
  return true;
}

void BitcodeFile::setInputSectionForSymbol(const ResolveInfo &R, Section &S) {
  m_InputSectionForSymbol[&R] = &S;
}

Section *BitcodeFile::getInputSectionForSymbol(const ResolveInfo &R) const {
  auto It = m_InputSectionForSymbol.find(&R);
  return It != m_InputSectionForSymbol.end() ? It->second : nullptr;
}
