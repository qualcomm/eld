//===- ARMRelocator.h------------------------------------------------------===//
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
#ifndef TARGET_ARM_ARMRELOCATOR_H
#define TARGET_ARM_ARMRELOCATOR_H

#include "ARMLDBackend.h"
#include "eld/Target/Relocator.h"
#include <mutex>

namespace eld {

/** \class ARMRelocator
 *  \brief ARMRelocator creates and destroys the ARM relocations.
 *
 */
class ARMRelocator : public Relocator {
public:
  ARMRelocator(ARMGNULDBackend &pParent, LinkerConfig &pConfig,
               Module &pModule);
  ~ARMRelocator();

  Result applyRelocation(Relocation &pRelocation) override;

  ARMGNULDBackend &getTarget() override { return m_Target; }

  const ARMGNULDBackend &getTarget() const override { return m_Target; }

  const char *getName(Relocation::Type pType) const override;

  Size getSize(Relocation::Type pType) const override;

  uint32_t getAddend(const Relocation *R) const override {
    if (R->symInfo()->isSection())
      return R->target();
    return 0;
  }

  void adjustAddend(Relocation *R) const override {
    if (R->symInfo()->isSection())
      R->setTargetData(0);
  }

  /// scanRelocation - determine the empty entries are needed or not and create
  /// the empty entries if needed.
  /// For ARM, following entries are check to create:
  /// - GOT entry (for .got section)
  /// - PLT entry (for .plt section)
  /// - dynamin relocation entries (for .rel.plt and .rel.dyn sections)
  void scanRelocation(Relocation &pReloc, eld::IRBuilder &pBuilder,
                      ELFSection &pSection, InputFile &pInputFile,
                      CopyRelocs &) override;

  ELFSegment *getSBRELSegment() const { return m_Target.getSBRELSegment(); }

  void setSBRELSegment(ELFSegment *S) { m_Target.setSBRELSegment(S); }

  uint32_t getNumRelocs() const override;

private:
  bool isInvalidReloc(Relocation &pType) const;
  void scanLocalReloc(InputFile &pInput, Relocation::Type, Relocation &pReloc,
                      const ELFSection &pSection);

  void scanGlobalReloc(InputFile &pInput, Relocation::Type, Relocation &pReloc,
                       eld::IRBuilder &pBuilder, ELFSection &pSection,
                       CopyRelocs &);

  bool checkValidReloc(Relocation &pReloc) const;

  uint32_t relocType() const override { return llvm::ELF::SHT_REL; }

  ARMGOT *getTLSModuleID(ResolveInfo *R, bool isStatic = false);

private:
  ARMGNULDBackend &m_Target;
};

} // namespace eld

#endif
