//===- ELFSection.h--------------------------------------------------------===//
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

#ifndef ELD_READERS_ELFSECTION_H
#define ELD_READERS_ELFSECTION_H
#include "eld/BranchIsland/BranchIsland.h"
#include "eld/Input/InputFile.h"
#include "eld/Readers/Section.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/Target/LDFileFormat.h"
#include "llvm/ADT/Hashing.h"
#include <string>

namespace eld {

class BranchIsland;
class InputFile;
class Section;

class ELFSectionBase : public Section {
public:
  virtual ~ELFSectionBase() {}

  uint32_t getType() const { return Type; }
  uint32_t getFlags() const { return Flags; }
  uint32_t getAddrAlign() const override { return AddrAlign; }
  uint32_t getEntSize() const { return EntSize; }
  ELFSectionBase *getLink() const { return Link; }
  uint32_t getInfo() const { return Info; }
  LDFileFormat::Kind getKind() const { return ELFSectionKind; }
  uint32_t getIndex() const { return Index; }

  void setFlags(uint32_t F) { Flags = F; }
  void setType(uint32_t T) { Type = T; }
  void setAddrAlign(uint32_t A) { AddrAlign = A; }
  void setEntSize(uint32_t E) { EntSize = E; }
  void setInfo(uint32_t I) { Info = I; }
  void setKind(LDFileFormat::Kind K) { ELFSectionKind = K; }
  void setIndex(uint32_t I) { Index = I; }
  void setLink(ELFSectionBase *L) { Link = L; }

  bool isIgnore() const { return ELFSectionKind == LDFileFormat::Ignore; }
  bool isMergeKind() const { return ELFSectionKind == LDFileFormat::MergeStr; }
  bool isNullKind() const { return ELFSectionKind == LDFileFormat::Null; }
  bool isDiscard() const { return ELFSectionKind == LDFileFormat::Discard; }
  bool isNoteGNUStack() const {
    return ELFSectionKind == LDFileFormat::StackNote;
  }
  bool isDebugKind() const { return ELFSectionKind == LDFileFormat::Debug; }
  bool isNoteKind() const { return ELFSectionKind == LDFileFormat::Note; }
  bool isGroupKind() const { return ELFSectionKind == LDFileFormat::Group; }
  bool isNamePool() const { return ELFSectionKind == LDFileFormat::NamePool; }
  bool isRelocationKind() const {
    return ELFSectionKind == LDFileFormat::Relocation;
  }

  bool isProgBits() const { return Type == llvm::ELF::SHT_PROGBITS; }
  bool isNoBits() const { return Type == llvm::ELF::SHT_NOBITS; }
  bool isEXIDX() const { return Type == llvm::ELF::SHT_ARM_EXIDX; }
  bool isNullType() const { return Type == llvm::ELF::SHT_NULL; }
  bool isRela() const { return Type == llvm::ELF::SHT_RELA; }
  bool isRel() const { return Type == llvm::ELF::SHT_REL; }
  bool isRelocationSection() const {
    return Type == llvm::ELF::SHT_REL || Type == llvm::ELF::SHT_RELA;
  }

  bool isCode() const { return (Flags & llvm::ELF::SHF_EXECINSTR); }
  bool isWritable() const { return Flags & llvm::ELF::SHF_WRITE; }
  bool isTLS() const { return Flags & llvm::ELF::SHF_TLS; }
  bool isBSS() const { return !isTLS() && isNoBits(); }
  bool isTBSS() const { return isTLS() && isNoBits(); }
  bool isAlloc() const { return Flags & llvm::ELF::SHF_ALLOC; }
  bool isLinkOrder() const { return Flags & llvm::ELF::SHF_LINK_ORDER; }
  bool isUninit() const { return Flags == 0; }
  bool isInGroup() const { return Flags & llvm::ELF::SHF_GROUP; }
  bool isRetain() const { return Flags & llvm::ELF::SHF_GNU_RETAIN; }
  bool isCompressed() const { return Flags & llvm::ELF::SHF_COMPRESSED; }
  bool isMergeStr() const {
    return (Flags & llvm::ELF::SHF_MERGE) && (Flags & llvm::ELF::SHF_STRINGS);
  }
  bool isNote() const { return Type == llvm::ELF::SHT_NOTE; }

protected:
  explicit ELFSectionBase(Section::Kind SectionKind,
                          LDFileFormat::Kind ELFSectionKind,
                          const std::string &Name, uint32_t Flags,
                          uint32_t EntSize, uint32_t AddrAlign, uint32_t Type,
                          uint32_t Info, ELFSectionBase *Link,
                          uint32_t SectionSize)
      : Section(SectionKind, Name, SectionSize), Type(Type), Flags(Flags),
        AddrAlign(AddrAlign), EntSize(EntSize), Link(Link), Info(Info),
        ELFSectionKind(ELFSectionKind) {}

