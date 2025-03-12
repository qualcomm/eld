//===- AArch64Relocator.h--------------------------------------------------===//
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

//===-  AArch64Relocator.h ------------------------------------------------===//
//===----------------------------------------------------------------------===//
#ifndef TARGET_AARCH64_AARCH64RELOCATOR_H
#define TARGET_AARCH64_AARCH64RELOCATOR_H

#include "AArch64LDBackend.h"
#include "eld/Fragment/GOT.h"
#include "eld/Target/Relocator.h"
#include <mutex>

namespace eld {
// FIXME: llvm::ELF doesn't define AArch64 dynamic relocation types
enum {
  // static relocations
  R_AARCH64_ADR_PREL_PG_HI21_NC = 0x114,
  // dyanmic rlocations
  R_AARCH64_COPY = 1024,
  R_AARCH64_GLOB_DAT = 1025,
  R_AARCH64_JUMP_SLOT = 1026,
  R_AARCH64_RELATIVE = 1027,
  R_AARCH64_TLS_DTPREL64 = 1028,
  R_AARCH64_TLS_DTPMOD64 = 1029,
  R_AARCH64_TLS_TPREL64 = 1030,
  R_AARCH64_TLSDESC = 1031,
  R_AARCH64_IRELATIVE = 1032,
  R_AARCH64_COPY_INSN = 1033
};

/** \class AArch64Relocator
 *  \brief AArch64Relocator creates and destroys the AArch64 relocations.
 *
 */
class AArch64Relocator : public Relocator {
public:
  AArch64Relocator(AArch64GNUInfoLDBackend &pParent, LinkerConfig &pConfig,
                   Module &pModule);
  ~AArch64Relocator();

  Result applyRelocation(Relocation &pRelocation) override;

  AArch64GNUInfoLDBackend &getTarget() override { return m_Target; }

  const AArch64GNUInfoLDBackend &getTarget() const override { return m_Target; }

  const char *getName(Relocation::Type pType) const override;

  uint32_t getNumRelocs() const override;

  Size getSize(Relocation::Type pType) const override;

  /// scanRelocation - determine the empty entries are needed or not and create
  /// the empty entries if needed.
  /// For AArch64, following entries are check to create:
  /// - GOT entry (for .got section)
  /// - PLT entry (for .plt section)
  /// - dynamic relocation entries (for .rel.plt and .rel.dyn sections)
  void scanRelocation(Relocation &pReloc, eld::IRBuilder &pBuilder,
                      ELFSection &pSection, InputFile &pInput,
                      CopyRelocs &) override;

  // Handle partial linking
  void partialScanRelocation(Relocation &pReloc,
                             const ELFSection &pSection) override;

private:
  bool isInvalidReloc(Relocation &pType) const;
  void scanLocalReloc(InputFile &pInput, Relocation &pReloc,
                      const ELFSection &pSection);

  void scanGlobalReloc(InputFile &pInput, Relocation &pReloc,
                       eld::IRBuilder &pBuilder, ELFSection &pSection,
                       CopyRelocs &);

private:
  AArch64GNUInfoLDBackend &m_Target;
};

} // namespace eld

#endif
