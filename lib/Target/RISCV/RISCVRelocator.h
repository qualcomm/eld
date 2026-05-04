//===- RISCVRelocator.h----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef RISCV_RELOCATION_FACTORY_H
#define RISCV_RELOCATION_FACTORY_H

#include "RISCVLDBackend.h"
#include "RISCVRelocationInternal.h"
#include "eld/Target/Relocator.h"
#include <mutex>

namespace eld {

class ResolveInfo;
class LinkerConfig;

/** \class RISCVRelocator
 *  \brief RISCVRelocator creates and destroys the RISCV relocations.
 *
 */
class RISCVRelocator : public Relocator {
public:
  RISCVRelocator(RISCVLDBackend &pParent, LinkerConfig &pConfig,
                 Module &pModule);

  Result applyRelocation(Relocation &pRelocation) override;

  void scanRelocation(Relocation &pReloc, eld::IRBuilder &pBuilder,
                      ELFSection &pSection, InputFile &pInput,
                      CopyRelocs &) override;

  // Handle partial linking
  void partialScanRelocation(Relocation &pReloc,
                             const ELFSection &pSection) override;

  RISCVLDBackend &getTarget() override;

  const RISCVLDBackend &getTarget() const override;

  const char *getName(Relocation::Type pType) const override;

  uint32_t getNumRelocs() const override;

  Size getSize(Relocation::Type pType) const override;

  bool is32bit() const { return config().targets().is32Bits(); }

private:
  virtual void scanLocalReloc(InputFile &pInput, Relocation &pReloc,
                              eld::IRBuilder &pBuilder, ELFSection &pSection);

  virtual void scanGlobalReloc(InputFile &pInput, Relocation &pReloc,
                               eld::IRBuilder &pBuilder, ELFSection &pSection,
                               CopyRelocs &);

  RISCVGOT *getTLSModuleID(ResolveInfo *R, bool isStatic);

  bool isInvalidReloc(Relocation &pReloc) const;

  bool isRelocSupported(Relocation &pReloc) const;

  RISCVGOT *getTLSModuleID(ResolveInfo *rsym);

  void handleScanForNonPreemptibleIFunc(Relocation &R, ELFObjectFile *Obj);

  /// Returns true if the relocation computation uses GOT entry of the symbol.
  bool isGOTBasedInstrRelocation(Relocation::Type relocType) const;

  /// Returns true if the relocation is an absolute relocation designed for
  /// data section of the program.
  bool isAbsDataRelocation(Relocation::Type relocType) const;

  /// Returns true if the relocation is used to compute an absolute
  /// or PC-relative address.
  ///
  /// This function returns false for PCREL_LO12_I and PCREL_LO12_S because
  /// these relocations may be used to construct a GOT address depending upon
  /// the corresponding Hi-relocation.
  bool isAbsOrPCRELAddressInstrRelocation(Relocation::Type relocType) const;

  bool isControlFlowRelocation(Relocation::Type relocType) const;

public:
  RISCVLDBackend &m_Target;

private:
  std::mutex m_RelocMutex;
};

} // namespace eld

#endif
