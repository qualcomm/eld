//===- RISCVLDBackend.h----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
#ifndef RISCV_LDBACKEND_H
#define RISCV_LDBACKEND_H

#include "RISCVGOT.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Fragment/RegionFragmentEx.h"
#include "eld/Object/ObjectBuilder.h"
#include "eld/Readers/ELFSection.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "eld/Target/GNULDBackend.h"
#include <unordered_set>

namespace eld {

class LinkerConfig;
class RISCVInfo;
class RISCVAttributeFragment;
class RISCVELFDynamic;
class RISCVPLT;
class RISCVRelaxationStats;

//===----------------------------------------------------------------------===//
/// RISCVLDBackend - linker backend of RISCV target of GNU ELF format
///
class RISCVLDBackend : public GNULDBackend {
public:
  RISCVLDBackend(Module &pModule, RISCVInfo *pInfo);

  ~RISCVLDBackend();

  void initializeAttributes() override;

  /// initRelocator - create and initialize Relocator.
  bool initRelocator() override;

  /// getRelocator - return relocator.
  Relocator *getRelocator() const override;

  void initTargetSections(ObjectBuilder &pBuilder) override;

  void initDynamicSections(ELFObjectFile &) override;

  void initPatchSections(ELFObjectFile &) override;

  void initTargetSymbols() override;

  bool initBRIslandFactory() override;

  bool initStubFactory() override;

  void mayBeRelax(int pass, bool &pFinished) override;

  /// getTargetSectionOrder - compute the layout order of RISCV target section
  unsigned int getTargetSectionOrder(const ELFSection &pSectHdr) const override;

  /// finalizeTargetSymbols - finalize the symbol value
  bool finalizeTargetSymbols() override;

  ELFDynamic *dynamic() override;

  void evaluateTargetSymbolsBeforeRelaxation() override;

  Stub *getBranchIslandStub(Relocation *pReloc,
                            int64_t pTargetValue) const override {
    return nullptr;
  }

  bool validateArchOpts() const override;

  void doPreLayout() override;

  bool handleRelocation(ELFSection *pSection, Relocation::Type pType,
                        LDSymbol &pSym, uint32_t pOffset,
                        Relocation::Address pAddend = 0,
                        bool pLastPass = false) override;

  // Handle the relocations that handleRelocation() could not process.
  bool handlePendingRelocations(ELFSection *S) override;

  virtual bool readSection(InputFile &pInput, ELFSection *S) override;

  bool shouldIgnoreRelocSync(Relocation *pReloc) const override;

  void translatePseudoRelocation(Relocation *reloc);

  Relocation::Type getCopyRelType() const override;

  // ---  GOT Support ------
  RISCVGOT *createGOT(GOT::GOTType T, ELFObjectFile *Obj, ResolveInfo *sym);

  void recordGOT(ResolveInfo *, RISCVGOT *);

  void recordGOTPLT(ResolveInfo *, RISCVGOT *);

  RISCVGOT *findEntryInGOT(ResolveInfo *) const;

  bool addSymbolToOutput(ResolveInfo *pInfo) override;

  uint64_t getValueForDiscardedRelocations(const Relocation *R) const override;

  // ----------------------- GC override ----------------------------
  std::optional<bool>
  shouldProcessSectionForGC(const ELFSection &pSec) const override;

  // ---------------------  PLT Support ---------------------------
  RISCVPLT *createPLT(ELFObjectFile *Obj, ResolveInfo *sym);

  void recordPLT(ResolveInfo *, RISCVPLT *);

  RISCVPLT *findEntryInPLT(ResolveInfo *) const;

  // ---------------------  Dynamic relocation support ------------
  bool hasSymInfo(const Relocation *X) const override {
    if (X->type() == llvm::ELF::R_RISCV_RELATIVE)
      return false;
    if (X->symInfo() && X->symInfo()->binding() == ResolveInfo::Local)
      return false;
    return true;
  }

  DynRelocType getDynRelocType(const Relocation *X) const override {
    // RISCV uses word deposits as GLOB_DAT on
    // other targets
    if (X->type() == llvm::ELF::R_RISCV_32 ||
        X->type() == llvm::ELF::R_RISCV_64)
      return DynRelocType::GLOB_DAT;
    if (X->type() == llvm::ELF::R_RISCV_JUMP_SLOT)
      return DynRelocType::JMP_SLOT;
    if (X->type() == llvm::ELF::R_RISCV_RELATIVE)
      return DynRelocType::RELATIVE;
    if (X->type() == llvm::ELF::R_RISCV_TLS_DTPMOD32 ||
        X->type() == llvm::ELF::R_RISCV_TLS_DTPMOD64) {
      if (X->symInfo() && X->symInfo()->binding() == ResolveInfo::Local)
        return DynRelocType::DTPMOD_LOCAL;
      return DynRelocType::DTPMOD_GLOBAL;
    }
    if (X->type() == llvm::ELF::R_RISCV_TLS_DTPREL32 ||
        X->type() == llvm::ELF::R_RISCV_TLS_DTPREL64) {
      if (X->symInfo() && X->symInfo()->binding() == ResolveInfo::Local)
        return DynRelocType::DTPREL_LOCAL;
      return DynRelocType::DTPREL_GLOBAL;
    }
    if (X->type() == llvm::ELF::R_RISCV_TLS_TPREL32 ||
        X->type() == llvm::ELF::R_RISCV_TLS_TPREL64) {
      if (X->symInfo() && X->symInfo()->binding() == ResolveInfo::Local)
        return DynRelocType::TPREL_LOCAL;
      return DynRelocType::TPREL_GLOBAL;
    }
    return DynRelocType::DEFAULT;
  }