  // Elf_Shdr fields
  uint32_t Type;
  uint32_t Flags;
  uint32_t AddrAlign;
  uint32_t EntSize;
  /// FIXME: Not every section has a link. This might be better stored in a map
  /// elsewhere, or as a uint32_t index.
  ELFSectionBase *Link;
  uint32_t Info;

  uint32_t Index = 0;
  LDFileFormat::Kind ELFSectionKind;
};

/** \class ELFSection
 *  \brief ELFSection represents a section header entry. It is a unified
 *  abstraction of a section header entry for various file formats.
 */
class ELFSection : public ELFSectionBase {
public:
  explicit ELFSection(LDFileFormat::Kind ELFSectionKind,
                      const std::string &Name, uint32_t Flags, uint32_t EntSize,
                      uint32_t AddrAlign, uint32_t Type, uint32_t Info,
                      ELFSectionBase *Link, uint32_t SectionSize,
                      uint64_t PAddr)
      : ELFSectionBase(Section::ELF, ELFSectionKind, Name, Flags, EntSize,
                       AddrAlign, Type, Info, Link, SectionSize),
        PAddr(PAddr) {}

  explicit ELFSection(Section::Kind SectionKind,
                      LDFileFormat::Kind ELFSectionKind,
                      const std::string &Name, uint32_t Flags, uint32_t EntSize,
                      uint32_t AddrAlign, uint32_t Type, uint32_t Info,
                      ELFSectionBase *Link, uint32_t SectionSize,
                      uint64_t PAddr)
      : ELFSectionBase(SectionKind, ELFSectionKind, Name, Flags, EntSize,
                       AddrAlign, Type, Info, Link, SectionSize),
        PAddr(PAddr) {}

  static bool classof(const Section *S) { return S->isELF(); }

  virtual ~ELFSection() {}

  // LayoutPrinter helper functions
  static llvm::StringRef getELFTypeStr(llvm::StringRef Name, uint32_t Type);

  static std::string getELFPermissionsStr(uint32_t Permissions);

  bool hasOffset() const;

  /// FIXME: We change the offset for input sections so this will not return the
  /// right content throughout the link.
  llvm::StringRef getContents() const {
    return m_InputFile->getSlice(Offset, size());
  }

  ELFSection *getLink() const {
    return llvm::dyn_cast_or_null<ELFSection>(Link);
  }

  void setHasNoFragments() { HasNoFragments = true; }

  bool hasNoFragments() const { return HasNoFragments; }

  void setFancyOffset() { IsFancyOffset = true; }

  bool isFancyOffset() const { return IsFancyOffset; }

  /// offset - An integer specifying the offset of this section in the file.
  ///   Before layouting, output's ELFSection::offset() should return zero.
  uint64_t offset() const { return Offset; }

  void setNoOffset() { Offset = (uint64_t)~0; }

  /// addr - An integer specifying the virtual address of this section in the
  /// virtual image.
  uint64_t addr() const { return Addr ? *Addr : 0; }

  bool hasVMA() const { return Addr.has_value(); }

  uint64_t pAddr() const { return PAddr; }

  void setOffsetAndAddr(uint64_t Off);

  void setOffset(uint64_t Off) { Offset = Off; }

  void setAddr(uint64_t A) { Addr = A; }

  void setWanted(bool W) { Wanted = W; }

  bool isWanted() const { return Wanted; }

  void setWantedInOutput(bool IsWanted = true) { WantedInOutput = IsWanted; }

  bool wantedInOutput() const {
    return !isDiscard() && !isIgnore() && WantedInOutput;
  }

  void setPaddr(size_t A) { PAddr = A; }

  void setSymbol(LDSymbol *S) { Symbol = S; }

  LDSymbol *getSymbol() const { return Symbol; }

