//===- TemplateLDBackend.h-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
#ifndef TEMPLATE_LDBACKEND_H
#define TEMPLATE_LDBACKEND_H

#include "eld/Config/LinkerConfig.h"
#include "eld/Input/ELFSection.h"
#include "eld/Object/ObjectBuilder.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "eld/Target/GNULDBackend.h"

namespace eld {

class LinkerConfig;
class TemplateInfo;

//===----------------------------------------------------------------------===//
/// TemplateLDBackend - linker backend of Template target of GNU ELF format
///
class TemplateLDBackend : public GNULDBackend {
public:
  TemplateLDBackend(Module &pModule, TemplateInfo *pInfo);

  ~TemplateLDBackend();

  void initializeAttributes() override;

  /// initRelocator - create and initialize Relocator.
  bool initRelocator() override;

  /// getRelocator - return relocator.
  Relocator *getRelocator() const override;

  void initTargetSections(ObjectBuilder &pBuilder) override;

  void initTargetSymbols() override;

  bool initBRIslandFactory() override;

  bool initStubFactory() override;

  /// getTargetSectionOrder - compute the layout order of target section
  unsigned int getTargetSectionOrder(const ELFSection &pSectHdr) const override;

  /// finalizeTargetSymbols - finalize the symbol value
  bool finalizeTargetSymbols() override;

  uint64_t getValueForDiscardedRelocations(const Relocation *R) const override;

  ELFDynamic *dynamic() override { return nullptr; }

  void doCreateProgramHdrs() override { return; }

private:
  /// getRelEntrySize - the size in BYTE of rela type relocation
  size_t getRelEntrySize() override { return 0; }

  /// getRelaEntrySize - the size in BYTE of rela type relocation
  size_t getRelaEntrySize() override { return 12; }

  uint64_t maxBranchOffset() override { return 0; }

private:
  Relocator *m_pRelocator;
  llvm::BumpPtrAllocator _alloc;

  LDSymbol *m_pEndOfImage;
};
} // namespace eld

#endif