  std::size_t PLTEntriesCount() const override { return m_PLTMap.size(); }

  std::size_t GOTEntriesCount() const override { return m_GOTMap.size(); }

  void doCreateProgramHdrs() override;

  int numReservedSegments() const override;

  void addTargetSpecificSegments() override;

  void setDefaultConfigs() override;

  Relocation *getPairedReloc(Relocation *R) const {
    auto reloc = m_PairedRelocs.find(R);
    if (reloc == m_PairedRelocs.end())
      return nullptr;
    return reloc->second;
  }

  // Get the value of the symbol, using the PLT slot if one exists.
  Relocation::Address getSymbolValuePLT(Relocation &R);

private:
  Relocation *findHIRelocation(ELFSection *S, uint64_t Value);

  // This is `handleRelocation` for internal RISC-V relocations IDs.
  bool handleVendorRelocation(ELFSection *pSection,
                              Relocation::Type pInternalType, LDSymbol &pSym,
                              uint32_t pOffset, Relocation::Address pAddend = 0,
                              bool pLastPass = false);

  void relaxDeleteBytes(llvm::StringRef Name, RegionFragmentEx &Region,
                        uint64_t Offset, unsigned NumBytes,
                        llvm::StringRef SymbolName);

  void reportMissedRelaxation(llvm::StringRef Name, RegionFragmentEx &Region,
                              uint64_t Offset, unsigned NumBytes,
                              llvm::StringRef SymbolName);

  bool isGOTReloc(Relocation *reloc) const;

  bool doRelaxationCall(Relocation *R, bool DoCompressed);
  bool doRelaxationQCCall(Relocation *R, bool DoCompressed);

  bool doRelaxationLui(Relocation *R, Relocation::DWord G);

  bool doRelaxationAlign(Relocation *R);

  bool doRelaxationPC(Relocation *R, Relocation::DWord G);

  /// getRelEntrySize - the size in BYTE of rela type relocation
  size_t getRelEntrySize() override { return 0; }

  /// getRelaEntrySize - the size in BYTE of rela type relocation
  size_t getRelaEntrySize() override {
    if (config().targets().is32Bits())
      return 12;
    return 24;
  }

  uint64_t maxBranchOffset() override { return 0; }

  bool checkABIStr(llvm::StringRef abi) const;

  bool finalizeScanRelocations() override;

  bool fitsInGP(Relocation::DWord, Relocation::DWord, Fragment *frag,
                ELFSection *TargetSection, size_t) const;

  bool DoesOverrideMerge(ELFSection *pSection) const override;

  ELFSection *mergeSection(ELFSection *pSection) override;

  void recordRelaxationStats(ELFSection &, size_t NumBytesDeleted,
                             size_t NumBytesNotDeleted);

  /// postProcessing - Backend can do any needed modification in the final stage
  eld::Expected<void> postProcessing(llvm::FileOutputBuffer &pOutput) override;

private:
  ELFSection *createGOTSection(InputFile &InputFile);
  ELFSection *createGOTPLTSection(InputFile &InputFile);
  ELFSection *createPLTSection(InputFile &InputFile);

  void defineGOTSymbol(Fragment &);

public:
  llvm::DenseMap<Relocation *, Relocation *> m_PairedRelocs;

private:
  /// RISCV Attribute Section
  ELFSection *m_pRISCVAttributeSection = nullptr;
  // RISCV Dynamic section
  RISCVELFDynamic *m_pDynamic = nullptr;
  /// RISCV Attribute Fragment
  RISCVAttributeFragment *AttributeFragment = nullptr;

  llvm::DenseMap<ResolveInfo *, RISCVGOT *> m_GOTMap;
  llvm::DenseMap<ResolveInfo *, RISCVGOT *> m_GOTPLTMap;
  llvm::DenseMap<ResolveInfo *, RISCVPLT *> m_PLTMap;
  std::vector<ResolveInfo *> m_LabeledSymbols;
  using PendingRelocInfo =
      std::tuple<ELFSection *, Relocation::Type, LDSymbol *, uint32_t,
                 Relocation::Address>;
  std::vector<PendingRelocInfo> m_PendingRelocations;
  std::unordered_set<Relocation *> m_DisableGPRelocs;
  Relocator *m_pRelocator = nullptr;
  LDSymbol *m_pGlobalPointer = nullptr;
  ELFSection *m_GlobalPointerSection = nullptr;
  ELFSection *m_psdata = nullptr;
  std::unordered_map<void *, LinkStats *> m_Stats;
  RISCVRelaxationStats *m_ModuleStats = nullptr;
  std::unordered_map<ELFSection *, std::unordered_map<uint32_t, Relocation *>>
      SectionRelocMap;
};
} // namespace eld

#endif
