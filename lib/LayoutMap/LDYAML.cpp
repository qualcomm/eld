//===- LDYAML.cpp----------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/LayoutMap/LDYAML.h"

#include "eld/Config/LinkerConfig.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Script/InputSectDesc.h"
#include "llvm/BinaryFormat/ELF.h"

using namespace llvm::yaml;
using namespace eld::LDYAML;

void MappingTraits<Header>::mapping(IO &IO, Header &Header) {
  IO.mapRequired("Architecture", Header.Architecture);
  IO.mapOptional("Emulation", Header.Emulation);
  IO.mapRequired("AddressSize", Header.AddressSize);
  IO.mapRequired("ABIPageSize", Header.ABIPageSize);
}

void MappingTraits<Version>::mapping(IO &IO, Version &Version) {
  IO.mapRequired("CompilerVersion", Version.VendorVersion);
  IO.mapRequired("LinkerVersion", Version.ELDVersion);
}

void MappingTraits<ArchiveRecord>::mapping(IO &IO, ArchiveRecord &Record) {
  IO.mapRequired("Origin", Record.Origin);
  IO.mapRequired("Referred", Record.Referred);
}

void MappingTraits<Common>::mapping(IO &IO, Common &Common) {
  IO.mapRequired("Name", Common.Name);
  IO.mapRequired("Size", Common.Size);
  IO.mapOptional("InputPath", Common.InputPath);
  IO.mapOptional("InputName", Common.InputName);
}

void MappingTraits<LinkStats>::mapping(IO &IO, LinkStats &S) {
  IO.mapRequired("Name", S.Name);
  IO.mapRequired("Count", S.Count);
}

void ScalarBitSetTraits<eld::LDYAML::SymbolType>::bitset(
    IO &IO, eld::LDYAML::SymbolType &SymbolType) {
  IO.bitSetCase(SymbolType, "STT_OBJECT", llvm::ELF::STT_OBJECT);
  IO.bitSetCase(SymbolType, "STT_FUNC", llvm::ELF::STT_FUNC);
  IO.bitSetCase(SymbolType, "STT_SECT", llvm::ELF::STT_SECTION);
  IO.bitSetCase(SymbolType, "STT_FILE", llvm::ELF::STT_FILE);
  IO.bitSetCase(SymbolType, "STT_COMMON", llvm::ELF::STT_COMMON);
  IO.bitSetCase(SymbolType, "STT_TLS", llvm::ELF::STT_TLS);
  IO.bitSetCase(SymbolType, "STT_GNU_IFUNC", llvm::ELF::STT_GNU_IFUNC);
}

void ScalarEnumerationTraits<eld::LDYAML::SymbolScope>::enumeration(
    IO &IO, eld::LDYAML::SymbolScope &SymbolScope) {
  IO.enumCase(SymbolScope, "Global", eld::ResolveInfo::Global);
  IO.enumCase(SymbolScope, "Weak", eld::ResolveInfo::Weak);
  IO.enumCase(SymbolScope, "Local", eld::ResolveInfo::Local);
  IO.enumCase(SymbolScope, "Absolute", eld::ResolveInfo::Absolute);
  IO.enumCase(SymbolScope, "None", eld::ResolveInfo::NoneBinding);
}

void MappingTraits<eld::LDYAML::Symbol>::mapping(IO &IO,
                                                 eld::LDYAML::Symbol &Symbol) {
  IO.mapRequired("Symbol", Symbol.Name);
  IO.mapRequired("Type", Symbol.Type);
  IO.mapRequired("Scope", Symbol.Scope);
  IO.mapRequired("Size", Symbol.Size);
  IO.mapOptional("Value", Symbol.Value);
}

void MappingTraits<eld::LDYAML::SimpleSymbol>::mapping(
    IO &IO, eld::LDYAML::SimpleSymbol &Symbol) {
  IO.mapRequired("Symbol", Symbol.Name);
  IO.mapRequired("Type", Symbol.Type);
  IO.mapRequired("Size", Symbol.Size);
}

void eld::LDYAML::InputSection::mapping(IO &IO) {
  MappingTraits<eld::LDYAML::InputSection>::mapping(IO, *this);
}

