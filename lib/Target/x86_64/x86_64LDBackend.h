//===- x86_64LDBackend.h---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef X86_64_LDBACKEND_H
#define X86_64_LDBACKEND_H

#include "eld/Config/LinkerConfig.h"
#include "eld/Object/ObjectBuilder.h"
#include "eld/Readers/ELFSection.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "eld/Target/GNULDBackend.h"
#include "x86_64ELFDynamic.h"
#include "x86_64PLT.h"

namespace eld {

class LinkerConfig;
class x86_64Info;

//===----------------------------------------------------------------------===//
/// x86_64LDBackend - linker backend of x86_64 target of GNU ELF format
///
class x86_64LDBackend : public GNULDBackend {
public:
  x86_64LDBackend(Module &pModule, x86_64Info *pInfo);

  ~x86_64LDBackend();

  void initializeAttributes() override;

  /// initRelocator - create and initialize Relocator.
  bool initRelocator() override;

  /// getRelocator - return relocator.
  Relocator *getRelocator() const override;

  void initTargetSections(ObjectBuilder &pBuilder) override;

  /// Create dynamic input sections in an input file.
  void initDynamicSections(ELFObjectFile &) override;

  void initTargetSymbols() override;

  bool initBRIslandFactory() override;

  bool initStubFactory() override;

  /// getTargetSectionOrder - compute the layout order of target section
  unsigned int getTargetSectionOrder(const ELFSection &pSectHdr) const override;

  /// finalizeTargetSymbols - finalize the symbol value
  bool finalizeTargetSymbols() override;

  uint64_t getValueForDiscardedRelocations(const Relocation *R) const override;

  x86_64ELFDynamic *dynamic() override;

  void doCreateProgramHdrs() override { return; }

  Stub *getBranchIslandStub(Relocation *pReloc,
                            int64_t pTargetValue) const override {
    return nullptr;
  }

  Relocation::Type getCopyRelType() const override;

  // ---  GOT Support ------
  x86_64GOT *createGOT(GOT::GOTType T, ELFObjectFile *Obj, ResolveInfo *sym);

  void recordGOT(ResolveInfo *, x86_64GOT *);

  void recordGOTPLT(ResolveInfo *, x86_64GOT *);

  x86_64GOT *findEntryInGOT(ResolveInfo *) const;

  // ---  PLT Support ------
  x86_64PLT *createPLT(ELFObjectFile *Obj, ResolveInfo *sym);

  void recordPLT(ResolveInfo *, x86_64PLT *);

  x86_64PLT *findEntryInPLT(ResolveInfo *) const;

  std::size_t PLTEntriesCount() const override { return m_PLTMap.size(); }

  std::size_t GOTEntriesCount() const override { return m_GOTMap.size(); }

  void setDefaultConfigs() override;

  void doPreLayout() override;

  DynRelocType getDynRelocType(const Relocation *X) const override {
    if (X->type() == llvm::ELF::R_X86_64_JUMP_SLOT)
      return DynRelocType::JMP_SLOT;
    return DynRelocType::DEFAULT;
  }

private:
  /// getRelEntrySize - the size in BYTE of rela type relocation
  size_t getRelEntrySize() override { return 0; }

  /// getRelaEntrySize - the size in BYTE of rela type relocation
  size_t getRelaEntrySize() override { return 24; }

  uint64_t maxBranchOffset() override { return 0; }

private:
  Relocator *m_pRelocator;

  x86_64ELFDynamic *m_pDynamic;
  LDSymbol *m_pEndOfImage;
  // Tracks .rela.plt entry index
  // m_RelaPLTIndex starts at 0 for the first function PLT entry
  uint32_t m_RelaPLTIndex = 0;

  llvm::DenseMap<ResolveInfo *, x86_64GOT *> m_GOTMap;
  llvm::DenseMap<ResolveInfo *, x86_64GOT *> m_GOTPLTMap;
  llvm::DenseMap<ResolveInfo *, x86_64PLT *> m_PLTMap;
};
} // namespace eld

#endif
