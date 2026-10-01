//===- InputTarReader.cpp--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Support/InputTarReader.h"

#include "eld/Config/LinkerConfig.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/PluginAPI/DiagnosticEntry.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include <optional>

namespace eld {

namespace {

constexpr size_t TarBlockSize = 512;
// Tar header layout used here:
// - Common record structure (both formats):
//   [512-byte header][file payload][NUL padding to 512-byte boundary].
//   Archives end with zero-filled 512-byte blocks (typically two).
// - V7 tar ("Version 7" UNIX tar):
//   Uses the base header fields only. Path is stored in `name` (up to 100
//   bytes). Numeric fields (for example `size`) are ASCII octal text.
//   Regular files are commonly identified by typeflag '\0'.
// - USTAR/POSIX tar:
//   Keeps the same 512-byte base header, and adds extra fields near the end:
//   `magic`/`version` plus `prefix` for path extension.
//   Path is reconstructed as `prefix` + "/" + `name` when `prefix` is present
//   (`name` is 100 bytes and `prefix` is 155 bytes).
//   Regular files are commonly identified by typeflag '0'.
// - Parser scope in this file:
//   We consume only what we need for linking workflows: `name`, `size`,
//   `typeflag`, and `prefix`; parse numeric values as octal; advance each entry
//   by payload size rounded up to 512-byte alignment; and keep only regular
//   files in the in-memory map.
constexpr size_t NameOffset = 0;
constexpr size_t NameSize = 100;
constexpr size_t SizeOffset = 124;
constexpr size_t SizeSize = 12;
constexpr size_t TypeFlagOffset = 156;
constexpr size_t PrefixOffset = 345;
constexpr size_t PrefixSize = 155;

static bool isZeroBlock(llvm::StringRef Block) {
  for (char C : Block) {
    if (C != '\0')
      return false;
  }
  return true;
}

static std::string readTarString(llvm::StringRef Raw) {
  size_t End = Raw.find('\0');
  llvm::StringRef S = End == llvm::StringRef::npos ? Raw : Raw.take_front(End);
  return S.rtrim(' ').str();
}

static std::unique_ptr<plugin::DiagnosticEntry>
makeTarDiag(llvm::StringRef Msg) {
  return std::make_unique<plugin::DiagnosticEntry>(
      plugin::DiagnosticEntry(Diag::error_invalid_tar_archive, {Msg.str()}));
}

static eld::Expected<uint64_t> parseOctal(llvm::StringRef Raw,
                                          llvm::StringRef FieldName) {
  // Tar header numeric fields are stored as ASCII octal strings (ustar/V7),
  // so parse the trimmed field using base-8.
  size_t End = Raw.find('\0');
  llvm::StringRef S = End == llvm::StringRef::npos ? Raw : Raw.take_front(End);
  S = S.trim(" \t");
  if (S.empty())
    return 0;

  uint64_t Value = 0;
  if (S.getAsInteger(/*Radix=*/8, Value))
    return makeTarDiag(
        llvm::formatv("invalid {0} value '{1}'", FieldName, S).str());
  return Value;
}

static std::string getEntryName(llvm::StringRef Header) {
  std::string Name = readTarString(Header.substr(NameOffset, NameSize));
  std::string Prefix = readTarString(Header.substr(PrefixOffset, PrefixSize));
  if (Prefix.empty())
    return Name;
  if (Name.empty())
    return Prefix;
  return Prefix + "/" + Name;
}

static uint64_t alignTarPayload(uint64_t Size) {
  // TAR payload is padded to a 512-byte boundary before the next header.
  return (Size + TarBlockSize - 1) & ~(TarBlockSize - 1);
}

template <typename CallbackT>
static eld::Expected<void> forEachEntry(llvm::StringRef TarData,
                                        CallbackT Callback) {
  size_t Offset = 0;
  while (Offset + TarBlockSize <= TarData.size()) {
    // Each entry starts with a fixed-size header block.
    llvm::StringRef Header = TarData.substr(Offset, TarBlockSize);
    Offset += TarBlockSize;
    if (isZeroBlock(Header))
      break;

    auto SizeOrErr = parseOctal(Header.substr(SizeOffset, SizeSize), "size");
    if (!SizeOrErr)
      return std::move(SizeOrErr.error());
    uint64_t Size = *SizeOrErr;
    uint64_t AlignedSize = alignTarPayload(Size);
    if (Offset + AlignedSize > TarData.size())
      return makeTarDiag(llvm::formatv("entry at offset {0} exceeds archive "
                                       "size",
                                       Offset)
                             .str());

    // Callback receives the logical payload bytes only (no alignment padding).
    size_t PayloadOffset = Offset;
    auto E = Callback(Header, TarData.substr(Offset, Size), PayloadOffset);
    if (!E)
      return std::move(E.error());
    Offset += static_cast<size_t>(AlignedSize);
  }
  return {};
}

} // namespace

eld::Expected<InputTarReader::FileMap>
InputTarReader::untar(llvm::StringRef TarData) {
  FileMap Files;
  auto E = forEachEntry(
      TarData, [&](llvm::StringRef Header, llvm::StringRef Payload, size_t) {
        std::string EntryName = getEntryName(Header);
        char TypeFlag = Header[TypeFlagOffset];

        // Both '\0' (V7 tar) and '0' (ustar/POSIX) encode
        // regular-file entries.
        // Keep only regular files in memory and skip
        // metadata/directory entries.
        if (TypeFlag == '\0' || TypeFlag == '0')
          Files[EntryName] = Payload.str();
        return eld::Expected<void>();
      });
  if (!E)
    return std::move(E.error());

  return Files;
}

eld::Expected<InputTarReader::FileMap>
InputTarReader::untarFile(llvm::StringRef TarPath) {
  auto BufferOrErr = llvm::MemoryBuffer::getFile(TarPath);
  if (!BufferOrErr) {
    std::error_code EC = BufferOrErr.getError();
    return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
        Diag::error_cannot_read_tar_file, {TarPath.str(), EC.message()}));
  }
  return untar((*BufferOrErr)->getBuffer());
}

