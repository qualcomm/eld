//===- HexagonLDBackend.h--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef HEXAGON_LDBACKEND_H
#define HEXAGON_LDBACKEND_H

#include "HexagonAttributeFragment.h"
#include "HexagonELFDynamic.h"
#include "HexagonGOT.h"
#include "HexagonPLT.h"
#include "HexagonTLSStub.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Object/ObjectBuilder.h"
#include "eld/Readers/ELFSection.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/Target/GNULDBackend.h"

namespace eld {

class LinkerConfig;
class HexagonInfo;
class RegionFragmentEx;

//===----------------------------------------------------------------------===//
/// HexagonLDBackend - linker backend of Hexagon target of GNU ELF format
///
class HexagonLDBackend : public GNULDBackend {
public:
  HexagonLDBackend(Module &pModule, HexagonInfo *pInfo);

  uint32_t machine() const;

  void initializeAttributes() override;

  bool finalizeScanRelocations() override;

  /// preLayout - Backend can do any needed modification before layout
  void doPreLayout() override;

  bool allocateCommonSymbols() override;

  /// postLayout - Backend can do any needed modification after layout
  void doPostLayout() override;

  /// postProcessing - Backend can do any needed modification in the final stage
  eld::Expected<void> postProcessing(llvm::FileOutputBuffer &pOutput) override;

  /// dynamic - the dynamic section of the target machine.
  /// Use co-variant return type to return its own dynamic section.
  HexagonELFDynamic *dynamic() override;

  /// initRelocator - create and initialize Relocator.
  bool initRelocator() override;

  /// getRelocator - return relocator.
  Relocator *getRelocator() const override;

  ResolveInfo::Desc getSymDesc(uint32_t shndx) const override {
    if (shndx >= llvm::ELF::SHN_HEXAGON_SCOMMON &&
        shndx <= llvm::ELF::SHN_HEXAGON_SCOMMON_8)
      return ResolveInfo::Common;
    return ResolveInfo::NoneDesc;
  }

  std::optional<bool>
  shouldProcessSectionForGC(const ELFSection &S) const override;

  bool validateArchOpts() const override { return m_pInfo->initialize(); }

  void initTargetSections(ObjectBuilder &pBuilder) override;

  void initDynamicSections(ELFObjectFile &) override;

  void initTargetSymbols() override;

  bool initBRIslandFactory() override;

  bool initStubFactory() override;

  void mayBeRelax(int pass, bool &pFinished) override;

  bool initTargetStubs() override;

  /// getTargetSectionOrder - compute the layout order of Hexagon target section
  unsigned int getTargetSectionOrder(const ELFSection &pSectHdr) const override;

  /// finalizeTargetSymbols - finalize the symbol value
  bool finalizeTargetSymbols() override;

  bool DoesOverrideMerge(ELFSection *pSection) const override;

  /// mergeSection - merge target dependent sections
  ELFSection *mergeSection(ELFSection *pSection) override;

  bool MoveSectionAndSort(ELFSection *pFrom, ELFSection *pTo);

  bool SetSDataSection();

  uint32_t getGP();

  uint32_t getMsgBase() const;

  int32_t getPacketOffset(const Relocation &reloc) const override;

  /// LTO Flow Setup
  bool ltoNeedAssembler() override;
  bool ltoCallExternalAssembler(const std::string &Input,
                                std::string RelocModel,
                                const std::string &Output) override;

  void AddLTOOptions(std::vector<std::string> &) override;

  ELFSection *getGuard() const { return m_pguard; }

  uint64_t getValueForDiscardedRelocations(const Relocation *R) const override;

  void mayWarnSection(ELFSection *sect) const override;

  Relocation::Type getCopyRelType() const override;

private:
  ELFSection *createGOTSection(InputFile &InputFile);
  ELFSection *createGOTPLTSection(InputFile &InputFile);
  ELFSection *createPLTSection(InputFile &InputFile);

  void defineGOTSymbol(Fragment &);

private:
  /// getRelEntrySize - the size in BYTE of rela type relocation
  size_t getRelEntrySize() override { return 0; }

