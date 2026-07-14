//===- x86_64Relocator.h---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef X86_64_RELOCATION_FACTORY_H
#define X86_64_RELOCATION_FACTORY_H

#include "eld/Target/Relocator.h"
#include "x86_64LDBackend.h"
#include <memory>
#include <mutex>

#define POSITION_OF_PACKET_BITS 14
#define MASK_END_PACKET (3 << POSITION_OF_PACKET_BITS)
#define END_OF_PACKET (3 << POSITION_OF_PACKET_BITS)
#define END_OF_DUPLEX (0 << POSITION_OF_PACKET_BITS)

namespace eld {

class ResolveInfo;
class LinkerConfig;

// Forward declaration — defined in x86_64Relocator.cpp (pimpl).
struct ScanFlagMap;

/** \class x86_64Relocator
 *  \brief x86_64Relocator creates and destroys the x86_64 relocations.
 *
 */
class x86_64Relocator : public Relocator {
public:
  x86_64Relocator(x86_64LDBackend &pParent, LinkerConfig &pConfig,
                  Module &pModule);
  ~x86_64Relocator(); // out-of-line: ScanFlagMap complete type needed for dtor

  Result applyRelocation(Relocation &pRelocation) override;

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

  // Phase 0: populate the per-global scan-flag map. Must run single-threaded
  // after symbol resolution (getGlobals() complete) and before the parallel
  // scan. Called from x86_64LDBackend::initScanRelocations().
  void initScanRelocations();

  // Phase 2: allocate GOT/PLT/TLS entries for all symbols flagged during the
  // parallel scan. Called from x86_64LDBackend::finalizeScanRelocations()
  // after Pool->wait(). Does NOT merge relocations (handled by ObjectLinker).
  void allocateDynEntries();

private:
  void scanOneReloc(InputFile &pInput, Relocation &pReloc,
                    eld::IRBuilder &pBuilder, ELFSection &pSection,
                    CopyRelocs &, bool isLocal);

  bool isRelocSupported(const Relocation &pReloc) const;

  x86_64GOT *getTLSModuleID(ResolveInfo *R, bool isStatic = false);

  x86_64LDBackend &m_Target;
  std::unique_ptr<ScanFlagMap> m_ScanFlags;
  x86_64GOT *m_TLSModuleIDGOT = nullptr;
};

} // namespace eld

#endif
