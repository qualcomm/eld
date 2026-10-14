//===- X86_32LDBackend.h-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
// Refer to eld/Target/GNULDBackend.h for additional hooks.
//===----------------------------------------------------------------------===//

#ifndef ELD_TARGET_X86_32_LD_BACKEND_H
#define ELD_TARGET_X86_32_LD_BACKEND_H

#include "eld/Target/GNULDBackend.h"

namespace eld {

class X86_32Info;
class X86_32GOTPLT0;

class X86_32LDBackend : public GNULDBackend {
public:
  X86_32LDBackend(Module &pModule, X86_32Info *pInfo);
  ~X86_32LDBackend() override;

  bool finalizeTargetSymbols() override;
  Relocator *getRelocator() const override;
  bool initRelocator() override;
  void initTargetSections(ObjectBuilder &pBuilder) override;
  void initDynamicSections(InputFile &pInputFile) override;
  void initTargetSymbols() override;
  void initializeAttributes() override;

  // Ensure that the i386 GOTPLT0 header exists when GOT-base relocations are
  // used, even if no imported symbols need GOT slots.
  void ensureGOTPLT0();
  void ensureGOTBaseRelocations();

  std::size_t PLTEntriesCount() const override { return 0; }
  std::size_t GOTEntriesCount() const override { return 0; }

  Stub *getBranchIslandStub(Relocation *pReloc,
                            int64_t pTargetValue) const override {
    return nullptr;
  }

private:
  bool defineGOTSymbol();

  size_t getRelEntrySize() override { return 8; }
  size_t getRelaEntrySize() override { return 0; }

  Relocator *m_pRelocator = nullptr;
  X86_32GOTPLT0 *m_pGOTPLT0 = nullptr;
  bool m_HasGOTBaseRelocations = false;
};

GNULDBackend *createX86_32LDBackend(Module &pModule);

} // namespace eld

#endif
