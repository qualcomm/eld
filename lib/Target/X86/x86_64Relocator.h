//===- x86_64Relocator.h---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef X86_64_RELOCATION_FACTORY_H
#define X86_64_RELOCATION_FACTORY_H

#include "eld/Target/Relocator.h"
#include "x86_64LDBackend.h"
#include <mutex>

#define POSITION_OF_PACKET_BITS 14
#define MASK_END_PACKET (3 << POSITION_OF_PACKET_BITS)
#define END_OF_PACKET (3 << POSITION_OF_PACKET_BITS)
#define END_OF_DUPLEX (0 << POSITION_OF_PACKET_BITS)

namespace eld {

class ResolveInfo;
class LinkerConfig;

/** \class x86_64Relocator
 *  \brief x86_64Relocator creates and destroys the x86_64 relocations.
 *
 */
class x86_64Relocator : public Relocator {
public:
  x86_64Relocator(x86_64LDBackend &pParent, LinkerConfig &pConfig,
                  Module &pModule);
  ~x86_64Relocator();

  Result applyRelocation(Relocation &pRelocation) override;

  /// scanRelocation - determine the empty entries are needed or not and create
  /// the empty entries if needed.
  /// For x86_64, following entries are check to create:
  /// - GOT entry (for .got and .got.plt sections)
  /// - PLT entry (for .plt section)
  /// - dynamin relocation entries (for .rel.plt and .rel.dyn sections)
  void scanRelocation(Relocation &pReloc, eld::IRBuilder &pBuilder,
                      ELFSection &pSection, InputFile &pInput,
                      CopyRelocs &) override;

  // Handle partial linking
  void partialScanRelocation(Relocation &pReloc,
                             const ELFSection &pSection) override;

  x86_64LDBackend &getTarget() override { return m_Target; }

  const x86_64LDBackend &getTarget() const override { return m_Target; }

  const char *getName(Relocation::Type pType) const override;

  Size getSize(Relocation::Type pType) const override;

  uint32_t getNumRelocs() const override;

  void computeTLSOffsets() override;

protected:
  void defineSymbolforGuard(eld::IRBuilder &pLinker, ResolveInfo *pSym,
                            x86_64LDBackend &pTarget);

private:
  virtual void scanLocalReloc(InputFile &pInput, Relocation &pReloc,
                              eld::IRBuilder &pBuilder, ELFSection &pSection);

  virtual void scanGlobalReloc(InputFile &pInput, Relocation &pReloc,
                               eld::IRBuilder &pBuilder, ELFSection &pSection,
                               CopyRelocs &);

  bool isInvalidReloc(Relocation &pReloc) const;

  x86_64GOT *getTLSModuleID(ResolveInfo *R, bool isStatic = false);

  x86_64LDBackend &m_Target;
};

} // namespace eld

#endif
