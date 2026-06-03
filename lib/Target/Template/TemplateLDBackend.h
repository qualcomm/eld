//===- TemplateLDBackend.h-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
// Refer to eld/Target/GNULDBackend.h for additional hooks.
//===----------------------------------------------------------------------===//
#ifndef TEMPLATE_LDBACKEND_H
#define TEMPLATE_LDBACKEND_H

#include "TemplateELFDynamic.h"
#include "TemplateGOT.h"
#include "TemplatePLT.h"
#include "eld/Target/GNULDBackend.h"

namespace eld {

class LinkerConfig;
class TemplateInfo;
class TemplateELFDynamic;

//===----------------------------------------------------------------------===//
/// TemplateLDBackend - linker backend of Template target of GNU ELF format
///
class TemplateLDBackend : public GNULDBackend {
public:
  TemplateLDBackend(Module &pModule, TemplateInfo *pInfo);

  ~TemplateLDBackend();

  /// finalizeTargetSymbols - finalize the symbol value
  bool finalizeTargetSymbols() override;

  /// getRelocator - return relocator.
  Relocator *getRelocator() const override;

  /// getTargetSectionOrder - compute the layout order of target section
  unsigned int getTargetSectionOrder(const ELFSection &pSectHdr) const override;

  /// initRelocator - create and initialize Relocator.
  bool initRelocator() override;

  void initTargetSections(ObjectBuilder &pBuilder) override;

  void initTargetSymbols() override;

  void initializeAttributes() override;

  ELFDynamic *dynamic() override { return m_pDynamic; }

  // ---  GOT Support ------
  TemplateGOT *createGOT(GOT::GOTType T, ELFObjectFile *Obj, ResolveInfo *sym);

  void recordGOT(ResolveInfo *, TemplateGOT *);

  void recordGOTPLT(ResolveInfo *, TemplateGOT *);

  // ---------------------  PLT Support ---------------------------
  TemplatePLT *createPLT(ELFObjectFile *Obj, ResolveInfo *sym);

  void recordPLT(ResolveInfo *, TemplatePLT *);

  TemplatePLT *findEntryInPLT(ResolveInfo *) const;

  TemplateGOT *findEntryInGOT(ResolveInfo *) const;

  // ---------------------  Dynamic relocation support ------------

private:
  /// getRelEntrySize - the size in BYTE of rela type relocation
  size_t getRelEntrySize() override { return 0; }

  /// getRelaEntrySize - the size in BYTE of rela type relocation
  size_t getRelaEntrySize() override { return 0; }

  uint64_t maxBranchOffset() override { return 0; }

public:
  Stub *getBranchIslandStub(Relocation *pReloc,
                            int64_t pTargetValue) const override;

  std::size_t PLTEntriesCount() const override { return m_PLTMap.size(); }

  std::size_t GOTEntriesCount() const override { return m_GOTMap.size(); }

private:
  Relocator *m_pRelocator = nullptr;
  TemplateELFDynamic *m_pDynamic = nullptr;

  llvm::DenseMap<ResolveInfo *, TemplateGOT *> m_GOTMap;
  llvm::DenseMap<ResolveInfo *, TemplateGOT *> m_GOTPLTMap;
  llvm::DenseMap<ResolveInfo *, TemplatePLT *> m_PLTMap;
};
} // namespace eld

#endif
