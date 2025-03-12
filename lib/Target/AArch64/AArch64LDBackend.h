//===- AArch64LDBackend.h--------------------------------------------------===//
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

#ifndef TARGET_AARCH64_AARCH64LDBACKEND_H
#define TARGET_AARCH64_AARCH64LDBACKEND_H

#include "AArch64ELFDynamic.h"
#include "AArch64ErrataFactory.h"
#include "AArch64ErrataIslandFactory.h"
#include "AArch64GOT.h"
#include "AArch64NoteGNUPropertyFragment.h"
#include "AArch64PLT.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Target/GNULDBackend.h"
#include <vector>

namespace eld {

class LinkerConfig;
class TargetInfo;

//===----------------------------------------------------------------------===//
/// AArch64GNUInfoLDBackend - linker backend of AArch64 target of GNU ELF format
///
class AArch64GNUInfoLDBackend : public GNULDBackend {
public:
  AArch64GNUInfoLDBackend(Module &pModule, TargetInfo *pInfo);
  ~AArch64GNUInfoLDBackend();

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

  /// initBRIslandFactory - initialize the branch island factory for relaxation
  bool initBRIslandFactory() override;

  /// initStubFactory - initialize the stub factory for relaxation
  bool initStubFactory() override;

  /// doPreLayout - Backend can do any needed modification before layout
  void doPreLayout() override;

  /// dynamic - the dynamic section of the target machine.
  /// Use co-variant return type to return its own dynamic section.
  AArch64ELFDynamic *dynamic() override;

  /// getTargetSectionOrder - compute the layout order of AArch64 target
  /// sections
  unsigned int getTargetSectionOrder(const ELFSection &pSectHdr) const override;

  /// finalizeTargetSymbols - finalize the symbol value
  bool finalizeTargetSymbols() override;

  void setOptions() override;

  void initSegmentFromLinkerScript(ELFSegment *pSegment) override;

  ELFSection *getTData() const { return m_ptdata; }

  ELFSection *getTBSS() const { return m_ptbss; }

  void defineIRelativeRange(ResolveInfo &pSym);

  bool scanErrata843419();

  Relocation::Type getCopyRelType() const override;

  // ---  GOT Support ------
  AArch64GOT *createGOT(GOT::GOTType T, ELFObjectFile *Obj, ResolveInfo *sym,
                        bool SkipPLTRef = false);

  void recordGOT(ResolveInfo *, AArch64GOT *);

  void recordGOTPLT(ResolveInfo *, AArch64GOT *);

  AArch64GOT *findEntryInGOT(ResolveInfo *) const;

  // ---  PLT Support ------
  AArch64PLT *createPLT(ELFObjectFile *Obj, ResolveInfo *sym,
                        bool isIRelative = false);

  void recordPLT(ResolveInfo *, AArch64PLT *);

  AArch64PLT *findEntryInPLT(ResolveInfo *) const;

  bool hasSymInfo(const Relocation *X) const override {
    if (X->type() == llvm::ELF::R_AARCH64_IRELATIVE)
      return false;
    if (X->type() == llvm::ELF::R_AARCH64_RELATIVE)
      return false;
    if (X->symInfo() && X->symInfo()->binding() == ResolveInfo::Local)
      return false;
    return true;
  }

  DynRelocType getDynRelocType(const Relocation *X) const override {
    if (X->type() == llvm::ELF::R_AARCH64_GLOB_DAT)
      return DynRelocType::GLOB_DAT;
    if (X->type() == llvm::ELF::R_AARCH64_JUMP_SLOT)
      return DynRelocType::JMP_SLOT;
    if (X->type() == llvm::ELF::R_AARCH64_ABS64)
      return DynRelocType::WORD_DEPOSIT;
    if (X->type() == llvm::ELF::R_AARCH64_RELATIVE)
      return DynRelocType::RELATIVE;
    if (X->type() == llvm::ELF::R_AARCH64_IRELATIVE)
      return DynRelocType::RELATIVE;
    if (X->type() == llvm::ELF::R_AARCH64_TLSDESC) {
      if (X->symInfo() && X->symInfo()->binding() == ResolveInfo::Local)
        return DynRelocType::TLSDESC_LOCAL;
      return DynRelocType::TLSDESC_GLOBAL;
    }
    if (X->type() == llvm::ELF::R_AARCH64_TLS_TPREL64) {
      if (X->symInfo() && X->symInfo()->binding() == ResolveInfo::Local)
        return DynRelocType::TPREL_LOCAL;
      return DynRelocType::TPREL_GLOBAL;
    }
    return DynRelocType::DEFAULT;
  }

  bool finalizeScanRelocations() override;

  Stub *getBranchIslandStub(Relocation *pReloc,
                            int64_t targetValue) const override;

  bool readSection(InputFile &pInput, ELFSection *S) override;

  bool DoesOverrideMerge(ELFSection *pSection) const override;

  ELFSection *mergeSection(ELFSection *pSection) override;

  bool processInputFiles(std::vector<InputFile *> &Inputs) override;

  void initializeAttributes() override;

  std::size_t PLTEntriesCount() const override { return m_PLTMap.size(); }

  std::size_t GOTEntriesCount() const override { return m_GOTMap.size(); }

private:
  ELFSection *createGOTSection(InputFile &InputFile);
  ELFSection *createGOTPLTSection(InputFile &InputFile);
  ELFSection *createPLTSection(InputFile &InputFile);

  void defineGOTSymbol(Fragment &F);

  /// maxBranchOffset
  /// FIXME:
  uint64_t maxBranchOffset() override { return 0x0; }

  void mayBeRelax(int pass, bool &pFinished) override;

  /// getRelEntrySize - the size in BYTE of rel type relocation
  size_t getRelEntrySize() override { return 16; }

  /// getRelEntrySize - the size in BYTE of rela type relocation
  size_t getRelaEntrySize() override { return 24; }

  /// LTO Flow Setup
  bool ltoNeedAssembler() override;
  bool ltoCallExternalAssembler(const std::string &Input,
                                std::string RelocModel,
                                const std::string &Output) override;

  bool isErratum843419Sequence(uint32_t insn1, uint32_t insn2, uint32_t insn3);

  void createErratum843419Stub(Fragment *frag, uint32_t offset);

  // Read features from GNU property sections.
  template <class ELFT>
  bool readGNUProperty(InputFile &pInput, ELFSection *S, uint32_t &featureSet);

  bool processInputFile(InputFile *In);

  // Create GNU property section.
  void createGNUPropertySection(bool);

private:
  AArch64ErrataFactory *m_pErrata843419Factory;

  AArch64ErrataIslandFactory *m_pAArch64ErrataIslandFactory;

  Relocator *m_pRelocator;

  AArch64ELFDynamic *m_pDynamic;
  LDSymbol *m_pIRelativeStart;
  LDSymbol *m_pIRelativeEnd;
  ELFSection *m_ptdata;
  ELFSection *m_ptbss;
  /// GNU Property section
  ELFSection *m_pNoteGNUProperty = nullptr;
  /// GNU Property fragment
  AArch64NoteGNUPropertyFragment *m_pGPF = nullptr;
  llvm::DenseMap<ResolveInfo *, AArch64GOT *> m_GOTMap;
  llvm::DenseMap<ResolveInfo *, AArch64GOT *> m_GOTPLTMap;
  llvm::DenseMap<ResolveInfo *, AArch64PLT *> m_PLTMap;
  std::unordered_map<InputFile *, uint32_t> NoteGNUPropertyMap;
};
} // namespace eld

#endif