void MappingTraits<eld::LDYAML::InputSection>::mapping(
    IO &IO, eld::LDYAML::InputSection &Input) {
  IO.mapRequired("Name", Input.Name);
  IO.mapRequired("Type", Input.Type);
  IO.mapRequired("Permissions", Input.InputPermissions);
  IO.mapRequired("LinkerScript", Input.LinkerScript);
  IO.mapRequired("Offset", Input.Offset);
  IO.mapRequired("Size", Input.Size);
  IO.mapRequired("Origin", Input.Origin);
  IO.mapRequired("Alignment", Input.Alignment);
  IO.mapOptional("Symbols", Input.Symbols);
}

void eld::LDYAML::InputBitcodeSection::mapping(IO &IO) {
  MappingTraits<eld::LDYAML::InputBitcodeSection>::mapping(IO, *this);
}

void MappingTraits<eld::LDYAML::InputBitcodeSection>::mapping(
    IO &IO, eld::LDYAML::InputBitcodeSection &Input) {
  IO.mapRequired("Name", Input.Name);
  IO.mapRequired("Type", Input.Type);
  IO.mapRequired("Permissions", Input.InputPermissions);
  IO.mapRequired("LinkerScript", Input.LinkerScript);
  IO.mapRequired("Offset", Input.Offset);
  IO.mapRequired("Size", Input.Size);
  IO.mapOptional("IRFile", Input.BitcodeOrigin);
  IO.mapRequired("PostLTOFile", Input.Origin);
  IO.mapRequired("Alignment", Input.Alignment);
  IO.mapOptional("Symbols", Input.Symbols);
}

void MappingTraits<std::shared_ptr<eld::LDYAML::DiscardedSection>>::mapping(
    IO &IO, std::shared_ptr<eld::LDYAML::DiscardedSection> &DiscardedSection) {
  if (DiscardedSection) {
    DiscardedSection->mapping(IO);
    return;
  }
  std::vector<llvm::StringRef> Keys = IO.keys();
  DiscardedSection = std::make_shared<eld::LDYAML::DiscardedSection>();
  DiscardedSection->mapping(IO);
}

void eld::LDYAML::DiscardedSection::mapping(IO &IO) {
  MappingTraits<eld::LDYAML::DiscardedSection>::mapping(IO, *this);
}

void MappingTraits<eld::LDYAML::DiscardedSection>::mapping(
    IO &IO, eld::LDYAML::DiscardedSection &DS) {
  IO.mapRequired("Name", DS.Name);
  IO.mapRequired("Type", DS.Type);
  IO.mapRequired("Permissions", DS.InputPermissions);
  IO.mapRequired("Size", DS.Size);
  IO.mapRequired("Origin", DS.Origin);
  IO.mapRequired("Alignment", DS.Alignment);
  IO.mapOptional("Symbols", DS.Symbols);
}

void ScalarBitSetTraits<eld::LDYAML::SectionType>::bitset(
    IO &IO, eld::LDYAML::SectionType &SectionType) {
  IO.bitSetCase(SectionType, "SHT_GROUP", llvm::ELF::SHT_GROUP);
  IO.bitSetCase(SectionType, "SHT_PROGBITS", llvm::ELF::SHT_PROGBITS);
  IO.bitSetCase(SectionType, "SHT_SYMTAB", llvm::ELF::SHT_SYMTAB);
  IO.bitSetCase(SectionType, "SHT_STRTAB", llvm::ELF::SHT_STRTAB);
  IO.bitSetCase(SectionType, "SHT_RELA", llvm::ELF::SHT_RELA);
  IO.bitSetCase(SectionType, "SHT_HASH", llvm::ELF::SHT_HASH);
  IO.bitSetCase(SectionType, "SHT_DYNAMIC", llvm::ELF::SHT_DYNAMIC);
  IO.bitSetCase(SectionType, "SHT_NOTE", llvm::ELF::SHT_NOTE);
  IO.bitSetCase(SectionType, "SHT_NOBITS", llvm::ELF::SHT_NOBITS);
  IO.bitSetCase(SectionType, "SHT_REL", llvm::ELF::SHT_REL);
  IO.bitSetCase(SectionType, "SHT_SHLIB", llvm::ELF::SHT_SHLIB);
  IO.bitSetCase(SectionType, "SHT_DYNSYM", llvm::ELF::SHT_DYNSYM);
  IO.bitSetCase(SectionType, "SHT_INIT_ARRAY", llvm::ELF::SHT_INIT_ARRAY);
  IO.bitSetCase(SectionType, "SHT_FINI_ARRAY", llvm::ELF::SHT_FINI_ARRAY);
  IO.bitSetCase(SectionType, "SHT_PREINIT_ARRAY", llvm::ELF::SHT_PREINIT_ARRAY);
  IO.bitSetCase(SectionType, "SHT_GROUP", llvm::ELF::SHT_GROUP);
  IO.bitSetCase(SectionType, "SHT_SYMTAB_SHNDX", llvm::ELF::SHT_SYMTAB_SHNDX);
}

