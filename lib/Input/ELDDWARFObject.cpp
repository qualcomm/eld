//===- ELDDWARFObject.cpp--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "ELDDWARFObject.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/Input/Input.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/DebugInfo/DWARF/DWARFRelocMap.h"
#include "llvm/Object/ELF.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using namespace llvm;
using namespace llvm::object;

namespace eld {
namespace {

/// Adapts the DWARF sections in an ELD ELF input for LLVM's DWARFContext.
/// Section contents are views into the input buffer, which must outlive the
/// context using this object.
template <class ELFT> class ELDDWARFObject final : public DWARFObject {
  using ELFFile = llvm::object::ELFFile<ELFT>;
  using Shdr = typename ELFT::Shdr;
  using Sym = typename ELFT::Sym;
  using Rel = typename ELFT::Rel;
  using Rela = typename ELFT::Rela;

  struct RelocationSectionInfo {
    const Shdr *Section;             // REL or RELA section header.
    typename ELFT::SymRange Symbols; // Symbols from sh_link's table.
    bool IsRela;                     // Whether addends are explicit.
  };

public:
  ELDDWARFObject(const ELFObjectFile &InputFile, ELFFile File)
      : File(std::move(File)), FileName(InputFile.getInput()->getName()) {}

  /// Read the ELF section table, expose the supported DWARF sections, and
  /// index relocation sections by their target section.
  llvm::Error initialize() {
    llvm::Expected<typename ELFT::ShdrRange> SectionsOrErr = File.sections();
    if (!SectionsOrErr)
      return SectionsOrErr.takeError();
    Sections = *SectionsOrErr;
    SectionsByIndex.resize(Sections.size());

    llvm::Expected<StringRef> SectionNamesOrErr =
        File.getSectionStringTable(Sections);
    if (!SectionNamesOrErr)
      return SectionNamesOrErr.takeError();

    for (size_t I = 0; I != Sections.size(); ++I) {
      llvm::Expected<StringRef> NameOrErr =
          File.getSectionName(Sections[I], *SectionNamesOrErr);
      if (!NameOrErr) {
        consumeError(NameOrErr.takeError());
        continue;
      }
      addSection(*NameOrErr, Sections[I], I);
    }

    addRelocationSections();
    return Error::success();
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

  /// Find the relocation entry or entries at Pos in Section. LLVM's
  /// RelocAddrEntry can represent up to two relocation components.
  std::optional<RelocAddrEntry> find(const DWARFSection &Section,
                                     uint64_t Pos) const override {
    auto SectionIt = SectionIndices.find(&Section);
    if (SectionIt == SectionIndices.end())
      return std::nullopt;

    std::optional<RelocAddrEntry> Result;
    for (const RelocationSectionInfo &RelocSection :
         RelocationsByTarget[SectionIt->second]) {
      std::optional<RelocAddrEntry> Entry;
      if (RelocSection.IsRela) {
        llvm::Expected<typename ELFT::RelaRange> RelocationsOrErr =
            File.relas(*RelocSection.Section);
        if (!RelocationsOrErr) {
          consumeError(RelocationsOrErr.takeError());
          continue;
        }
        Entry =
            findRelocation<Rela, true>(RelocSection, *RelocationsOrErr, Pos);
      } else {
        llvm::Expected<typename ELFT::RelRange> RelocationsOrErr =
            File.rels(*RelocSection.Section);
        if (!RelocationsOrErr) {
          consumeError(RelocationsOrErr.takeError());
          continue;
        }
        Entry =
            findRelocation<Rel, false>(RelocSection, *RelocationsOrErr, Pos);
      }
      if (!Entry)
        continue;
      if (!Result) {
        Result = std::move(Entry);
      } else if (!Result->Reloc2) {
        Result->Reloc2 = Entry->Reloc;
        Result->SymbolValue2 = Entry->SymbolValue;
      }
    }
    return Result;
  }

private:
  /// Return a non-owning view of an ELF section's bytes.
  StringRef sectionData(const Shdr &Section) const {
    llvm::Expected<ArrayRef<uint8_t>> ContentsOrErr =
        File.getSectionContents(Section);
    if (!ContentsOrErr) {
      consumeError(ContentsOrErr.takeError());
      return {};
    }
    return toStringRef(*ContentsOrErr);
  }

  template <class SectionT>
  void setSection(SectionT &Section, const Shdr &RawSection, size_t Index) {
    Section.Data = sectionData(RawSection);
    // DWARFSection identity is used by find() to retrieve relocations for the
    // corresponding ELF section.
    SectionsByIndex[Index] = &Section;
    SectionIndices[&Section] = Index;
  }

  /// Map a DWARF section name to the section representation required by LLVM.
  void addSection(StringRef Name, const Shdr &RawSection, size_t Index) {
    if (Name.starts_with("."))
      Name = Name.drop_front();

    // DWARFSection records section identity for relocation lookup. Sections
    // exposed as StringRef are passed to LLVM as raw byte tables.
    if (Name == "debug_info") {
      setSection(InfoSection, RawSection, Index);
      HasInfoSection = true;
    } else if (Name == "debug_abbrev") {
      AbbrevSection = sectionData(RawSection);
    } else if (Name == "debug_line") {
      setSection(LineSection, RawSection, Index);
    } else if (Name == "debug_line_str") {
      LineStrSection = sectionData(RawSection);
    } else if (Name == "debug_str") {
      StrSection = sectionData(RawSection);
    } else if (Name == "debug_str_offsets") {
      setSection(StrOffsetsSection, RawSection, Index);
    }
  }

  template <class RelTy, bool IsRela>
  std::optional<RelocAddrEntry>
  findRelocation(const RelocationSectionInfo &RelocSection,
                 ArrayRef<RelTy> Relocations, uint64_t Pos) const {
    // ELF relocation entries are ordered by offset, so find the entry at Pos
    // without scanning unrelated entries.
    auto It = partition_point(Relocations, [=](const RelTy &Relocation) {
      return Relocation.r_offset < Pos;
    });
    if (It == Relocations.end() || It->r_offset != Pos)
      return std::nullopt;

    uint32_t SymbolIndex = It->getSymbol(/*isMips64EL=*/false);
    if (SymbolIndex >= RelocSection.Symbols.size())
      return std::nullopt;
    const Sym &Symbol = RelocSection.Symbols[SymbolIndex];
    uint64_t SectionIndex =
        Symbol.st_shndx == ELF::SHN_ABS ? uint64_t(-1) : Symbol.st_shndx;

    // RELA stores its addend in the relocation; REL stores it in the section.
    int64_t Addend = 0;
    if constexpr (IsRela)
      Addend = It->r_addend;
    DataRefImpl RefData;
    RefData.p = Addend;
    RelocationRef RelocRef(RefData, nullptr);
    RelocationResolver Resolver =
        IsRela ? resolveRelaRelocation : resolveRelRelocation;
    return RelocAddrEntry{SectionIndex,
                          RelocRef,
                          Symbol.st_value,
                          std::optional<RelocationRef>(),
                          0,
                          Resolver};
  }

  void addRelocationSections() {
    RelocationsByTarget.resize(Sections.size());
    for (const Shdr &RelSection : Sections) {
      if (RelSection.sh_type != ELF::SHT_REL &&
          RelSection.sh_type != ELF::SHT_RELA)
        continue;
      if (RelSection.sh_info >= SectionsByIndex.size() ||
          !SectionsByIndex[RelSection.sh_info])
        continue;
      if (RelSection.sh_link >= Sections.size())
        continue;

      // sh_info identifies the relocated section; sh_link identifies the
      // symbol table used to resolve relocation symbol indices.
      const Shdr &SymbolTable = Sections[RelSection.sh_link];
      llvm::Expected<typename ELFT::SymRange> SymbolsOrErr =
          File.symbols(&SymbolTable);
      if (!SymbolsOrErr) {
        consumeError(SymbolsOrErr.takeError());
        continue;
      }

      RelocationsByTarget[RelSection.sh_info].push_back(
          {&RelSection, *SymbolsOrErr, RelSection.sh_type == ELF::SHT_RELA});
    }
  }

  static uint64_t resolveRelaRelocation(uint64_t, uint64_t,
                                        uint64_t SymbolValue, uint64_t,
                                        int64_t Addend) {
    // RELA carries its addend explicitly.
    return SymbolValue + Addend;
  }

  static uint64_t resolveRelRelocation(uint64_t, uint64_t, uint64_t SymbolValue,
                                       uint64_t LocationData, int64_t) {
    // REL stores the addend in the bytes at the relocation location.
    return SymbolValue + LocationData;
  }

  // File and the section data below refer to the original input buffer.
  ELFFile File;
  typename ELFT::ShdrRange Sections;
  std::string FileName;
  DWARFSection InfoSection;
  bool HasInfoSection = false; // Do not expose a missing info section.
  // Track which ELF target sections are represented as DWARFSection objects.
  std::vector<const DWARFSection *> SectionsByIndex;
  // Map a DWARFSection back to its ELF section index for find().
  DenseMap<const DWARFSection *, uint32_t> SectionIndices;
  // Relocation sections grouped by the ELF section they apply to.
  std::vector<std::vector<RelocationSectionInfo>> RelocationsByTarget;
  StringRef AbbrevSection;
  StringRef StrSection;
  StringRef LineStrSection;
  DWARFSection LineSection;
  DWARFSection StrOffsetsSection;
};

template <class ELFT>
llvm::Expected<std::unique_ptr<const DWARFObject>>
createELDDWARFObjectForELFT(const ELFObjectFile &Input) {
  // Parse the input with the ELF class selected by the caller, then expose it
  // through LLVM's architecture-independent DWARFObject interface.
  llvm::Expected<ELFFile<ELFT>> FileOrErr =
      ELFFile<ELFT>::create(Input.getContents());
  if (!FileOrErr)
    return FileOrErr.takeError();
  auto Object =
      std::make_unique<ELDDWARFObject<ELFT>>(Input, std::move(*FileOrErr));
  if (llvm::Error Err = Object->initialize())
    return std::move(Err);
  return std::unique_ptr<const DWARFObject>(std::move(Object));
}

} // namespace

llvm::Expected<std::unique_ptr<const DWARFObject>>
detail::createELDDWARFObject(const ELFObjectFile &Input) {
  // Select an ELF parser specialization from the kind established by ELD's
  // normal input reader.
  switch (Input.getELFKind()) {
  case ObjectFile::ELF32LEKind:
    return createELDDWARFObjectForELFT<ELF32LE>(Input);
  case ObjectFile::ELF32BEKind:
    return createStringError(inconvertibleErrorCode(),
                             "big-endian ELF DWARF is unsupported");
  case ObjectFile::ELF64LEKind:
    return createELDDWARFObjectForELFT<ELF64LE>(Input);
  case ObjectFile::ELF64BEKind:
    return createStringError(inconvertibleErrorCode(),
                             "big-endian ELF DWARF is unsupported");
  case ObjectFile::ELFNoneKind:
    return createStringError(inconvertibleErrorCode(),
                             "input has no ELF format information");
  }
  llvm_unreachable("unknown ELF kind");
}

} // namespace eld
