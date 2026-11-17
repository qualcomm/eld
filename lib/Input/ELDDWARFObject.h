//===- ELDDWARFObject.h----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_INPUT_ELDDWARFOBJECT_H
#define ELD_INPUT_ELDDWARFOBJECT_H

#include "eld/PluginAPI/Expected.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/DebugInfo/DWARF/DWARFObject.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace eld {

class ELFObjectFile;
class ELFSection;

namespace detail {

/// Adapts the DWARF sections in an ELD ELF input for LLVM's DWARFContext.
/// Section contents are views into the input buffer or the fragments created
/// by ELD, which must outlive the context using this object.
template <class ELFT> class ELDDWARFObject final : public llvm::DWARFObject {
public:
  explicit ELDDWARFObject(const ELFObjectFile &InputFile);

  /// Expose the supported DWARF sections from ELD's parsed input sections.
  eld::Expected<void> initialize();

  llvm::StringRef getFileName() const override { return FileName; }
  uint8_t getAddressSize() const override { return ELFT::Is64Bits ? 8 : 4; }
  bool isLittleEndian() const override { return true; }

  void forEachInfoSections(
      llvm::function_ref<void(const llvm::DWARFSection &)> F) const override {
    for (const llvm::DWARFSection &Section : InfoSections)
      F(Section);
  }

  void forEachTypesSections(
      llvm::function_ref<void(const llvm::DWARFSection &)> F) const override {
    for (const llvm::DWARFSection &Section : TypesSections)
      F(Section);
  }

  void forEachInfoDWOSections(
      llvm::function_ref<void(const llvm::DWARFSection &)> F) const override {
    for (const llvm::DWARFSection &Section : InfoDWOSections)
      F(Section);
  }

  void forEachTypesDWOSections(
      llvm::function_ref<void(const llvm::DWARFSection &)> F) const override {
    for (const llvm::DWARFSection &Section : TypesDWOSections)
      F(Section);
  }

  llvm::StringRef getAbbrevSection() const override { return AbbrevSection; }
  const llvm::DWARFSection &getLocSection() const override {
    return LocSection;
  }
  const llvm::DWARFSection &getLoclistsSection() const override {
    return LoclistsSection;
  }
  llvm::StringRef getArangesSection() const override { return ArangesSection; }
  const llvm::DWARFSection &getFrameSection() const override {
    return FrameSection;
  }
  const llvm::DWARFSection &getEHFrameSection() const override {
    return EHFrameSection;
  }
  const llvm::DWARFSection &getLineSection() const override {
    return LineSection;
  }
  llvm::StringRef getLineStrSection() const override { return LineStrSection; }
  llvm::StringRef getStrSection() const override { return StrSection; }
  const llvm::DWARFSection &getRangesSection() const override {
    return RangesSection;
  }
  const llvm::DWARFSection &getRnglistsSection() const override {
    return RnglistsSection;
  }
  const llvm::DWARFSection &getMacroSection() const override {
    return MacroSection;
  }
  llvm::StringRef getMacroDWOSection() const override {
    return MacroDWOSection;
  }
  llvm::StringRef getMacinfoSection() const override { return MacinfoSection; }
  llvm::StringRef getMacinfoDWOSection() const override {
    return MacinfoDWOSection;
  }
  const llvm::DWARFSection &getPubnamesSection() const override {
    return PubnamesSection;
  }
  const llvm::DWARFSection &getPubtypesSection() const override {
    return PubtypesSection;
  }
  const llvm::DWARFSection &getGnuPubnamesSection() const override {
    return GnuPubnamesSection;
  }
  const llvm::DWARFSection &getGnuPubtypesSection() const override {
    return GnuPubtypesSection;
  }
  const llvm::DWARFSection &getStrOffsetsSection() const override {
    return StrOffsetsSection;
  }
  llvm::StringRef getAbbrevDWOSection() const override {
    return AbbrevDWOSection;
  }
  const llvm::DWARFSection &getLineDWOSection() const override {
    return LineDWOSection;
  }
  const llvm::DWARFSection &getLocDWOSection() const override {
    return LocDWOSection;
  }
  const llvm::DWARFSection &getLoclistsDWOSection() const override {
    return LoclistsDWOSection;
  }
  llvm::StringRef getStrDWOSection() const override { return StrDWOSection; }
  const llvm::DWARFSection &getStrOffsetsDWOSection() const override {
    return StrOffsetsDWOSection;
  }
  const llvm::DWARFSection &getRangesDWOSection() const override {
    return RangesDWOSection;
  }
  const llvm::DWARFSection &getRnglistsDWOSection() const override {
    return RnglistsDWOSection;
  }
  const llvm::DWARFSection &getAddrSection() const override {
    return AddrSection;
  }
  const llvm::DWARFSection &getAppleNamesSection() const override {
    return AppleNamesSection;
  }
  const llvm::DWARFSection &getAppleTypesSection() const override {
    return AppleTypesSection;
  }
  const llvm::DWARFSection &getAppleNamespacesSection() const override {
    return AppleNamespacesSection;
  }
  const llvm::DWARFSection &getNamesSection() const override {
    return NamesSection;
  }
  const llvm::DWARFSection &getAppleObjCSection() const override {
    return AppleObjCSection;
  }
  llvm::StringRef getCUIndexSection() const override { return CUIndexSection; }
  llvm::StringRef getGdbIndexSection() const override {
    return GdbIndexSection;
  }
  llvm::StringRef getTUIndexSection() const override { return TUIndexSection; }

  /// Relocation lookup is not provided by this adapter.
  std::optional<llvm::RelocAddrEntry> find(const llvm::DWARFSection &,
                                           uint64_t) const override {
    return std::nullopt;
  }

private:
  eld::Expected<llvm::StringRef> sectionData(ELFSection &Section);
  eld::Expected<void> setSection(llvm::DWARFSection &Section,
                                 ELFSection &InputSection);
  llvm::SmallVector<llvm::DWARFSection, 1> *
  mapUnitSection(llvm::StringRef Name);
  llvm::DWARFSection *mapNameToDWARFSection(llvm::StringRef Name);
  llvm::StringRef *mapNameToStringSection(llvm::StringRef Name);
  eld::Expected<void> addSection(llvm::StringRef Name,
                                 ELFSection &InputSection);

  // DWARFSection data refers to the input buffer or decompressed storage owned
  // by the corresponding ELFSection.
  const ELFObjectFile &InputFile;
  std::string FileName;
  llvm::SmallVector<llvm::DWARFSection, 1> InfoSections;
  llvm::SmallVector<llvm::DWARFSection, 1> TypesSections;
  llvm::SmallVector<llvm::DWARFSection, 1> InfoDWOSections;
  llvm::SmallVector<llvm::DWARFSection, 1> TypesDWOSections;
  llvm::StringRef AbbrevSection;
  llvm::StringRef AbbrevDWOSection;
  llvm::StringRef ArangesSection;
  llvm::StringRef StrSection;
  llvm::StringRef StrDWOSection;
  llvm::StringRef LineStrSection;
  llvm::StringRef MacroDWOSection;
  llvm::StringRef MacinfoSection;
  llvm::StringRef MacinfoDWOSection;
  llvm::StringRef CUIndexSection;
  llvm::StringRef GdbIndexSection;
  llvm::StringRef TUIndexSection;
  llvm::DWARFSection LocSection;
  llvm::DWARFSection LoclistsSection;
  llvm::DWARFSection LoclistsDWOSection;
  llvm::DWARFSection LineDWOSection;
  llvm::DWARFSection FrameSection;
  llvm::DWARFSection EHFrameSection;
  llvm::DWARFSection LocDWOSection;
  llvm::DWARFSection StrOffsetsDWOSection;
  llvm::DWARFSection RangesDWOSection;
  llvm::DWARFSection RnglistsDWOSection;
  llvm::DWARFSection AddrSection;
  llvm::DWARFSection AppleNamesSection;
  llvm::DWARFSection AppleTypesSection;
  llvm::DWARFSection AppleNamespacesSection;
  llvm::DWARFSection AppleObjCSection;
  llvm::DWARFSection NamesSection;
  llvm::DWARFSection LineSection;
  llvm::DWARFSection RangesSection;
  llvm::DWARFSection RnglistsSection;
  llvm::DWARFSection MacroSection;
  llvm::DWARFSection PubnamesSection;
  llvm::DWARFSection PubtypesSection;
  llvm::DWARFSection GnuPubnamesSection;
  llvm::DWARFSection GnuPubtypesSection;
  llvm::DWARFSection StrOffsetsSection;
};

eld::Expected<std::unique_ptr<const llvm::DWARFObject>>
createELDDWARFObject(const ELFObjectFile &Input);

} // namespace detail
} // namespace eld

#endif // ELD_INPUT_ELDDWARFOBJECT_H