  /// getRelaEntrySize - the size in BYTE of rela type relocation
  size_t getRelaEntrySize() override { return 12; }

  uint64_t maxBranchOffset() override { return ~(~0U << 12); }

  uint64_t getCommonSectionHash(LDSymbol *sym) {
    int8_t maxGPSize = config().options().getGPSize();
    uint32_t shndx = sym->sectionIndex();
    bool isSmallData = ((shndx >= llvm::ELF::SHN_HEXAGON_SCOMMON) &&
                        (shndx <= llvm::ELF::SHN_HEXAGON_SCOMMON_8));
    if (isSmallData) {
      switch (shndx) {
      case llvm::ELF::SHN_HEXAGON_SCOMMON_1:
        return m_scommon_1_hash;
      case llvm::ELF::SHN_HEXAGON_SCOMMON_2:
        return m_scommon_2_hash;
      case llvm::ELF::SHN_HEXAGON_SCOMMON_4:
        return m_scommon_4_hash;
      default:
        return m_scommon_8_hash;
      }
    }

    int32_t sz = sym->size();

    if (maxGPSize < sz)
      return m_common_hash;

    if (sz <= 1)
      return m_scommon_1_hash;
    else if (sz <= 2)
      return m_scommon_2_hash;
    else if (sz <= 4)
      return m_scommon_4_hash;
    else if (sz <= 8)
      return m_scommon_8_hash;
    return m_common_hash;
  }

  std::string getCommonSectionName(LDSymbol *sym) {
    int8_t maxGPSize = config().options().getGPSize();
    uint32_t shndx = sym->sectionIndex();
    bool isSmallData = ((shndx >= llvm::ELF::SHN_HEXAGON_SCOMMON) &&
                        (shndx <= llvm::ELF::SHN_HEXAGON_SCOMMON_8));
    if (isSmallData) {
      switch (shndx) {
      case llvm::ELF::SHN_HEXAGON_SCOMMON_1:
        return ".scommon.1";
      case llvm::ELF::SHN_HEXAGON_SCOMMON_2:
        return ".scommon.2";
      case llvm::ELF::SHN_HEXAGON_SCOMMON_4:
        return ".scommon.4";
      default:
        return ".scommon.8";
      }
    }

    int32_t sz = sym->size();

    if (maxGPSize < sz)
      return "COMMON";

    if (sz <= 1)
      return ".scommon.1";
    else if (sz <= 2)
      return ".scommon.2";
    else if (sz <= 4)
      return ".scommon.4";
    else if (sz <= 8)
      return ".scommon.8";
    return "COMMON";
  }

public:
  // ---  TLS Stubs Support ------

  HexagonTLSStub *createTLSStub(HexagonTLSStub::StubType T);

  HexagonTLSStub *findTLSStub(std::string stubName);

  void recordTLSStub(std::string stubName, HexagonTLSStub *T);

  // ---  GOT Support ------
  HexagonGOT *createGOT(GOT::GOTType T, ELFObjectFile *Obj, ResolveInfo *sym);

  void recordGOT(ResolveInfo *, HexagonGOT *);

  void recordGOTPLT(ResolveInfo *, HexagonGOT *);

  HexagonGOT *findEntryInGOT(ResolveInfo *) const;

  // ---  PLT Support ------
  HexagonPLT *createPLT(ELFObjectFile *Obj, ResolveInfo *sym);

  void recordPLT(ResolveInfo *, HexagonPLT *);

  HexagonPLT *findEntryInPLT(ResolveInfo *) const;

  bool hasSymInfo(const Relocation *X) const override {
    if (X->type() == llvm::ELF::R_HEX_RELATIVE)
      return false;
    if (X->symInfo() && X->symInfo()->binding() == ResolveInfo::Local)
      return false;
    return true;
  }