eld::Expected<std::vector<std::string>>
InputTarReader::listEntryNames(llvm::StringRef TarData) {
  std::vector<std::string> Names;
  auto E = forEachEntry(TarData,
                        [&](llvm::StringRef Header, llvm::StringRef, size_t) {
                          Names.push_back(getEntryName(Header));
                          return eld::Expected<void>();
                        });
  if (!E)
    return std::move(E.error());
  return Names;
}

eld::Expected<std::vector<InputTarReader::EntryInfo>>
InputTarReader::listRegularFileEntries(llvm::StringRef TarData) {
  std::vector<EntryInfo> Entries;
  auto E =
      forEachEntry(TarData, [&](llvm::StringRef Header, llvm::StringRef Payload,
                                size_t PayloadOffset) {
        char TypeFlag = Header[TypeFlagOffset];
        if (TypeFlag != '\0' && TypeFlag != '0')
          return eld::Expected<void>();

        Entries.push_back(
            EntryInfo{getEntryName(Header), PayloadOffset, Payload.size()});
        return eld::Expected<void>();
      });
  if (!E)
    return std::move(E.error());
  return Entries;
}

eld::Expected<std::vector<std::string>>
InputTarReader::listEntryNamesFile(llvm::StringRef TarPath) {
  auto BufferOrErr = llvm::MemoryBuffer::getFile(TarPath);
  if (!BufferOrErr) {
    std::error_code EC = BufferOrErr.getError();
    return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
        Diag::error_cannot_read_tar_file, {TarPath.str(), EC.message()}));
  }
  return listEntryNames((*BufferOrErr)->getBuffer());
}

eld::Expected<void> InputTarReader::emitEntryNames(llvm::StringRef TarData,
                                                   llvm::raw_ostream &OS) {
  auto NamesOrErr = listEntryNames(TarData);
  if (!NamesOrErr)
    return std::move(NamesOrErr.error());
  for (const auto &Name : *NamesOrErr)
    OS << Name << '\n';
  return {};
}

eld::Expected<void> InputTarReader::emitEntryNamesFile(llvm::StringRef TarPath,
                                                       llvm::raw_ostream &OS) {
  auto NamesOrErr = listEntryNamesFile(TarPath);
  if (!NamesOrErr)
    return std::move(NamesOrErr.error());
  for (const auto &Name : *NamesOrErr)
    OS << Name << '\n';
  return {};
}

eld::Expected<llvm::MemoryBufferRef>
InputTarReader::findFile(llvm::StringRef TarData, llvm::StringRef FileName) {
  std::optional<llvm::MemoryBufferRef> Found;
  auto E = forEachEntry(
      TarData, [&](llvm::StringRef Header, llvm::StringRef Payload, size_t) {
        char TypeFlag = Header[TypeFlagOffset];
        // Both '\0' and '0' represent regular files in tar headers.
        if (TypeFlag != '\0' && TypeFlag != '0')
          return eld::Expected<void>();

        std::string EntryName = getEntryName(Header);
        llvm::StringRef EntryNameRef(EntryName);
        // TarWriter usually prefixes a base directory, so allow both exact
        // and suffix match (e.g. "reproduce.out/path/file" vs "file").
        std::string Suffix = ("/" + FileName).str();
        if (EntryNameRef == FileName || EntryNameRef.ends_with(Suffix)) {
          Found = llvm::MemoryBufferRef(Payload, EntryNameRef);
        }
        return eld::Expected<void>();
      });
  if (!E)
    return std::move(E.error());

  if (!Found)
    return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
        Diag::error_tar_file_not_found, {FileName.str()}));
  return *Found;
}

eld::Expected<void> InputTarReader::traceEntryNames(llvm::StringRef TarData,
                                                    LinkerConfig &Config) {
  if (!Config.getPrinter()->traceUntar())
    return {};

  auto NamesOrErr = listEntryNames(TarData);
  if (!NamesOrErr)
    return std::move(NamesOrErr.error());
  for (const auto &Name : *NamesOrErr)
    Config.raise(Diag::trace_untar_entry) << Name;
  return {};
}

eld::Expected<void> InputTarReader::traceEntryNamesFile(llvm::StringRef TarPath,
                                                        LinkerConfig &Config) {
  auto BufferOrErr = llvm::MemoryBuffer::getFile(TarPath);
  if (!BufferOrErr) {
    std::error_code EC = BufferOrErr.getError();
    return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
        Diag::error_cannot_read_tar_file, {TarPath.str(), EC.message()}));
  }
  return traceEntryNames((*BufferOrErr)->getBuffer(), Config);
}

} // namespace eld