void ScalarBitSetTraits<eld::LDYAML::Permissions>::bitset(
    IO &IO, eld::LDYAML::Permissions &Permissions) {
  IO.bitSetCase(Permissions, "SHF_ALLOC", llvm::ELF::SHF_ALLOC);
  IO.bitSetCase(Permissions, "SHF_WRITE", llvm::ELF::SHF_WRITE);
  IO.bitSetCase(Permissions, "SHF_EXECINSTR", llvm::ELF::SHF_EXECINSTR);
  IO.bitSetCase(Permissions, "SHF_MERGE", llvm::ELF::SHF_MERGE);
  IO.bitSetCase(Permissions, "SHF_STRINGS", llvm::ELF::SHF_STRINGS);
  IO.bitSetCase(Permissions, "SHF_GROUP", llvm::ELF::SHF_GROUP);
  IO.bitSetCase(Permissions, "SHF_HEX_GPREL", llvm::ELF::SHF_HEX_GPREL);
}

void MappingTraits<Assignment>::mapping(IO &IO, Assignment &Assignment) {
  IO.mapRequired("Name", Assignment.Name);
  IO.mapRequired("Value", Assignment.Value);
  IO.mapRequired("Text", Assignment.Text);
}

void eld::LDYAML::Assignment::mapping(IO &IO) {
  MappingTraits<eld::LDYAML::Assignment>::mapping(IO, *this);
}

void MappingTraits<LinkerScriptRule>::mapping(
    IO &IO, LinkerScriptRule &LinkerScriptRule) {
  IO.mapRequired("LinkerScript", LinkerScriptRule.LinkerScript);
}

void eld::LDYAML::LinkerScriptRule::mapping(IO &IO) {
  MappingTraits<eld::LDYAML::LinkerScriptRule>::mapping(IO, *this);
}

void MappingTraits<eld::LDYAML::Padding>::mapping(
    IO &IO, eld::LDYAML::Padding &Padding) {
  IO.mapRequired("Name", Padding.Name);
  IO.mapRequired("Offset", Padding.Offset);
  IO.mapRequired("Padding", Padding.PaddingValue);
}

void eld::LDYAML::Padding::mapping(IO &IO) {
  MappingTraits<eld::LDYAML::Padding>::mapping(IO, *this);
}

void MappingTraits<OutputSection>::mapping(IO &IO, OutputSection &Output) {
  IO.mapRequired("Name", Output.Name);
  IO.mapRequired("Type", Output.Type);
  IO.mapRequired("Permissions", Output.OutputPermissions);
  IO.mapRequired("Address", Output.Address);
  IO.mapRequired("Offset", Output.Offset);
  IO.mapRequired("Size", Output.Size);
  IO.mapRequired("Contents", Output.Inputs);
}

void eld::LDYAML::OutputSection::mapping(IO &IO) {
  MappingTraits<eld::LDYAML::OutputSection>::mapping(IO, *this);
}

void ScalarEnumerationTraits<eld::LDYAML::CodeGenType>::enumeration(
    IO &IO, eld::LDYAML::CodeGenType &CodeGenType) {
  IO.enumCase(CodeGenType, "Executable", eld::LinkerConfig::CodeGenType::Exec);
  IO.enumCase(CodeGenType, "Dynamic Library",
              eld::LinkerConfig::CodeGenType::DynObj);
  IO.enumCase(CodeGenType, "ObjectFile",
              eld::LinkerConfig::CodeGenType::Object);
}

void MappingTraits<CommandLineDefault>::mapping(
    IO &IO, CommandLineDefault &CommandLineDefault) {
  IO.mapRequired("Name", CommandLineDefault.Name);
  IO.mapRequired("Value", CommandLineDefault.Value);
  IO.mapRequired("Description", CommandLineDefault.Description);
}