  DynRelocType getDynRelocType(const Relocation *X) const override {
    if (X->type() == llvm::ELF::R_HEX_GLOB_DAT)
      return DynRelocType::GLOB_DAT;
    if (X->type() == llvm::ELF::R_HEX_JMP_SLOT)
      return DynRelocType::JMP_SLOT;
    if (X->type() == llvm::ELF::R_HEX_32)
      return DynRelocType::WORD_DEPOSIT;
    if (X->type() == llvm::ELF::R_HEX_RELATIVE)
      return DynRelocType::RELATIVE;
    if (X->type() == llvm::ELF::R_HEX_DTPMOD_32) {
      if (X->symInfo() && X->symInfo()->binding() == ResolveInfo::Local)
        return DynRelocType::DTPMOD_LOCAL;
      return DynRelocType::DTPMOD_GLOBAL;
    }
    if (X->type() == llvm::ELF::R_HEX_DTPREL_32) {
      if (X->symInfo() && X->symInfo()->binding() == ResolveInfo::Local)
        return DynRelocType::DTPREL_LOCAL;
      return DynRelocType::DTPREL_GLOBAL;
    }
    if (X->type() == llvm::ELF::R_HEX_TPREL_32) {
      if (X->symInfo() && X->symInfo()->binding() == ResolveInfo::Local)
        return DynRelocType::TPREL_LOCAL;
      return DynRelocType::TPREL_GLOBAL;
    }
    return DynRelocType::DEFAULT;
  }

  Stub *getBranchIslandStub(Relocation *pReloc,
                            int64_t pTargetValue) const override;

  /// readSection - read target dependent sections
  bool readSection(InputFile &pInput, ELFSection *S) override;

  bool addSymbols() override;

  bool finalizeLayout() override;

  // --- Relaxed Relocations support ---
  bool isRelocationRelaxed(Relocation *R) const override;

  std::size_t PLTEntriesCount() const override { return m_PLTMap.size(); }

  std::size_t GOTEntriesCount() const override { return m_GOTMap.size(); }

private:
  // Do relaxation on Hexagon!
  bool canSectionBeRelaxed(InputFile &pInput, ELFSection *S) const;

  // Can fragment be relaxed ?
  bool canFragmentBeRelaxed(Fragment *F) const;

  bool
  haslinkerRelaxed(const std::vector<RegionFragmentEx *> &FragsForRelaxation);

  /// If a common symbol has a small common section index
  /// SHN_HEXAGON_SCOMMON_X, then returns the corresponding small common section
  /// name
  /// '.scommon.x.SymbolName'.
  /// If a common symbol does not have a small common section index,
  /// then return a small common section name '.scommon.x.SymbolName' only if
  /// the symbol size satisfies the group optimization size constraint.
  /// Otherwise, return 'COMMON.SymbolName'
  std::string computeInternalCommonSectionName(const LDSymbol *comSym) const;

  void createAttributeSection();

private:
  Relocator *m_pRelocator;

  HexagonELFDynamic *m_pDynamic;

  ELFSection *m_psdata;
  ELFSection *m_pscommon_1;
  ELFSection *m_pscommon_2;
  ELFSection *m_pscommon_4;
  ELFSection *m_pscommon_8;
  ELFSection *m_pstart;
  ELFSection *m_pguard;
  LDSymbol *m_psdabase;

  // .hexagon.attributes section
  ELFSection *AttributeSection;
  HexagonAttributeFragment *AttributeFragment;

  LDSymbol *m_pBSSEnd;
  LDSymbol *m_pTLSBASE;
  LDSymbol *m_pTDATAEND;
  LDSymbol *m_pTLSEND;

  uint64_t m_scommon_1_hash;
  uint64_t m_scommon_2_hash;
  uint64_t m_scommon_4_hash;
  uint64_t m_scommon_8_hash;
  uint64_t m_common_hash;
  LDSymbol *m_pMSGBase;
  LDSymbol *m_pEndOfImage;

  llvm::DenseMap<ResolveInfo *, HexagonGOT *> m_GOTMap;
  llvm::DenseMap<ResolveInfo *, HexagonGOT *> m_GOTPLTMap;
  llvm::DenseMap<ResolveInfo *, HexagonPLT *> m_PLTMap;
  llvm::StringMap<HexagonTLSStub *> m_TLSStubMap;
  llvm::StringMap<ELFSection *> m_TLSStubs;
  // Relocations discarded because of relaxation
  std::unordered_set<Relocation *> m_RelaxedRelocs;
  std::mutex Mutex;
};
} // namespace eld

#endif
