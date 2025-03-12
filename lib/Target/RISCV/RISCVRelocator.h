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
  ~RISCVRelocator();

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

  Relocation::Address getSymbolValuePLT(Relocation &R);

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

public:
  RISCVLDBackend &m_Target;

private:
  std::mutex m_RelocMutex;
};

} // namespace eld

#endif
