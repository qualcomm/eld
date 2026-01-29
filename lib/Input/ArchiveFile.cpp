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

ArchiveFile::ArchiveFile(Input *I, DiagnosticEngine *DiagEngine)
    : InputFile(I, DiagEngine, InputFile::GNUArchiveFileKind) {
  // FIXME: We should be more strict with usage of 'Input' while creating
  // an 'InputFile'. The 'if' check in constructor is very misleading as now the
  // user can not be sure if the attributes will be set properly or not.
  // Ideally, we should only allow 'Input' to create an 'InputFile' if the
  // 'Input' object has been prepared/initialized.
  if (I->getSize())
    Contents = I->getFileContents();
}

// Get the Input file from the Lazy Load Member map.
Input *ArchiveFile::getLazyLoadMember(off_t PFileOffset) const {
  ASSERT(AFI, "AFI must not be null!");

  auto I = AFI->LazyLoadMemberMap.find(PFileOffset);
  if (I == AFI->LazyLoadMemberMap.end())
    return nullptr;
  return I->second;
}

/// getSymbolTable - get the symtab
ArchiveFile::SymTabType &ArchiveFile::getSymbolTable() {
  ASSERT(AFI, "AFI must not be null!");
  return AFI->SymTab;
}

/// numOfSymbols - return the number of symbols in symtab
size_t ArchiveFile::numOfSymbols() const {
  ASSERT(AFI, "AFI must not be null!");
  return AFI->SymTab.size();
}

/// addSymbol - add a symtab entry to symtab
/// @param Name - symbol name
/// @param FileOffset - file offset in symtab represents a object file
void ArchiveFile::addSymbol(llvm::StringRef Name, uint32_t FileOffset,
                            uint8_t Type, uint8_t Status) {
  ASSERT(AFI, "AFI must not be null!");
  [[maybe_unused]] const char *Base = getContents().data();
  ASSERT(Name.data() >= Base &&
             (Name.data() + Name.size()) <= (Base + getContents().size()),
         "archive symbol name must reference archive contents");
  uint32_t NameOffset = (Name.data() - getContents().data());
  uint32_t NameSize = Name.size();
  AFI->SymTab.emplace_back(Symbol{NameOffset, NameSize, FileOffset,
                                  static_cast<Symbol::SymbolStatus>(Status),
                                  static_cast<Symbol::SymbolType>(Type)});
}

/// getObjFileOffset - get the file offset that represent a object file
uint32_t ArchiveFile::getObjFileOffset(size_t SymIdx) const {
  ASSERT(AFI, "AFI must not be null!");
  return AFI->SymTab[SymIdx].FileOffset;
}

/// getSymbolStatus - get the status of a symbol
ArchiveFile::Symbol::SymbolStatus
ArchiveFile::getSymbolStatus(size_t PSymIdx) const {
  ASSERT(AFI, "AFI must not be null!");
  return AFI->SymTab[PSymIdx].Status;
}

/// setSymbolStatus - set the status of a symbol
void ArchiveFile::setSymbolStatus(
    size_t SymIdx, enum ArchiveFile::Symbol::SymbolStatus Status) {
  ASSERT(AFI, "AFI must not be null!");
  AFI->SymTab[SymIdx].Status = Status;
}

Input *ArchiveFile::createMemberInput(llvm::StringRef MemberName,
                                      MemoryArea *Data, off_t ChildOffset) {
  Input *Inp = ArchiveMemberInput::create(DiagEngine, this, MemberName, Data,
                                          ChildOffset);
  return Inp;
}

// Returns all members of the archive file.
const std::vector<Input *> &ArchiveFile::getAllMembers() const {
  ASSERT(AFI, "AFI must not be null!");
  return AFI->Members;
}

// Add a member.
void ArchiveFile::addMember(Input *I) {
  ASSERT(AFI, "AFI must not be null!");
  AFI->Members.push_back(std::move(I));
}

// Add a member that needs to be loaded.
void ArchiveFile::addLazyLoadMember(off_t FileOffset, Input *I) {
  ASSERT(AFI, "AFI must not be null!");
  AFI->LazyLoadMemberMap[FileOffset] = I;
}

// Does the archive have Members already parsed ?
bool ArchiveFile::hasMembers() const {
  if (!AFI)
    return false;
  return (AFI->Members.size() > 0);
}

void ArchiveFile::initArchiveFileInfo() { AFI = make<ArchiveFileInfo>(); }

size_t ArchiveFile::getLoadedMemberCount() const {
  return ProcessedMemberCount;
}

void ArchiveFile::incrementLoadedMemberCount() { ProcessedMemberCount++; }

size_t ArchiveFile::getTotalMemberCount() const {
  if (!AFI)
    return 0;
  return AFI->Members.size();
}
