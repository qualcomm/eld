//===- LDYAML.h------------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_LAYOUTMAP_LDYAML_H
#define ELD_LAYOUTMAP_LDYAML_H

#include "eld/Script/Assignment.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ObjectYAML/YAML.h"

namespace eld {
namespace LDYAML {
struct Header {
  std::string Architecture;
  std::string Emulation;
  std::string AddressSize;
  llvm::yaml::Hex64 ABIPageSize;
};
struct Version {
  std::string VendorVersion;
  std::string ELDVersion;
};
struct ArchiveRecord {
  std::string Origin;
  std::string Referred;
};
struct Common {
  std::string Name;
  llvm::yaml::Hex32 Size;
  std::string InputPath;
  std::string InputName;
};

struct LinkStats {
  std::string Name;
  uint64_t Count;
};

LLVM_YAML_STRONG_TYPEDEF(uint32_t, SymbolType)
LLVM_YAML_STRONG_TYPEDEF(uint32_t, SymbolScope)
struct Symbol {
  std::string Name;
  SymbolType Type;
  SymbolScope Scope;
  llvm::yaml::Hex32 Size;
  llvm::yaml::Hex64 Value;
};
// Display Input file if it is used or not.
struct InputFile {
  virtual ~InputFile() {};
  virtual void mapping(llvm::yaml::IO &IO) = 0;
};
LLVM_YAML_STRONG_TYPEDEF(bool, InputUsed)
struct RegularInput : public InputFile {
  virtual void mapping(llvm::yaml::IO &IO) override;
  std::string Name;
  InputUsed Used;
};
struct Archive : public InputFile {
  virtual void mapping(llvm::yaml::IO &IO) override;
  std::string Name;
  InputUsed Used;
  std::vector<std::shared_ptr<InputFile>> ArchiveMembers;
};
LLVM_YAML_STRONG_TYPEDEF(uint32_t, Permissions)
LLVM_YAML_STRONG_TYPEDEF(uint32_t, SectionType)
struct Content {
  enum Kind {
    Assignment,
    LinkerScriptRule,
    Padding,
    InputSection,
    OutputSection
  };
  virtual ~Content() {};
  virtual void mapping(llvm::yaml::IO &IO) = 0;
  Kind ContentKind;
};
struct Assignment : public Content {
  virtual void mapping(llvm::yaml::IO &IO) override;
  static bool classof(const Content *C) {
    return C->ContentKind == Kind::Assignment;
  }
  std::string Name;
  llvm::yaml::Hex64 Value;
  std::string Text;
};

struct LinkerScriptRule : public Content {
  virtual void mapping(llvm::yaml::IO &IO) override;
  static bool classof(const Content *C) {
    return C->ContentKind == Kind::LinkerScriptRule;
  }
  std::string LinkerScript;
};

struct Padding : public Content {
  virtual void mapping(llvm::yaml::IO &IO) override;
  static bool classof(const Content *C) {
    return C->ContentKind == Kind::Padding;
  }
  llvm::StringRef Name;
  llvm::yaml::Hex64 Offset;
  llvm::yaml::Hex64 PaddingValue;
};
struct InputSection : public Content {
  virtual void mapping(llvm::yaml::IO &IO) override;
  static bool classof(const Content *C) {
    return C->ContentKind == Kind::InputSection;
  }
  std::string Name;
  SectionType Type;
  Permissions InputPermissions;
  std::string LinkerScript;
  llvm::yaml::Hex64 Offset;
  llvm::yaml::Hex64 Size;
  std::string Origin;
  uint32_t Alignment;
  std::vector<Symbol> Symbols;
};

struct InputBitcodeSection : public InputSection {
  virtual void mapping(llvm::yaml::IO &IO) override;
  std::string BitcodeOrigin;
};
struct OutputSection : public Content {
  virtual void mapping(llvm::yaml::IO &IO) override;
  static bool classof(const Content *C) {
    return C->ContentKind == Kind::OutputSection;
  }
  std::string Name;
  SectionType Type;
  llvm::yaml::Hex64 Address;
  llvm::yaml::Hex64 Offset;
  llvm::yaml::Hex64 Size;
  Permissions OutputPermissions;
  std::vector<std::shared_ptr<Content>> Inputs;
};

struct SimpleSymbol {
  std::string Name;
  SymbolType Type;
  llvm::yaml::Hex64 Size;
};

struct Reuse {
  std::string From;
  std::vector<SimpleSymbol> Symbols;
  std::string FromFile;
  llvm::yaml::Hex64 Addend;
};

struct Trampoline {
  std::string Name;
  std::string From;
  std::vector<SimpleSymbol> FromSymbols;
  std::string FromFile;
  std::string To;
  std::string ToSection;
  std::vector<SimpleSymbol> ToSymbols;
  std::string ToFile;
  llvm::yaml::Hex64 Addend;
  std::vector<Reuse> Uses;
};

struct TrampolineInfo {
  virtual void mapping(llvm::yaml::IO &IO);
  std::string OutputSectionName;
  std::vector<Trampoline> Trampolines;
  virtual ~TrampolineInfo() {}
};

struct CommandLineDefault {
  std::string Name;
  std::string Value;
  std::string Description;
};
LLVM_YAML_STRONG_TYPEDEF(uint8_t, CodeGenType)
struct DiscardedSection {
  virtual void mapping(llvm::yaml::IO &IO);
  std::string Name;
  SectionType Type;
  Permissions InputPermissions;
  std::string LinkerScript;
  llvm::yaml::Hex64 Size;
  std::string Origin;
  uint32_t Alignment;
  std::vector<Symbol> Symbols;
  virtual ~DiscardedSection() {}
};

LLVM_YAML_STRONG_TYPEDEF(uint32_t, SegmentType)
LLVM_YAML_STRONG_TYPEDEF(uint32_t, SegmentPermissions)
struct LoadRegion {
  virtual void mapping(llvm::yaml::IO &IO);
  SegmentType Type;
  llvm::yaml::Hex64 VirtualAddress;
  llvm::yaml::Hex64 PhysicalAddress;
  std::string SegmentName;
  SegmentPermissions SegPermission;
  llvm::yaml::Hex64 FileSize;
  llvm::yaml::Hex64 MemorySize;
  llvm::yaml::Hex64 FileOffset;
  uint32_t Alignment;
  std::vector<std::string> Sections;
  virtual ~LoadRegion() {}
};

// Cross Reference Table.
struct CRef {
  virtual void mapping(llvm::yaml::IO &IO);
  std::string symbolName;
  std::vector<std::string> FileRefs;
  virtual ~CRef() {}
};

struct Module {
  Header ModuleHeader;
  Version ModuleVersion;
  std::vector<eld::LDYAML::ArchiveRecord> ArchiveRecords;
  std::vector<eld::LDYAML::Common> Commons;
  std::vector<eld::LDYAML::LinkStats> Stats;
  std::vector<std::string> Features;
  std::vector<std::string> InputActions;
  std::vector<std::string> LinkerScriptsUsed;
  CodeGenType BuildType;
  std::string OutputFile;
  std::string CommandLine;
  llvm::yaml::Hex64 EntryAddress;
  std::vector<CommandLineDefault> CommandLineDefaults;
  std::vector<std::shared_ptr<Content>> OutputSections;
  std::vector<std::shared_ptr<eld::LDYAML::InputFile>> InputFileInfo;
  std::vector<std::shared_ptr<eld::LDYAML::DiscardedSection>>
      DiscardedSectionGroups;
  std::vector<std::shared_ptr<eld::LDYAML::DiscardedSection>> DiscardedSections;
  std::vector<eld::LDYAML::Common> DiscardedCommons;
  std::vector<eld::LDYAML::LoadRegion> LoadRegions;
  std::vector<eld::LDYAML::CRef> CrossReferences;
  std::vector<eld::LDYAML::TrampolineInfo> TrampolinesGenerated;
};

} // namespace LDYAML
} // namespace eld
LLVM_YAML_IS_SEQUENCE_VECTOR(eld::LDYAML::ArchiveRecord)
LLVM_YAML_IS_SEQUENCE_VECTOR(eld::LDYAML::Common)
LLVM_YAML_IS_SEQUENCE_VECTOR(eld::LDYAML::LinkStats)
LLVM_YAML_IS_SEQUENCE_VECTOR(eld::LDYAML::Symbol)
LLVM_YAML_IS_SEQUENCE_VECTOR(eld::LDYAML::SimpleSymbol)
LLVM_YAML_IS_SEQUENCE_VECTOR(eld::LDYAML::Trampoline)
LLVM_YAML_IS_SEQUENCE_VECTOR(eld::LDYAML::Reuse)
LLVM_YAML_IS_SEQUENCE_VECTOR(std::shared_ptr<eld::LDYAML::Content>)
LLVM_YAML_IS_SEQUENCE_VECTOR(std::shared_ptr<eld::LDYAML::InputFile>)
LLVM_YAML_IS_SEQUENCE_VECTOR(eld::LDYAML::CommandLineDefault)
LLVM_YAML_IS_SEQUENCE_VECTOR(std::shared_ptr<eld::LDYAML::DiscardedSection>)
LLVM_YAML_IS_SEQUENCE_VECTOR(eld::LDYAML::LoadRegion)
LLVM_YAML_IS_SEQUENCE_VECTOR(eld::LDYAML::CRef)
LLVM_YAML_IS_SEQUENCE_VECTOR(eld::LDYAML::TrampolineInfo)
namespace llvm {
namespace yaml {
template <> struct MappingTraits<eld::LDYAML::Header> {
  static void mapping(IO &IO, eld::LDYAML::Header &Header);
};
template <> struct MappingTraits<eld::LDYAML::Version> {
  static void mapping(IO &IO, eld::LDYAML::Version &Version);
};
template <> struct MappingTraits<eld::LDYAML::ArchiveRecord> {
  static void mapping(IO &IO, eld::LDYAML::ArchiveRecord &Record);
};
template <> struct MappingTraits<eld::LDYAML::Common> {
  static void mapping(IO &IO, eld::LDYAML::Common &Common);
};
template <> struct MappingTraits<eld::LDYAML::LinkStats> {
  static void mapping(IO &IO, eld::LDYAML::LinkStats &LinkStats);
};
template <> struct ScalarBitSetTraits<eld::LDYAML::SymbolType> {
  static void bitset(IO &IO, eld::LDYAML::SymbolType &SymbolType);
};
template <> struct ScalarEnumerationTraits<eld::LDYAML::SymbolScope> {
  static void enumeration(IO &IO, eld::LDYAML::SymbolScope &SymbolScope);
};
template <> struct MappingTraits<eld::LDYAML::Symbol> {
  static void mapping(IO &IO, eld::LDYAML::Symbol &Symbol);
};
template <> struct MappingTraits<eld::LDYAML::SimpleSymbol> {
  static void mapping(IO &IO, eld::LDYAML::SimpleSymbol &Symbol);
};
template <> struct MappingTraits<eld::LDYAML::InputSection> {
  static void mapping(IO &IO, eld::LDYAML::InputSection &Input);
};
template <> struct MappingTraits<eld::LDYAML::InputBitcodeSection> {
  static void mapping(IO &IO, eld::LDYAML::InputBitcodeSection &Input);
};
template <> struct ScalarBitSetTraits<eld::LDYAML::Permissions> {
  static void bitset(IO &IO, eld::LDYAML::Permissions &Permissions);
};
template <> struct ScalarBitSetTraits<eld::LDYAML::SectionType> {
  static void bitset(IO &IO, eld::LDYAML::SectionType &SectionType);
};
template <> struct MappingTraits<eld::LDYAML::Assignment> {
  static void mapping(IO &IO, eld::LDYAML::Assignment &Assignment);
};
template <> struct MappingTraits<eld::LDYAML::LinkerScriptRule> {
  static void mapping(IO &IO, eld::LDYAML::LinkerScriptRule &LinkerScriptRule);
};
template <> struct MappingTraits<eld::LDYAML::Padding> {
  static void mapping(IO &IO, eld::LDYAML::Padding &Padding);
};
template <> struct MappingTraits<eld::LDYAML::OutputSection> {
  static void mapping(IO &IO, eld::LDYAML::OutputSection &Output);
};
template <> struct ScalarEnumerationTraits<eld::LDYAML::CodeGenType> {
  static void enumeration(IO &IO, eld::LDYAML::CodeGenType &CodeGenType);
};
template <> struct MappingTraits<eld::LDYAML::CommandLineDefault> {
  static void mapping(IO &IO,
                      eld::LDYAML::CommandLineDefault &CommandLineDefault);
};
template <> struct MappingTraits<eld::LDYAML::Module> {
  static void mapping(IO &IO, eld::LDYAML::Module &Module);
};
template <> struct MappingTraits<std::shared_ptr<eld::LDYAML::Content>> {
  static void mapping(IO &IO, std::shared_ptr<eld::LDYAML::Content> &Content);
};
template <> struct MappingTraits<eld::LDYAML::DiscardedSection> {
  static void mapping(IO &IO, eld::LDYAML::DiscardedSection &DS);
};
template <> struct MappingTraits<eld::LDYAML::LoadRegion> {
  static void mapping(IO &IO, eld::LDYAML::LoadRegion &DS);
};
template <> struct MappingTraits<eld::LDYAML::CRef> {
  static void mapping(IO &IO, eld::LDYAML::CRef &C);
};
template <> struct ScalarBitSetTraits<eld::LDYAML::SegmentPermissions> {
  static void bitset(IO &IO,
                     eld::LDYAML::SegmentPermissions &SegmentPermissions);
};
template <> struct ScalarBitSetTraits<eld::LDYAML::SegmentType> {
  static void bitset(IO &IO, eld::LDYAML::SegmentType &SegmentType);
};
template <> struct ScalarEnumerationTraits<eld::LDYAML::InputUsed> {
  static void enumeration(IO &IO, eld::LDYAML::InputUsed &InputUsed);
};
template <> struct MappingTraits<std::shared_ptr<eld::LDYAML::InputFile>> {
  static void mapping(IO &IO,
                      std::shared_ptr<eld::LDYAML::InputFile> &InputFile);
};
template <> struct MappingTraits<eld::LDYAML::RegularInput> {
  static void mapping(IO &IO, eld::LDYAML::RegularInput &RegularInput);
};
template <> struct MappingTraits<eld::LDYAML::Archive> {
  static void mapping(IO &IO, eld::LDYAML::Archive &Archive);
};
template <> struct MappingTraits<eld::LDYAML::Trampoline> {
  static void mapping(IO &IO, eld::LDYAML::Trampoline &Trampoline);
};
template <> struct MappingTraits<eld::LDYAML::Reuse> {
  static void mapping(IO &IO, eld::LDYAML::Reuse &Reuse);
};
template <> struct MappingTraits<eld::LDYAML::TrampolineInfo> {
  static void mapping(IO &IO, eld::LDYAML::TrampolineInfo &TInfo);
};
template <>
struct MappingTraits<std::shared_ptr<eld::LDYAML::DiscardedSection>> {
  static void
  mapping(IO &IO,
          std::shared_ptr<eld::LDYAML::DiscardedSection> &DiscardedSection);
};
} // namespace yaml
} // namespace llvm
#endif // ELD_LAYOUTMAP_LDYAML_H
