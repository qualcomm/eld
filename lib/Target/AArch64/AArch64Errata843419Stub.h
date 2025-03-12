//===- AArch64Errata843419Stub.h-------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef TARGET_AARCH64_AARCH64ERRATA843419STUB
#define TARGET_AARCH64_AARCH64ERRATA843419STUB

#include "eld/Fragment/Stub.h"
#include "llvm/Support/DataTypes.h"
#include <string>
#include <vector>

namespace eld {

class Module;
class BranchIsland;
class FragmentRef;
class IRBuilder;

class AArch64Errata843419Stub : public Stub {
public:
  AArch64Errata843419Stub();

  AArch64Errata843419Stub(const uint32_t *pData, size_t pSize,
                          const_fixup_iterator pBegin,
                          const_fixup_iterator pEnd, size_t align);

  virtual ~AArch64Errata843419Stub() {}

  virtual Stub *clone(InputFile *, Relocation *, eld::IRBuilder *,
                      DiagnosticEngine *) override;

  virtual bool isRelocInRange(const Relocation *pReloc, int64_t fragAddr,
                              int64_t &Offset, Module &Module) const override;

  virtual const std::string &name() const override;

  virtual const uint32_t *getData() const { return m_pData; }

  virtual const uint8_t *getContent() const override;

  virtual size_t size() const override;

  virtual size_t alignment() const override;

private:
  static const uint32_t TEMPLATE[];

private:
  std::string m_Name;
  const uint32_t *m_pData;
  size_t m_Size;
};

} // namespace eld

#endif
