//===- HexagonAbsoluteStub.h-----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//

#ifndef ELD_HEXAGON_ABSOLUTE_STUB_H
#define ELD_HEXAGON_ABSOLUTE_STUB_H

#include "eld/Fragment/Stub.h"
#include "llvm/Support/DataTypes.h"
#include <mutex>
#include <string>
#include <vector>

namespace eld {

class Relocation;
class ResolveInfo;

/** \class HexagonAbsoluteStub
 *  \brief Hexagon stub for abs long call from source to target
 *
 */
class HexagonAbsoluteStub : public Stub {
public:
  HexagonAbsoluteStub(bool pIsOutputPIC);

  ~HexagonAbsoluteStub();

  // observers
  const std::string &name() const override;

  const uint8_t *getContent() const override;

  size_t alignment() const override;

  HexagonAbsoluteStub(const HexagonAbsoluteStub &);

  HexagonAbsoluteStub &operator=(const HexagonAbsoluteStub &);

  /// for doClone
  HexagonAbsoluteStub(const uint8_t *pData, size_t pSize,
                      const_fixup_iterator pBegin, const_fixup_iterator pEnd,
                      size_t align);

  HexagonAbsoluteStub(const uint8_t *pData, size_t pSize, uint32_t align);

  virtual Stub *clone(InputFile *, Relocation *, eld::IRBuilder *,
                      DiagnosticEngine *) override;

  virtual Stub *clone(InputFile *, Relocation *, Fragment *frag,
                      eld::IRBuilder *, DiagnosticEngine *DiagEngine) override;

  virtual bool isRelocInRange(const Relocation *pReloc, int64_t targetAddr,
                              int64_t &Offset, Module &Module) const override;

  virtual uint32_t getRealAddend(const Relocation &,
                                 DiagnosticEngine *) const override;

  bool isCompatible(Stub *S) const override { return true; }

private:
  std::string m_Name;
  static const uint32_t TEMPLATE[];
  static const uint32_t TEMPLATE_PIC[];
  const uint8_t *m_pData;
  std::mutex Mutex;
};

} // namespace eld

#endif
