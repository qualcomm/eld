//===- ARMLDBackend.h------------------------------------------------------===//
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

#ifndef TARGET_ARM_ARMLDBACKEND_H
#define TARGET_ARM_ARMLDBACKEND_H

#include "ARMELFDynamic.h"
#include "ARMGOT.h"
#include "ARMPLT.h"
#include "eld/Fragment/RegionTableFragment.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Support/Memory.h"
#include "eld/Target/GNULDBackend.h"

#define THM_MAX_BRANCH_BITS 23
#define THM2_MAX_BRANCH_BITS 25

namespace eld {

class LinkerConfig;
class TargetInfo;
class ARMAttributeFragment;

//===----------------------------------------------------------------------===//
/// ARMGNULDBackend - linker backend of ARM target of GNU ELF format
///
class ARMGNULDBackend : public GNULDBackend {
public:
  // max branch offsets for ARM, THUMB, and THUMB2
  // @ref gold/arm.cc:99
  static const int32_t ARM_MAX_FWD_BRANCH_OFFSET = ((((1 << 23) - 1) << 2) + 8);
  static const int32_t ARM_MAX_BWD_BRANCH_OFFSET = ((-((1 << 23) << 2)) + 8);

  enum VENEER_TYPE { VENEER_ABS, VENEER_PIC, VENEER_MOV, VENEER_THUMB1 };

public:
  ARMGNULDBackend(Module &pModule, TargetInfo *pInfo);
  ~ARMGNULDBackend();

public:
  typedef std::vector<llvm::ELF::Elf32_Dyn *> ELF32DynList;

public:
  /// initTargetSections - initialize target dependent sections in output.
  void initTargetSections(ObjectBuilder &pBuilder) override;

  /// Create dynamic input sections in an input file.
  void initDynamicSections(ELFObjectFile &) override;

  /// initTargetSymbols - initialize target dependent symbols in output.
  void initTargetSymbols() override;

  /// initRelocator - create and initialize Relocator.
  bool initRelocator() override;

  /// getRelocator - return relocator.
  Relocator *getRelocator() const override;

  //  -----  relaxation  -----  //
  /// initBRIslandFactory - initialize the branch island factory for relaxation
  bool initBRIslandFactory() override;

  /// initStubFactory - initialize the stub factory for relaxation
  bool initStubFactory() override;

  /// doPreLayout - Backend can do any needed modification before layout
  void doPreLayout() override;

  /// doPostLayout -Backend can do any needed modification after layout
  void doPostLayout() override;

  /// dynamic - the dynamic section of the target machine.
  /// Use co-variant return type to return its own dynamic section.
  ARMELFDynamic *dynamic() override;

  eld::Expected<uint64_t> emitSection(ELFSection *pSection,
                                      MemoryRegion &pRegion) const override;

  /// getTargetSectionOrder - compute the layout order of ARM target sections
  unsigned int getTargetSectionOrder(const ELFSection &pSectHdr) const override;

  bool finalizeScanRelocations() override;

  /// finalizeTargetSymbols - finalize the symbol value
  bool finalizeTargetSymbols() override;

  bool DoesOverrideMerge(ELFSection *pSection) const override;

  /// mergeSection - merge target dependent sections
  ELFSection *mergeSection(ELFSection *pSection) override;

  /// readSection - read target dependent sections
  bool readSection(InputFile &pInput, ELFSection *S) override;

  /// setUpReachedSectionsForGC - set the reference from section XXX to
  /// .ARM.exidx.XXX to make sure GC correctly handle section exidx
  void setUpReachedSectionsForGC(GarbageCollection::SectionReachedListMap
                                     &pSectReachedListMap) const override;

  void initSegmentFromLinkerScript(ELFSegment *pSegment) override;

  int numReservedSegments() const override;

  /// LTO Flow Setup
  bool ltoNeedAssembler() override;
  bool ltoCallExternalAssembler(const std::string &Input,
                                std::string RelocModel,
                                const std::string &Output) override;

  void defineIRelativeRange(ResolveInfo &pSym);

  uint64_t getSectLink(const ELFSection *S) const override;

  Relocation::Type getCopyRelType() const override;

  // ---  GOT Support ------
  ARMGOT *createGOT(GOT::GOTType T, ELFObjectFile *Obj, ResolveInfo *sym,
                    bool SkipPLTRef = false);

  void recordGOT(ResolveInfo *, ARMGOT *);

  void recordGOTPLT(ResolveInfo *, ARMGOT *);

  ARMGOT *findEntryInGOT(ResolveInfo *) const;

  // ---  PLT Support ------
  ARMPLT *createPLT(ELFObjectFile *Obj, ResolveInfo *sym,
                    bool isIRelative = false);

  void recordPLT(ResolveInfo *, ARMPLT *);

  ARMPLT *findEntryInPLT(ResolveInfo *) const;

  int64_t getPLTAddr(ResolveInfo *pInfo) const override;

