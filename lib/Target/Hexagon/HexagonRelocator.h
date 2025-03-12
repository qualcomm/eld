//===- HexagonRelocator.h--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef HEXAGON_RELOCATION_FACTORY_H
#define HEXAGON_RELOCATION_FACTORY_H

#include "HexagonLDBackend.h"
#include "eld/Target/Relocator.h"
#include <mutex>

#define POSITION_OF_PACKET_BITS 14
#define MASK_END_PACKET (3 << POSITION_OF_PACKET_BITS)
#define END_OF_PACKET (3 << POSITION_OF_PACKET_BITS)
#define END_OF_DUPLEX (0 << POSITION_OF_PACKET_BITS)

namespace eld {

class ResolveInfo;
class LinkerConfig;

/** \class HexagonRelocator
 *  \brief HexagonRelocator creates and destroys the Hexagon relocations.
 *
 */
class HexagonRelocator : public Relocator {
public:
  HexagonRelocator(HexagonLDBackend &pParent, LinkerConfig &pConfig,
                   Module &pModule);
  ~HexagonRelocator();

  Result applyRelocation(Relocation &pRelocation) override;

  /// scanRelocation - determine the empty entries are needed or not and create
  /// the empty entries if needed.
  /// For Hexagon, following entries are check to create:
  /// - GOT entry (for .got and .got.plt sections)
  /// - PLT entry (for .plt section)
  /// - dynamin relocation entries (for .rel.plt and .rel.dyn sections)
  void scanRelocation(Relocation &pReloc, eld::IRBuilder &pBuilder,
                      ELFSection &pSection, InputFile &pInput,
                      CopyRelocs &) override;

  uint32_t getAddend(const Relocation *R) const override {
    return R->addend() - m_Target.getPacketOffset(*R);
  }

  /// Merge string relocations are modified to point directly to the string so
  /// the addend needs to be adjusted. For PCREL relocations this sets the
  /// addend to the packet offset. For all other relocations the addend is zero.
  void adjustAddend(Relocation *R) const override {
    R->setAddend(m_Target.getPacketOffset(*R));
  }

  // Handle partial linking
  void partialScanRelocation(Relocation &pReloc,
                             const ELFSection &pSection) override;

  HexagonLDBackend &getTarget() override { return m_Target; }

  const HexagonLDBackend &getTarget() const override { return m_Target; }

  const char *getName(Relocation::Type pType) const override;

  Size getSize(Relocation::Type pType) const override;

  HexagonGOT *getTLSModuleID(ResolveInfo *rsym);

  uint32_t getNumRelocs() const override;

protected:
  void defineSymbolforGuard(eld::IRBuilder &pLinker, ResolveInfo *pSym,
                            HexagonLDBackend &pTarget);

private:
  virtual void scanLocalReloc(InputFile &pInput, Relocation &pReloc,
                              eld::IRBuilder &pBuilder, ELFSection &pSection);

  virtual void scanGlobalReloc(InputFile &pInput, Relocation &pReloc,
                               eld::IRBuilder &pBuilder, ELFSection &pSection,
                               CopyRelocs &);

  void CreateGOTAbsolute(ELFObjectFile *Obj, const Relocation &pReloc);
  void CreateGOTGD(ELFObjectFile *Obj, const Relocation &pReloc, bool Global);
  void CreateGOTIE(ELFObjectFile *Obj, const Relocation &pReloc);
  void CreatePLT(ELFObjectFile *Obj, ResolveInfo *pInfo);
  void CreateTLSPLT(ELFObjectFile *Obj, Relocation &pReloc,
                    HexagonTLSStub::StubType StaticStub,
                    HexagonTLSStub::StubType DynStub);

  bool isRelocSupported(Relocation &pReloc) const;

  bool isInvalidReloc(Relocation &pReloc) const;

  HexagonLDBackend &m_Target;
  LDSymbol *m_Guard;
};

} // namespace eld

#endif