void MappingTraits<Module>::mapping(IO &IO, Module &Module) {
  IO.mapRequired("Header", Module.ModuleHeader);
  IO.mapRequired("VersionInformation", Module.ModuleVersion);
  IO.mapOptional("ArchiveRecords", Module.ArchiveRecords);
  IO.mapOptional("Commons", Module.Commons);
  IO.mapOptional("LinkStats", Module.Stats);
  IO.mapOptional("Features", Module.Features);
  IO.mapOptional("Inputs", Module.InputActions);
  IO.mapOptional("InputInfo", Module.InputFileInfo);
  IO.mapOptional("LinkerScriptsUsed", Module.LinkerScriptsUsed);
  IO.mapRequired("BuildType", Module.BuildType);
  IO.mapRequired("OutputFile", Module.OutputFile);
  IO.mapRequired("EntryAddress", Module.EntryAddress);
  IO.mapRequired("CommandLine", Module.CommandLine);
  IO.mapRequired("CommandLineDefaults", Module.CommandLineDefaults);
  IO.mapRequired("OutputSections", Module.OutputSections);
  IO.mapOptional("Trampolines", Module.TrampolinesGenerated);
  IO.mapOptional("DiscardedComdats", Module.DiscardedSectionGroups);
  IO.mapOptional("DiscardedSections", Module.DiscardedSections);
  IO.mapOptional("DiscardedCommonSymbols", Module.DiscardedCommons);
  IO.mapOptional("LoadRegions", Module.LoadRegions);
  IO.mapOptional("CrossReference", Module.CrossReferences);
}

void MappingTraits<std::shared_ptr<eld::LDYAML::Content>>::mapping(
    IO &IO, std::shared_ptr<eld::LDYAML::Content> &Content) {
  if (Content) {
    Content->mapping(IO);
    return;
  }
  std::vector<llvm::StringRef> Keys = IO.keys();
  if (llvm::find(Keys, "Contents") != Keys.end()) {
    Content = std::make_shared<eld::LDYAML::OutputSection>();
    Content->ContentKind = Content::Kind::OutputSection;
  } else if (llvm::find(Keys, "PostLTOFile") != Keys.end()) {
    Content = std::make_shared<eld::LDYAML::InputBitcodeSection>();
    Content->ContentKind = Content::Kind::InputSection;
  } else if (llvm::find(Keys, "Origin") != Keys.end()) {
    Content = std::make_shared<eld::LDYAML::InputSection>();
    Content->ContentKind = Content::Kind::InputSection;
  } else if (llvm::find(Keys, "Padding") != Keys.end()) {
    Content = std::make_shared<eld::LDYAML::Padding>();
    Content->ContentKind = Content::Kind::Padding;
  } else if (llvm::find(Keys, "LinkerScript") != Keys.end()) {
    Content = std::make_shared<eld::LDYAML::LinkerScriptRule>();
    Content->ContentKind = Content::Kind::LinkerScriptRule;
  } else {
    Content = std::make_shared<eld::LDYAML::Assignment>();
    Content->ContentKind = Content::Kind::Assignment;
  }
  Content->mapping(IO);
}

void eld::LDYAML::LoadRegion::mapping(IO &IO) {
  MappingTraits<eld::LDYAML::LoadRegion>::mapping(IO, *this);
}

void MappingTraits<eld::LDYAML::LoadRegion>::mapping(
    IO &IO, eld::LDYAML::LoadRegion &L) {
  IO.mapOptional("Name", L.SegmentName);
  IO.mapRequired("Type", L.Type);
  IO.mapRequired("Permissions", L.SegPermission);
  IO.mapRequired("FileOffset", L.FileOffset);
  IO.mapRequired("VirtualAddress", L.VirtualAddress);
  IO.mapRequired("PhysicalAddress", L.PhysicalAddress);
  IO.mapRequired("FileSize", L.FileSize);
  IO.mapRequired("MemorySize", L.MemorySize);
  IO.mapRequired("Alignment", L.Alignment);
  IO.mapOptional("Sections", L.Sections);
}

void eld::LDYAML::CRef::mapping(IO &IO) {
  MappingTraits<eld::LDYAML::CRef>::mapping(IO, *this);
}

void MappingTraits<eld::LDYAML::CRef>::mapping(IO &IO, eld::LDYAML::CRef &C) {
  IO.mapRequired("Symbol", C.SymbolName);
  IO.mapRequired("ReferencedBy", C.FileRefs);
}

