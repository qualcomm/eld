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
    enum Status { Include, Exclude, Unknown };

    enum Type { NoType, DefineData, DefineFunction, Common, Weak };

    Symbol(const char *pName, uint32_t pOffset, Type pType, enum Status pStatus)
        : name(pName), fileOffset(pOffset), status(pStatus), type(pType) {}

    ~Symbol() {}

  public:
    std::string name;
    uint32_t fileOffset;
    enum Status status;
    Type type;
  };

  typedef std::vector<Symbol *> SymTabType;

public:
  ArchiveFile(Input *I, DiagnosticEngine *diagEngine);

  /// Archive Kind Support.
  enum ArchiveKind { ELF, Bitcode, Mixed, Unknown };

  void setArchiveKind(ArchiveKind K) { AKind = K; }

  ArchiveKind getArchiveKind() const { return AKind; }

  /// Helper functions for figuring out the Kind of Archive.
  bool isBitcodeArchive() const { return AKind == Bitcode; }

  bool isELFArchive() const { return AKind == ELF; }

  bool isMixedArchive() const { return AKind == Mixed; }

  // Returns all members of the archive file.
  std::vector<Input *> const &GetAllMembers() const;

  /// numOfSymbols - return the number of symbols in symtab
  size_t numOfSymbols() const;

  // Add a member.
  void AddMember(Input *I);

  // Get the Member file, given an offset.
  Input *getLazyLoadMember(off_t pFileOffset) const;

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

  void addSymbol(const char *pName, uint32_t pFileOffset,
                 enum Symbol::Type pType = ArchiveFile::Symbol::NoType,
                 enum Symbol::Status pStatus = ArchiveFile::Symbol::Unknown);

  /// getObjFileOffset - get the file offset that represent a object file
  uint32_t getObjFileOffset(size_t pSymIdx) const;

  /// getSymbolStatus - get the status of a symbol
  enum Symbol::Status getSymbolStatus(size_t pSymIdx) const;

  /// setSymbolStatus - set the status of a symbol
  void setSymbolStatus(size_t pSymIdx, enum Symbol::Status pStatus);

  Input *createMemberInput(llvm::StringRef memberName, MemoryArea *data,
                           off_t childOffset);

  /// -------------------------Export/NoExport---------------------------
  bool noExport() const { return m_bNoExport; }

  void setNoExport(bool NoExport = true) { m_bNoExport = NoExport; }

  /// ------------------------Release Memory ---------------------------
  void releaseMemory(bool isVerbose = false);

  virtual ~ArchiveFile() = default;

  bool isAlreadyReleased() const;

  size_t getLoadedMemberCount() const;

  void incrementLoadedMemberCount();

  size_t getTotalMemberCount() const;

  bool isThin() const { return m_IsThin; }

  bool isThinArchive() const override { return m_IsThin; }

  void setThinArchive() { m_IsThin = true; }

  void initArchiveFileInfo();

  void setArchiveFileInfo(ArchiveFileInfo *AFI) { m_AFI = AFI; }

  ArchiveFileInfo *getArchiveFileInfo() const { return m_AFI; }

private:
  ArchiveFileInfo *m_AFI = nullptr;
  size_t m_ProcessedMemberCount = 0;
  bool m_bNoExport = false;
  ArchiveKind AKind = Unknown;
  bool m_IsThin = false;
};

} // namespace eld

#endif // ELD_INPUT_ARCHIVEFILE_H
