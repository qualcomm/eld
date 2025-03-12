//===- ArchiveFile.cpp-----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Input/ArchiveFile.h"
#include "eld/Diagnostics/DiagnosticPrinter.h"
#include "eld/Input/ArchiveFileInfo.h"
#include "eld/Input/ArchiveMemberInput.h"

using namespace eld;

ArchiveFile::ArchiveFile(Input *I, DiagnosticEngine *diagEngine)
    : InputFile(I, diagEngine, InputFile::GNUArchiveFileKind) {
  // FIXME: We should be more strict with usage of 'Input' while creating
  // an 'InputFile'. The 'if' check in constructor is very misleading as now the
  // user can not be sure if the attributes will be set properly or not.
  // Ideally, we should only allow 'Input' to create an 'InputFile' if the
  // 'Input' object has been prepared/initialized.
  if (I->getSize())
    Contents = I->getFileContents();
}

// Get the Input file from the Lazy Load Member map.
Input *ArchiveFile::getLazyLoadMember(off_t pFileOffset) const {
  ASSERT(m_AFI, "m_AFI must not be null!");

  auto I = m_AFI->m_LazyLoadMemberMap.find(pFileOffset);
  if (I == m_AFI->m_LazyLoadMemberMap.end())
    return nullptr;
  return I->second;
}

/// getSymbolTable - get the symtab
ArchiveFile::SymTabType &ArchiveFile::getSymbolTable() {
  ASSERT(m_AFI, "m_AFI must not be null!");
  return m_AFI->m_SymTab;
}

/// numOfSymbols - return the number of symbols in symtab
size_t ArchiveFile::numOfSymbols() const {
  ASSERT(m_AFI, "m_AFI must not be null!");
  return m_AFI->m_SymTab.size();
}

/// addSymbol - add a symtab entry to symtab
/// @param pName - symbol name
/// @param pFileOffset - file offset in symtab represents a object file
void ArchiveFile::addSymbol(const char *pName, uint32_t pFileOffset,
                            enum ArchiveFile::Symbol::Type pType,
                            enum ArchiveFile::Symbol::Status pStatus) {
  ASSERT(m_AFI, "m_AFI must not be null!");
  Symbol *entry = make<Symbol>(pName, pFileOffset, pType, pStatus);
  m_AFI->m_SymTab.push_back(entry);
}

/// getObjFileOffset - get the file offset that represent a object file
uint32_t ArchiveFile::getObjFileOffset(size_t pSymIdx) const {
  ASSERT(m_AFI, "m_AFI must not be null!");
  return m_AFI->m_SymTab[pSymIdx]->fileOffset;
}

/// getSymbolStatus - get the status of a symbol
ArchiveFile::Symbol::Status ArchiveFile::getSymbolStatus(size_t pSymIdx) const {
  ASSERT(m_AFI, "m_AFI must not be null!");
  return m_AFI->m_SymTab[pSymIdx]->status;
}

/// setSymbolStatus - set the status of a symbol
void ArchiveFile::setSymbolStatus(size_t pSymIdx,
                                  enum ArchiveFile::Symbol::Status pStatus) {
  ASSERT(m_AFI, "m_AFI must not be null!");
  m_AFI->m_SymTab[pSymIdx]->status = pStatus;
}

Input *ArchiveFile::createMemberInput(llvm::StringRef memberName,
                                      MemoryArea *data, off_t childOffset) {
  Input *inp = ArchiveMemberInput::Create(m_DiagEngine, this, memberName, data,
                                          childOffset);
  return inp;
}

void ArchiveFile::releaseMemory(bool isVerbose) {
  if (isELFArchive())
    return;
  if (m_Input)
    m_Input->releaseMemory(isVerbose);
}

bool ArchiveFile::isAlreadyReleased() const {
  return m_Input && m_Input->isAlreadyReleased();
}

// Returns all members of the archive file.
const std::vector<Input *> &ArchiveFile::GetAllMembers() const {
  ASSERT(m_AFI, "m_AFI must not be null!");
  return m_AFI->Members;
}

// Add a member.
void ArchiveFile::AddMember(Input *I) {
  ASSERT(m_AFI, "m_AFI must not be null!");
  m_AFI->Members.push_back(std::move(I));
}

// Add a member that needs to be loaded.
void ArchiveFile::addLazyLoadMember(off_t FileOffset, Input *I) {
  ASSERT(m_AFI, "m_AFI must not be null!");
  m_AFI->m_LazyLoadMemberMap[FileOffset] = I;
}

// Does the archive have Members already parsed ?
bool ArchiveFile::hasMembers() const {
  if (!m_AFI)
    return false;
  return (m_AFI->Members.size() > 0);
}

void ArchiveFile::initArchiveFileInfo() { m_AFI = make<ArchiveFileInfo>(); }

size_t ArchiveFile::getLoadedMemberCount() const {
  return m_ProcessedMemberCount;
}

void ArchiveFile::incrementLoadedMemberCount() { m_ProcessedMemberCount++; }

size_t ArchiveFile::getTotalMemberCount() const {
  if (!m_AFI)
    return 0;
  return m_AFI->Members.size();
}
