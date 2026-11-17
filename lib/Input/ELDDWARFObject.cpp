//===- ELDDWARFObject.cpp--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "ELDDWARFObject.h"
#include "eld/Diagnostics/Diagnostic.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/Input/Input.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/DebugInfo/DWARF/DWARFRelocMap.h"
#include "llvm/Object/Decompressor.h"
#include "llvm/Object/ELF.h"
#include "llvm/Support/Compression.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/Error.h"
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

using namespace llvm;
using namespace llvm::object;

namespace eld {
namespace {

constexpr uint64_t MaxDecompressedDebugSectionSize = 1ULL << 30;

std::unique_ptr<plugin::DiagnosticEntry>
makeDWARFUnavailableDiagnostic(StringRef FileName, StringRef Reason) {
  return std::make_unique<plugin::DiagnosticEntry>(
      Diag::verbose_dwarf_object_unavailable,
      std::vector<std::string>{FileName.str(), Reason.str()});
}

template <typename T>
eld::Expected<T> convertLLVMExpected(llvm::Expected<T> Result,
                                     StringRef FileName) {
  if (!Result)
    return makeDWARFUnavailableDiagnostic(FileName,
                                          llvm::toString(Result.takeError()));
  return std::move(*Result);
}

/// Adapts the DWARF sections in an ELD ELF input for LLVM's DWARFContext.
/// Uncompressed section contents are views into the input buffer, which must
/// outlive the context using this object. Decompressed contents are owned by
/// this adapter.
template <class ELFT> class ELDDWARFObject final : public DWARFObject {
  using ELFFile = llvm::object::ELFFile<ELFT>;
  using Shdr = typename ELFT::Shdr;

public:
  ELDDWARFObject(const ELFObjectFile &InputFile, ELFFile File)
      : File(std::move(File)), FileName(InputFile.getInput()->getName()) {}

  /// Read the ELF section table and expose the supported DWARF sections.
  eld::Expected<void> initialize() {
    eld::Expected<typename ELFT::ShdrRange> SectionsOrErr =
        convertLLVMExpected(File.sections(), FileName);
    if (!SectionsOrErr)
      return std::move(SectionsOrErr.error());
    auto Sections = *SectionsOrErr;
    eld::Expected<StringRef> SectionNamesOrErr =
        convertLLVMExpected(File.getSectionStringTable(Sections), FileName);
    if (!SectionNamesOrErr)
      return std::move(SectionNamesOrErr.error());

    for (size_t I = 0; I != Sections.size(); ++I) {
      eld::Expected<StringRef> NameOrErr = convertLLVMExpected(
          File.getSectionName(Sections[I], *SectionNamesOrErr), FileName);
      if (!NameOrErr)
        return std::move(NameOrErr.error());
      eld::Expected<void> AddSectionResult =
          addSection(*NameOrErr, Sections[I]);
      if (!AddSectionResult)
        return std::move(AddSectionResult.error());
    }

    return {};
  }

  StringRef getFileName() const override { return FileName; }
  uint8_t getAddressSize() const override { return ELFT::Is64Bits ? 8 : 4; }
  bool isLittleEndian() const override { return true; }

  void forEachInfoSections(
      function_ref<void(const DWARFSection &)> F) const override {
    if (HasInfoSection)
      F(InfoSection);
  }

  StringRef getAbbrevSection() const override { return AbbrevSection; }
  const DWARFSection &getLineSection() const override { return LineSection; }
  const DWARFSection &getStrOffsetsSection() const override {
    return StrOffsetsSection;
  }
  StringRef getStrSection() const override { return StrSection; }
  StringRef getLineStrSection() const override { return LineStrSection; }

  /// Relocation lookup is not provided by this adapter.
  std::optional<RelocAddrEntry> find(const DWARFSection &,
                                     uint64_t) const override {
    return std::nullopt;
  }

private:
  /// Return section bytes, decompressing ELF and GNU compressed debug
  /// sections into storage owned by this adapter.
  eld::Expected<StringRef> sectionData(StringRef Name, const Shdr &Section) {
    eld::Expected<ArrayRef<uint8_t>> ContentsOrErr =
        convertLLVMExpected(File.getSectionContents(Section), FileName);
    if (!ContentsOrErr)
      return std::move(ContentsOrErr.error());

    StringRef Data = toStringRef(*ContentsOrErr);
    const bool IsGNUCompressed = Name.starts_with(".zdebug_");
    const bool IsELFCompressed = Section.sh_flags & ELF::SHF_COMPRESSED;
    if (!IsGNUCompressed && !IsELFCompressed)
      return Data;

    auto Decompressed = std::make_unique<std::string>();
    if (IsELFCompressed) {
      eld::Expected<Decompressor> DecompressorOrErr = convertLLVMExpected(
          Decompressor::create(Name, Data, /*IsLE=*/true, ELFT::Is64Bits),
          FileName);
      if (!DecompressorOrErr)
        return std::move(DecompressorOrErr.error());
      uint64_t DecompressedSize = DecompressorOrErr->getDecompressedSize();
      if (DecompressedSize > std::numeric_limits<size_t>::max() ||
          DecompressedSize > MaxDecompressedDebugSectionSize)
        return makeDWARFUnavailableDiagnostic(
            FileName, "compressed debug section is too large");
      if (llvm::Error Err =
              DecompressorOrErr->resizeAndDecompress(*Decompressed))
        return makeDWARFUnavailableDiagnostic(FileName,
                                              llvm::toString(std::move(Err)));
    } else {
      // GNU's .zdebug format has a "ZLIB" signature, an eight-byte
      // big-endian uncompressed size, and then the zlib-compressed data.
      constexpr size_t HeaderSize = 12;
      if (Data.size() < HeaderSize || !Data.starts_with("ZLIB"))
        return makeDWARFUnavailableDiagnostic(
            FileName, "invalid GNU compressed debug section");
      uint64_t DecompressedSize = support::endian::read64be(Data.data() + 4);
      constexpr uint64_t MaxExpansionRatio = 1032;
      uint64_t CompressedSize = Data.size() - HeaderSize;
      uint64_t MinCompressedSize = DecompressedSize / MaxExpansionRatio +
                                   (DecompressedSize % MaxExpansionRatio != 0);
      if (DecompressedSize > std::numeric_limits<size_t>::max() ||
          DecompressedSize > MaxDecompressedDebugSectionSize ||
          CompressedSize < MinCompressedSize)
        return makeDWARFUnavailableDiagnostic(
            FileName, "GNU compressed debug section is too large");
      Decompressed->resize(static_cast<size_t>(DecompressedSize));
      size_t ActualSize = Decompressed->size();
      if (llvm::Error Err = compression::zlib::decompress(
              arrayRefFromStringRef(Data.drop_front(HeaderSize)),
              reinterpret_cast<uint8_t *>(Decompressed->data()), ActualSize))
        return makeDWARFUnavailableDiagnostic(FileName,
                                              llvm::toString(std::move(Err)));
      Decompressed->resize(ActualSize);
    }

    DecompressedSections.push_back(std::move(Decompressed));
    const std::string &OwnedData = *DecompressedSections.back();
    return StringRef(OwnedData.data(), OwnedData.size());
  }

  eld::Expected<void> setSection(DWARFSection &Section, StringRef Name,
                                 const Shdr &RawSection) {
    eld::Expected<StringRef> DataOrErr = sectionData(Name, RawSection);
    if (!DataOrErr)
      return std::move(DataOrErr.error());
    Section.Data = *DataOrErr;
    return {};
  }

  /// Map a DWARF section name to the section representation required by LLVM.
  eld::Expected<void> addSection(StringRef Name, const Shdr &RawSection) {
    StringRef OriginalName = Name;
    if (Name.starts_with(".zdebug_"))
      Name = Name.drop_front(2);
    else if (Name.starts_with("."))
      Name = Name.drop_front();

    // Info and line sections use DWARFSection; the remaining sections are
    // exposed to LLVM as raw byte tables.
    if (Name == "debug_info") {
      eld::Expected<void> Result =
          setSection(InfoSection, OriginalName, RawSection);
      if (!Result)
        return std::move(Result.error());
      HasInfoSection = true;
    } else if (Name == "debug_abbrev") {
      eld::Expected<StringRef> DataOrErr =
          sectionData(OriginalName, RawSection);
      if (!DataOrErr)
        return std::move(DataOrErr.error());
      AbbrevSection = *DataOrErr;
    } else if (Name == "debug_line") {
      eld::Expected<void> Result =
          setSection(LineSection, OriginalName, RawSection);
      if (!Result)
        return std::move(Result.error());
    } else if (Name == "debug_line_str") {
      eld::Expected<StringRef> DataOrErr =
          sectionData(OriginalName, RawSection);
      if (!DataOrErr)
        return std::move(DataOrErr.error());
      LineStrSection = *DataOrErr;
    } else if (Name == "debug_str") {
      eld::Expected<StringRef> DataOrErr =
          sectionData(OriginalName, RawSection);
      if (!DataOrErr)
        return std::move(DataOrErr.error());
      StrSection = *DataOrErr;
    } else if (Name == "debug_str_offsets") {
      eld::Expected<void> Result =
          setSection(StrOffsetsSection, OriginalName, RawSection);
      if (!Result)
        return std::move(Result.error());
    }
    return {};
  }

  // The ELF file and any uncompressed section data refer to the original
  // input buffer. DecompressedSections owns data for compressed sections.
  ELFFile File;
  std::string FileName;
  DWARFSection InfoSection;
  bool HasInfoSection = false; // Do not expose a missing info section.
  StringRef AbbrevSection;
  StringRef StrSection;
  StringRef LineStrSection;
  DWARFSection LineSection;
  DWARFSection StrOffsetsSection;
  std::vector<std::unique_ptr<std::string>> DecompressedSections;
};

template <class ELFT>
eld::Expected<std::unique_ptr<const DWARFObject>>
createELDDWARFObjectForELFT(const ELFObjectFile &Input) {
  // Parse the input with the ELF class selected by the caller, then expose it
  // through LLVM's architecture-independent DWARFObject interface.
  std::string FileName = Input.getInput()->getName();
  eld::Expected<ELFFile<ELFT>> FileOrErr =
      convertLLVMExpected(ELFFile<ELFT>::create(Input.getContents()), FileName);
  if (!FileOrErr)
    return std::move(FileOrErr.error());
  auto Object =
      std::make_unique<ELDDWARFObject<ELFT>>(Input, std::move(*FileOrErr));
  eld::Expected<void> InitResult = Object->initialize();
  if (!InitResult)
    return std::move(InitResult.error());
  return std::unique_ptr<const DWARFObject>(std::move(Object));
}

} // namespace

eld::Expected<std::unique_ptr<const DWARFObject>>
detail::createELDDWARFObject(const ELFObjectFile &Input) {
  // Select an ELF parser specialization from the kind established by ELD's
  // normal input reader.
  switch (Input.getELFKind()) {
  case ObjectFile::ELF32LEKind:
    return createELDDWARFObjectForELFT<ELF32LE>(Input);
  case ObjectFile::ELF32BEKind:
  case ObjectFile::ELF64BEKind:
    return makeDWARFUnavailableDiagnostic(
        Input.getInput()->getName(), "big-endian ELF DWARF is unsupported");
  case ObjectFile::ELF64LEKind:
    return createELDDWARFObjectForELFT<ELF64LE>(Input);
  case ObjectFile::ELFNoneKind:
    return makeDWARFUnavailableDiagnostic(
        Input.getInput()->getName(), "input has no ELF format information");
  }
  llvm_unreachable("unknown ELF kind");
}

} // namespace eld
