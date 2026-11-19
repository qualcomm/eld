//===- ELDDWARFObject.cpp--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "ELDDWARFObject.h"
#include "eld/Diagnostics/Diagnostic.h"
#include "eld/Fragment/RegionFragment.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/Input/Input.h"
#include "eld/Readers/ELFSection.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Object/ELF.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using namespace llvm;
using namespace llvm::object;

namespace eld {
namespace {

std::unique_ptr<plugin::DiagnosticEntry>
makeDWARFUnavailableDiagnostic(StringRef FileName, StringRef Reason) {
  return std::make_unique<plugin::DiagnosticEntry>(
      Diag::verbose_dwarf_object_unavailable,
      std::vector<std::string>{FileName.str(), Reason.str()});
}

} // namespace

namespace detail {

template <class ELFT>
ELDDWARFObject<ELFT>::ELDDWARFObject(const ELFObjectFile &InputFile)
    : InputFile(InputFile), FileName(InputFile.getInput()->getName()) {}

template <class ELFT> eld::Expected<void> ELDDWARFObject<ELFT>::initialize() {
  for (Section *S : InputFile.getSections()) {
    auto *ELFS = llvm::dyn_cast_or_null<ELFSection>(S);
    if (!ELFS)
      continue;
    eld::Expected<void> AddSectionResult = addSection(ELFS->name(), *ELFS);
    if (!AddSectionResult)
      return std::move(AddSectionResult.error());
  }

  return {};
}

/// Return the section's current data, falling back to its original input bytes
/// when no region fragment has been created.
template <class ELFT>
eld::Expected<StringRef>
ELDDWARFObject<ELFT>::sectionData(ELFSection &Section) {
  if (Section.hasFragments()) {
    for (Fragment *F : Section.getFragments())
      if (auto *Region = llvm::dyn_cast<RegionFragment>(F))
        return Region->getRegion();
  }
  return Section.getOriginalContents();
}

template <class ELFT>
eld::Expected<void> ELDDWARFObject<ELFT>::setSection(DWARFSection &Section,
                                                     ELFSection &InputSection) {
  eld::Expected<StringRef> DataOrErr = sectionData(InputSection);
  if (!DataOrErr)
    return std::move(DataOrErr.error());
  Section.Data = *DataOrErr;
  return {};
}

template <class ELFT>
SmallVector<DWARFSection, 1> *
ELDDWARFObject<ELFT>::mapUnitSection(StringRef Name) {
  return StringSwitch<SmallVector<DWARFSection, 1> *>(Name)
      .Case("debug_info", &InfoSections)
      .Case("debug_types", &TypesSections)
      .Case("debug_info.dwo", &InfoDWOSections)
      .Case("debug_types.dwo", &TypesDWOSections)
      .Default(nullptr);
}

template <class ELFT>
DWARFSection *ELDDWARFObject<ELFT>::mapNameToDWARFSection(StringRef Name) {
  return StringSwitch<DWARFSection *>(Name)
      .Case("debug_loc", &LocSection)
      .Case("debug_loclists", &LoclistsSection)
      .Case("debug_loclists.dwo", &LoclistsDWOSection)
      .Case("debug_line", &LineSection)
      .Case("debug_frame", &FrameSection)
      .Case("eh_frame", &EHFrameSection)
      .Case("debug_str_offsets", &StrOffsetsSection)
      .Case("debug_ranges", &RangesSection)
      .Case("debug_ranges.dwo", &RangesDWOSection)
      .Case("debug_rnglists", &RnglistsSection)
      .Case("debug_rnglists.dwo", &RnglistsDWOSection)
      .Case("debug_loc.dwo", &LocDWOSection)
      .Case("debug_line.dwo", &LineDWOSection)
      .Case("debug_names", &NamesSection)
      .Case("debug_str_offsets.dwo", &StrOffsetsDWOSection)
      .Case("debug_addr", &AddrSection)
      .Case("apple_names", &AppleNamesSection)
      .Case("debug_pubnames", &PubnamesSection)
      .Case("debug_pubtypes", &PubtypesSection)
      .Case("debug_gnu_pubnames", &GnuPubnamesSection)
      .Case("debug_gnu_pubtypes", &GnuPubtypesSection)
      .Case("apple_types", &AppleTypesSection)
      .Case("apple_namespaces", &AppleNamespacesSection)
      .Case("apple_namespac", &AppleNamespacesSection)
      .Case("apple_objc", &AppleObjCSection)
      .Case("debug_macro", &MacroSection)
      .Default(nullptr);
}

template <class ELFT>
StringRef *ELDDWARFObject<ELFT>::mapNameToStringSection(StringRef Name) {
  return StringSwitch<StringRef *>(Name)
      .Case("debug_abbrev", &AbbrevSection)
      .Case("debug_aranges", &ArangesSection)
      .Case("debug_str", &StrSection)
      .Case("debug_macinfo", &MacinfoSection)
      .Case("debug_macinfo.dwo", &MacinfoDWOSection)
      .Case("debug_macro.dwo", &MacroDWOSection)
      .Case("debug_abbrev.dwo", &AbbrevDWOSection)
      .Case("debug_str.dwo", &StrDWOSection)
      .Case("debug_cu_index", &CUIndexSection)
      .Case("debug_tu_index", &TUIndexSection)
      .Case("gdb_index", &GdbIndexSection)
      .Case("debug_line_str", &LineStrSection)
      .Default(nullptr);
}

/// Map a DWARF section name to the section representation required by LLVM.
template <class ELFT>
eld::Expected<void> ELDDWARFObject<ELFT>::addSection(StringRef Name,
                                                     ELFSection &InputSection) {
  if (Name.starts_with("."))
    Name = Name.drop_front();

  if (SmallVector<DWARFSection, 1> *Sections = mapUnitSection(Name)) {
    eld::Expected<StringRef> DataOrErr = sectionData(InputSection);
    if (!DataOrErr)
      return std::move(DataOrErr.error());
    Sections->push_back({*DataOrErr, 0});
    return {};
  }

  if (DWARFSection *Section = mapNameToDWARFSection(Name)) {
    eld::Expected<void> Result = setSection(*Section, InputSection);
    if (!Result)
      return std::move(Result.error());
    if (Name == "debug_frame" || Name == "eh_frame")
      Section->Address = InputSection.addr();
    // LLVM also exposes .debug_ranges through the DWO getter.
    if (Name == "debug_ranges")
      RangesDWOSection.Data = Section->Data;
    return {};
  }

  if (StringRef *Section = mapNameToStringSection(Name)) {
    eld::Expected<StringRef> DataOrErr = sectionData(InputSection);
    if (!DataOrErr)
      return std::move(DataOrErr.error());
    *Section = *DataOrErr;
  }
  return {};
}

} // namespace detail

namespace {

template <class ELFT>
eld::Expected<std::unique_ptr<const DWARFObject>>
createELDDWARFObjectForELFT(const ELFObjectFile &Input) {
  auto Object = std::make_unique<detail::ELDDWARFObject<ELFT>>(Input);
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