  bool hasSymInfo(const Relocation *X) const override {
    if (X->type() == llvm::ELF::R_ARM_IRELATIVE)
      return false;
    if (X->type() == llvm::ELF::R_ARM_RELATIVE)
      return false;
    if (X->symInfo() && X->symInfo()->binding() == ResolveInfo::Local)
      return false;
    return true;
  }

  DynRelocType getDynRelocType(const Relocation *X) const override {
    if (X->type() == llvm::ELF::R_ARM_GLOB_DAT)
      return DynRelocType::GLOB_DAT;
    if (X->type() == llvm::ELF::R_ARM_JUMP_SLOT)
      return DynRelocType::JMP_SLOT;
    if (X->type() == llvm::ELF::R_ARM_ABS32)
      return DynRelocType::WORD_DEPOSIT;
    if (X->type() == llvm::ELF::R_ARM_RELATIVE)
      return DynRelocType::RELATIVE;
    if (X->type() == llvm::ELF::R_ARM_IRELATIVE)
      return DynRelocType::RELATIVE;
    if (X->type() == llvm::ELF::R_ARM_TLS_DTPMOD32) {
      if (X->symInfo() && X->symInfo()->binding() == ResolveInfo::Local)
        return DynRelocType::DTPMOD_LOCAL;
      return DynRelocType::DTPMOD_GLOBAL;
    }
    if (X->type() == llvm::ELF::R_ARM_TLS_DTPOFF32) {
      if (X->symInfo() && X->symInfo()->binding() == ResolveInfo::Local)
        return DynRelocType::DTPREL_LOCAL;
      return DynRelocType::DTPREL_GLOBAL;
    }
    if (X->type() == llvm::ELF::R_ARM_TLS_TPOFF32) {
      if (X->symInfo() && X->symInfo()->binding() == ResolveInfo::Local)
        return DynRelocType::TPREL_LOCAL;
      return DynRelocType::TPREL_GLOBAL;
    }
    return DynRelocType::DEFAULT;
  }

  void setSBRELSegment(ELFSegment *E) { m_pSBRELSegment = E; }

  ELFSegment *getSBRELSegment() const { return m_pSBRELSegment; }

  virtual void finalizeBeforeWrite() override;

  void defineGOTSymbol(Fragment &F);

  void mayBeRelax(int pass, bool &pFinished) override;

  /// initTargetStubs
  bool initTargetStubs() override;

  /// getRelEntrySize - the size in BYTE of rel type relocation
  size_t getRelEntrySize() override { return 8; }

  /// getRelEntrySize - the size in BYTE of rela type relocation
  size_t getRelaEntrySize() override {
    assert(0 && "ARM backend with Rela type relocation\n");
    return 12;
  }

  /// doCreateProgramHdrs - backend can implement this function to create the
  /// target-dependent segments
  virtual void doCreateProgramHdrs() override;

  /// Sort ARM.EDIDX
  void sortEXIDX();

  void finishAssignOutputSections() override;

  bool updateTargetSections() override;

  virtual bool handleBSS(const ELFSection *prev,
                         const ELFSection *cur) const override;

  /// Add Target specific segments.
  void addTargetSpecificSegments() override;

  Stub *getBranchIslandStub(Relocation *pReloc,
                            int64_t targetValue) const override;

  bool canRewriteToBLX() const;

  bool isMicroController() const;

  bool isJ1J2BranchEncoding() const;

  bool canUseMovTMovW() const;

  void initializeAttributes() override;

  bool handleRelocation(ELFSection *Section, Relocation::Type Type,
                        LDSymbol &Sym, uint32_t Offset,
                        Relocation::Address Addend = 0,
                        bool LastVisit = false) override;

  std::size_t PLTEntriesCount() const override { return m_PLTMap.size(); }

  std::size_t GOTEntriesCount() const override { return m_GOTMap.size(); }

  void setDefaultConfigs() override;

private:
  void createAttributeSection(uint32_t Flag, uint32_t Align);

private:
  Relocator *m_pRelocator;

  ARMELFDynamic *m_pDynamic;
  LDSymbol *m_pEXIDXStart;
  LDSymbol *m_pEXIDXEnd;
  LDSymbol *m_pIRelativeStart;
  LDSymbol *m_pIRelativeEnd;

  //     variable name           :  ELF
  ELFSection *m_pEXIDX;              // .ARM.exidx
  ELFSection *m_pRegionTableSection; // RegionTable section.
  TargetFragment *m_pRegionTableFragment;
  LDSymbol *m_pRWPIBase;       // _RWPI_BASE__
  ELFSegment *m_pSBRELSegment; // SBREL segment.
  /// ARM Attribute Section
  ELFSection *m_pARMAttributeSection;
  /// ARM Attribute Fragment
  ARMAttributeFragment *AttributeFragment;
  bool m_bEmitRegionTable;
  llvm::DenseMap<ResolveInfo *, ARMGOT *> m_GOTMap;
  llvm::DenseMap<ResolveInfo *, ARMGOT *> m_GOTPLTMap;
  llvm::DenseMap<ResolveInfo *, ARMPLT *> m_PLTMap;
};
} // namespace eld

#endif