  llvm::SmallVectorImpl<const ELFSection *> &getGroupSections() {
    return GroupSections;
  }

  void addSectionsToGroup(const ELFSection *S) { GroupSections.push_back(S); }

  ELFSection *getOutputELFSection() const {
    return m_OutputSection ? m_OutputSection->getSection() : nullptr;
  }

  //  LTO Tracking support
  bool hasOldInputFile() const override { return OldInput != nullptr; }

  InputFile *getOldInputFile() const override { return OldInput; }

  void setOldInputFile(InputFile *I) { OldInput = I; }

  ///  __attribute__((at(address))) support
  void setFixedAddr() { IsFixedAddr = true; }

  bool isFixedAddr() const { return IsFixedAddr; }

  bool hasSectionData() const;

  llvm::SmallVectorImpl<Fragment *> &getFragmentList() { return Fragments; }

  void splice(llvm::SmallVectorImpl<Fragment *>::iterator Where,
              llvm::SmallVectorImpl<Fragment *> &InputVector,
              bool DoClear = true) {
    Fragments.insert(Where, InputVector.begin(), InputVector.end());
    if (DoClear)
      InputVector.clear();
  }

  void addFragment(Fragment *F);

  void remove(llvm::SmallVectorImpl<Fragment *>::iterator Iter) {
    Fragments.erase(Iter);
  }

  /// Returns true if the fragment is removed; Otherwise returns false.
  bool removeFragment(Fragment *F) {
    auto *Iter = std::find(Fragments.begin(), Fragments.end(), F);
    if (Iter != Fragments.end()) {
      Fragments.erase(Iter);
      return true;
    }
    return false;
  }

  void clearFragments() { Fragments.clear(); }

  void addFragmentAndUpdateSize(Fragment *F);

  bool hasRelocData() const { return (!Relocations.empty()); }

  llvm::SmallVectorImpl<Relocation *> &getRelocations() { return Relocations; }

  void addRelocation(Relocation *R) {
    assert(R);
    Relocations.push_back(R);
    if (!R->targetSection())
      return;
    if (!R->targetSection()->size())
      R->targetSection()->setWanted(true);
  }

  Relocation *findRelocation(uint64_t Offset, Relocation::Type Type,
                             bool Reverse = true) const;

  Relocation *createOneReloc();

  // Linker Script support for sorting sections.
  int getPriority() const;

  void addDependentSection(ELFSection *S) { DependentSections.push_back(S); }

  const llvm::SmallVectorImpl<ELFSection *> &getDependentSections() const {
    return DependentSections;
  }

  Fragment *getFirstFragmentInRule() const;

  std::string getDecoratedName(const GeneralOptions &options) const override;

  virtual bool verify(DiagnosticEngine *DiagEngine) const { return true; }

  uint64_t getSectionHash() const override {
    return llvm::hash_combine(m_Name, Flags,
                              originalInput()->getInput()->decoratedPath());
  }

  void setExcludedFromGC() { ShouldExcludeFromGC = true; }
  bool isExcludedFromGC() const { return ShouldExcludeFromGC; }

  std::optional<std::string> getRMSectName() const;

protected:
  /// FIXME: This has different meanings for Input/Output sections.
  uint64_t Offset = ~uint64_t(0);
  std::optional<uint64_t> Addr;
  /// FIXME: only relevant for output sections.
  uint64_t PAddr;
  LDSymbol *Symbol = nullptr;

  /// FIXME: Only relevant for LTO. This should be moved out.
  InputFile *OldInput = nullptr;

  /// FIXME: These can probably be moved out
  bool Wanted = false;
  bool WantedInOutput = false;
  bool IsFixedAddr = false;
  bool IsFancyOffset = false;

  /// FIXME: We can just query the fragment list instead of storing this?
  bool HasNoFragments = false;

  /// FIXME: We only use this for dynamic relocation sections. We can just check
  /// the section properties instead of storing this?
  bool ShouldExcludeFromGC = false;

  llvm::SmallVector<Fragment *, 0> Fragments;
  llvm::SmallVector<Relocation *, 0> Relocations;

  /// FIXME: These vectors can be moved out of this class?
  llvm::SmallVector<const ELFSection *, 0> GroupSections;
  llvm::SmallVector<ELFSection *, 0> DependentSections;
};

} // namespace eld

#endif
