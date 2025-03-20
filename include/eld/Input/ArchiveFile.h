//===- ArchiveFile.h-------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#ifndef ELD_INPUT_ARCHIVEFILE_H
#define ELD_INPUT_ARCHIVEFILE_H

#include "eld/Input/InputFile.h"
#include "eld/Support/MemoryArea.h"
#include "llvm/ADT/DenseMap.h"
#include <cstddef>
#include <optional>
#include <vector>

namespace eld {
struct ArchiveFileInfo;
class DiagnosticPrinter;
class ArchiveFile : public InputFile {
public:
  // -----------Declare Types ----------------
  struct Symbol {
    enum SymbolStatus { Include, Exclude, Unknown };

    enum SymbolType { NoType, DefineData, DefineFunction, Common, Weak };

    Symbol(const char *PName, uint32_t POffset, SymbolType PType,
           enum SymbolStatus PStatus)
        : Name(PName), FileOffset(POffset), Status(PStatus), Type(PType) {}

    ~Symbol() {}

  public:
    std::string Name;
    uint32_t FileOffset;
    enum SymbolStatus Status;
    SymbolType Type;
  };

  typedef std::vector<Symbol *> SymTabType;

public:
  ArchiveFile(Input *I, DiagnosticEngine *DiagEngine);

  /// Archive Kind Support.
  enum ArchiveKind { ELF, Bitcode, Mixed, Unknown };

  void setArchiveKind(ArchiveKind K) { AKind = K; }

  ArchiveKind getArchiveKind() const { return AKind; }

  /// Helper functions for figuring out the Kind of Archive.
  bool isBitcodeArchive() const { return AKind == Bitcode; }

  bool isELFArchive() const { return AKind == ELF; }

  bool isMixedArchive() const { return AKind == Mixed; }

  // Returns all members of the archive file.
  std::vector<Input *> const &getAllMembers() const;

  /// numOfSymbols - return the number of symbols in symtab
  size_t numOfSymbols() const;

  // Add a member.
  void addMember(Input *I);

  // Get the Member file, given an offset.
  Input *getLazyLoadMember(off_t PFileOffset) const;

  // Add a member that needs to be loaded.
  void addLazyLoadMember(off_t FileOffset, Input *I);

  // Does the archive have Members already parsed ?
  bool hasMembers() const;

  /// Casting support.
  static bool classof(const InputFile *I) {
    return (I->getKind() == InputFile::GNUArchiveFileKind);
  }

  /// getSymbolTable - get the symtab
  SymTabType &getSymbolTable();

  void
  addSymbol(const char *PName, uint32_t PFileOffset,
            enum Symbol::SymbolType PType = ArchiveFile::Symbol::NoType,
            enum Symbol::SymbolStatus PStatus = ArchiveFile::Symbol::Unknown);

  /// getObjFileOffset - get the file offset that represent a object file
  uint32_t getObjFileOffset(size_t PSymIdx) const;

  /// getSymbolStatus - get the status of a symbol
  enum Symbol::SymbolStatus getSymbolStatus(size_t PSymIdx) const;

  /// setSymbolStatus - set the status of a symbol
  void setSymbolStatus(size_t PSymIdx, enum Symbol::SymbolStatus PStatus);

  Input *createMemberInput(llvm::StringRef MemberName, MemoryArea *Data,
                           off_t ChildOffset);

  /// -------------------------Export/NoExport---------------------------
  bool noExport() const { return BNoExport; }

  void setNoExport(bool NoExport = true) { BNoExport = NoExport; }

  /// ------------------------Release Memory ---------------------------
  void releaseMemory(bool IsVerbose = false);

  virtual ~ArchiveFile() = default;

  bool isAlreadyReleased() const;

  size_t getLoadedMemberCount() const;

  void incrementLoadedMemberCount();

  size_t getTotalMemberCount() const;

  bool isThin() const { return IsThin; }

  bool isThinArchive() const override { return IsThin; }

  void setThinArchive() { IsThin = true; }

  void initArchiveFileInfo();

  void setArchiveFileInfo(ArchiveFileInfo *PAFI) { AFI = PAFI; }

  ArchiveFileInfo *getArchiveFileInfo() const { return AFI; }

private:
  ArchiveFileInfo *AFI = nullptr;
  size_t ProcessedMemberCount = 0;
  bool BNoExport = false;
  ArchiveKind AKind = Unknown;
  bool IsThin = false;
};

} // namespace eld

#endif // ELD_INPUT_ARCHIVEFILE_H
