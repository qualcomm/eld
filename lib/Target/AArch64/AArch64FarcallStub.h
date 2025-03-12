//===- AArch64FarcallStub.h------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_AARCH64_FARCALL_STUB_H
#define ELD_AARCH64_FARCALL_STUB_H

#include "eld/Fragment/Stub.h"
#include "llvm/Support/DataTypes.h"
#include <string>
#include <vector>

namespace eld {

class Module;
class Relocation;
class ResolveInfo;

/** \class AArch64FarcallStub
 *  \brief AArch64 stub for far call from source to target
 *
 */
class AArch64FarcallStub : public Stub {
public:
  AArch64FarcallStub(bool pIsOutputPIC);

  ~AArch64FarcallStub();

  // observers
  const std::string &name() const override;

  const uint8_t *getContent() const override;

  size_t alignment() const override;

  AArch64FarcallStub(const AArch64FarcallStub &);

  AArch64FarcallStub &operator=(const AArch64FarcallStub &);

  /// for doClone
  AArch64FarcallStub(const uint32_t *pData, size_t pSize,
                     const_fixup_iterator pBegin, const_fixup_iterator pEnd,
                     size_t align);

  Stub *clone(InputFile *, Relocation *, eld::IRBuilder *,
              DiagnosticEngine *) override;

  bool isRelocInRange(const Relocation *pReloc, int64_t targetAddr,
                      int64_t &Offset, Module &M) const override;

  uint32_t getRealAddend(const Relocation &reloc,
                         DiagnosticEngine *) const override {
    return 0;
  }

private:
  std::string m_Name;
  static const uint32_t TEMPLATE[];
  static const uint32_t TEMPLATE_PIC[];
  const uint32_t *m_pData;
};

} // namespace eld

#endif
