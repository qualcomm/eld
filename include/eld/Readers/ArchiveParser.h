//===- ArchiveParser.h-----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_NEWREADERS_ARCHIVEPARSER_H
#define ELD_NEWREADERS_ARCHIVEPARSER_H
#include "eld/Input/ArchiveFile.h"
#include "eld/PluginAPI/Expected.h"
#include "llvm/ADT/Hashing.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Object/Archive.h"
#include "llvm/Object/Binary.h"
#include "llvm/Object/ELFObjectFile.h"
#include "llvm/Object/IRObjectFile.h"
#include <cstdint>
#include <unordered_map>

namespace eld {
class Module;
class ResolveInfo;

class ArchiveParser final {
public:
  explicit ArchiveParser(Module &module) : m_Module(module) {};

  // clang-format off
  /// Parses the archive file.
  ///
  /// The parsing consists of the following things in the same order as listed:
  /// - Reads symbol table. This computes the required symbol information for
  ///   each symbol in the archive symbol table, and adds the symbols to the
  ///   eld representation of the archive.
  /// - Sets the archive string table.
  /// - Adds archive members to the eld representation of the archive.
  /// - Finds the required symbols, and includes the archive members, that
  ///   contains the required symbols, in the link process.
  ///   In case of --whole-archive, all the archive members are included in
  ///   the link process.
  // clang-format on
  eld::Expected<uint32_t> parseFile(InputFile &inputFile) const;

  struct ArchiveSymbol {
    uint64_t ChildOffset = 0;
    llvm::StringRef SymbolName;

    ArchiveSymbol(uint64_t childOffset, llvm::StringRef symbolName)
        : ChildOffset(childOffset), SymbolName(symbolName) {}

    bool operator==(const ArchiveSymbol &other) const {
      return ChildOffset == other.ChildOffset && SymbolName == other.SymbolName;
    }
  };

private:
  using ArchiveSymbolInfoTable =
      std::unordered_map<ArchiveSymbol, ArchiveFile::Symbol::SymbolType>;

  Module &m_Module;

  /// Creates an LLVM object for reading an archive member.
  ///
  /// It internally reports diagnostics with less severity than an error.
  /// Diagnostics with severity of error or higher are returned to the caller
  /// function.
  ///
  /// If archive member is unsupported, then it internally reports
  /// a warning if the archive file warning configuration is enabled
  /// and returns nullptr.
  eld::Expected<std::unique_ptr<llvm::object::Binary>>
  createMemberReader(const ArchiveFile &archiveFile,
                     const llvm::object::Archive &archiveReader,
                     const llvm::object::Archive::Child &member) const;

  /// Adds all the symbols present in the archive symbol table to the eld
  /// representation of the archive.
  eld::Expected<bool>
  readSymbolTable(const llvm::object::Archive &archiveReader,
                  ArchiveFile *archive) const;

  /// Sets string table.
  eld::Expected<bool> setStringTable(llvm::object::Archive &archiveReader,
                                     ArchiveFile *archive) const;

  /// Computes symbol information table for all the symbols present in the
  /// archive symbol table.
  eld::Expected<ArchiveParser::ArchiveSymbolInfoTable>
  computeSymInfoTable(const llvm::object::Archive &archiveReader,
                      ArchiveFile *archive) const;

  /// Adds all the required symbols (symbols present in the archive symbol
  /// table) to symbolInfoTable with a placeholder value
  /// (ArchiveFile::Symbol::NoType).
  eld::Expected<bool> addReqSymbolsWithPlaceholderValue(
      const llvm::object::Archive &archiveReader,
      ArchiveSymbolInfoTable &symbolInfoTable) const;

  /// It computes symbol information for the provided archive member symbols
  /// that are present in the archive symbol table.
  eld::Expected<bool>
  computeSymInfoTableForMember(const ArchiveFile &archiveFile,
                               const llvm::object::Archive &archiveReader,
                               const llvm::object::Archive::Child &member,
                               ArchiveSymbolInfoTable &symbolInfoTable) const;

  /// It computes symbol information for the provided archive ELF member symbols
  /// that are present in the archive symbol table.
  template <class ELFT>
  eld::Expected<bool> computeSymInfoTableForELFMember(
      const llvm::object::ELFObjectFile<ELFT> &ELFObjFile,
      const llvm::object::Archive::Child &member,
      ArchiveSymbolInfoTable &symbolInfoTable) const;

  /// It computes symbol information for the provided archive IR member symbols
  /// that are present in the archive symbol table.
  eld::Expected<bool>
  computeSymInfoTableForIRMember(const llvm::object::IRObjectFile &IRObj,
                                 const llvm::object::Archive::Child &member,
                                 ArchiveSymbolInfoTable &symbolInfoTable) const;

  /// Adds all the archive members to the ArchiveFile.
  eld::Expected<bool> addMembers(llvm::object::Archive &archiveReader,
                                 ArchiveFile *archive) const;

  /// Create corresponding Input for the archive member.
  eld::Expected<Input *>
  createMemberInput(llvm::object::Archive &archiveReader,
                    const llvm::object::Archive::Child &member,
                    ArchiveFile *archive) const;

  /// Includes all the archive members of the archive in the link process.
  eld::Expected<uint32_t> includeAllMembers(ArchiveFile *archive) const;

  /// Includes the archive member in the link process.
  eld::Expected<bool> includeMember(Input *member) const;

  /// Determines if a symbol should be included or not.
  ArchiveFile::Symbol::SymbolStatus
  shouldIncludeSymbol(const ArchiveFile::Symbol &pSym, InputFile **pSite) const;

  /// Returns true if both info and type represents the same type of
  /// information.
  bool definedSameType(const ResolveInfo *info,
                       ArchiveFile::Symbol::SymbolType type) const;

  eld::Expected<MemoryArea *>
  getMemberData(const ArchiveFile &archiveFile,
                llvm::object::Archive &archiveReader,
                const llvm::object::Archive::Child &member) const;

  void warnRepeatedMembers(const ArchiveFile &archiveFile) const;
};

} // namespace eld

namespace std {
/// Hash data-structure for ArchiveSymbol. It is required for using
/// ArchiveSymbol as a key in std::unordered_map.
template <> struct hash<typename eld::ArchiveParser::ArchiveSymbol> {
  using Key = typename eld::ArchiveParser::ArchiveSymbol;
  std::size_t operator()(const Key &key) const noexcept {
    return llvm::hash_combine(key.ChildOffset, key.SymbolName);
  }
};
} // namespace std
#endif