void ScalarBitSetTraits<eld::LDYAML::SegmentType>::bitset(
    IO &IO, eld::LDYAML::SegmentType &SegmentType) {
  IO.bitSetCase(SegmentType, "PT_PHDR", llvm::ELF::PT_PHDR);
  IO.bitSetCase(SegmentType, "PT_INTERP", llvm::ELF::PT_INTERP);
  IO.bitSetCase(SegmentType, "PT_LOAD", llvm::ELF::PT_LOAD);
  IO.bitSetCase(SegmentType, "PT_NOTE", llvm::ELF::PT_NOTE);
  IO.bitSetCase(SegmentType, "PT_DYNAMIC", llvm::ELF::PT_DYNAMIC);
  IO.bitSetCase(SegmentType, "PT_TLS", llvm::ELF::PT_TLS);
}

void ScalarBitSetTraits<eld::LDYAML::SegmentPermissions>::bitset(
    IO &IO, eld::LDYAML::SegmentPermissions &SegmentPermissions) {
  IO.bitSetCase(SegmentPermissions, "READ", llvm::ELF::PF_R);
  IO.bitSetCase(SegmentPermissions, "WRITE", llvm::ELF::PF_W);
  IO.bitSetCase(SegmentPermissions, "EXECUTE", llvm::ELF::PF_X);
  IO.bitSetCase(SegmentPermissions, "OS_SPECIFIC", llvm::ELF::PF_MASKOS);
  IO.bitSetCase(SegmentPermissions, "PROCESSOR_SPECIFIC",
                llvm::ELF::PF_MASKPROC);
}

void ScalarEnumerationTraits<eld::LDYAML::InputUsed>::enumeration(
    IO &IO, eld::LDYAML::InputUsed &InputUsed) {
  IO.enumCase(InputUsed, "NotUsed", false);
  IO.enumCase(InputUsed, "Used", true);
}

void MappingTraits<std::shared_ptr<eld::LDYAML::InputFile>>::mapping(
    IO &IO, std::shared_ptr<eld::LDYAML::InputFile> &InputFile) {
  if (InputFile) {
    InputFile->mapping(IO);
    return;
  }
  std::vector<llvm::StringRef> Keys = IO.keys();
  if (llvm::find(Keys, "Members") == Keys.end()) {
    InputFile = std::make_shared<eld::LDYAML::RegularInput>();
  } else {
    InputFile = std::make_shared<eld::LDYAML::Archive>();
  }
  InputFile->mapping(IO);
}

void eld::LDYAML::RegularInput::mapping(IO &IO) {
  MappingTraits<eld::LDYAML::RegularInput>::mapping(IO, *this);
}

void MappingTraits<eld::LDYAML::RegularInput>::mapping(
    IO &IO, eld::LDYAML::RegularInput &R) {
  IO.mapRequired("Path", R.Name);
  IO.mapRequired("Used", R.Used);
}

void eld::LDYAML::Archive::mapping(IO &IO) {
  MappingTraits<eld::LDYAML::Archive>::mapping(IO, *this);
}

void MappingTraits<eld::LDYAML::Archive>::mapping(IO &IO,
                                                  eld::LDYAML::Archive &A) {
  IO.mapRequired("Path", A.Name);
  IO.mapRequired("Used", A.Used);
  IO.mapOptional("Members", A.ArchiveMembers);
}

void eld::LDYAML::TrampolineInfo::mapping(IO &IO) {
  MappingTraits<eld::LDYAML::TrampolineInfo>::mapping(IO, *this);
}

void MappingTraits<eld::LDYAML::TrampolineInfo>::mapping(
    IO &IO, eld::LDYAML::TrampolineInfo &TInfo) {
  IO.mapRequired("OutputSection", TInfo.OutputSectionName);
  IO.mapRequired("Trampolines", TInfo.Trampolines);
}

void MappingTraits<eld::LDYAML::Trampoline>::mapping(
    IO &IO, eld::LDYAML::Trampoline &T) {
  IO.mapRequired("Name", T.Name);
  IO.mapRequired("From", T.From);
  IO.mapOptional("FromSymbols", T.FromSymbols);
  IO.mapRequired("Origin", T.FromFile);
  IO.mapRequired("To", T.To);
  IO.mapRequired("ToSection", T.ToSection);
  IO.mapOptional("ToSymbols", T.ToSymbols);
  IO.mapRequired("Destination", T.ToFile);
  IO.mapOptional("Addend", T.Addend);
  IO.mapOptional("Uses", T.Uses);
}

void MappingTraits<eld::LDYAML::Reuse>::mapping(IO &IO, eld::LDYAML::Reuse &R) {
  IO.mapRequired("From", R.From);
  IO.mapOptional("Symbols", R.Symbols);
  IO.mapRequired("Origin", R.FromFile);
  IO.mapOptional("Addend", R.Addend);
}
