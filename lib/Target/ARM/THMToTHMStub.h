//===- THMToTHMStub.h------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#ifndef TARGET_ARM_THMTOTHMSTUB_H
#define TARGET_ARM_THMTOTHMSTUB_H

#include "eld/Fragment/Stub.h"
#include "llvm/Support/DataTypes.h"
#include <string>
#include <vector>

namespace eld {

class DiagnosticEngine;
class ARMGNULDBackend;
class Relocation;
class ResolveInfo;
class Module;

/** \class THMToTHMStub
 *  \brief ARM stub for long call from ARM source to ARM target
 *
 */
class THMToTHMStub : public Stub {
public:
  THMToTHMStub(uint32_t type, ARMGNULDBackend *Target);

  ~THMToTHMStub();

  // observers
  const std::string &name() const override;

  const uint8_t *getContent() const override;

  size_t alignment() const override;

  // for T bit of this stub
  uint64_t initSymValue() const override;

  THMToTHMStub(const THMToTHMStub &);

  THMToTHMStub &operator=(const THMToTHMStub &);

  /// for doClone
  THMToTHMStub(const uint32_t *pData, size_t pSize, const_fixup_iterator pBegin,
               const_fixup_iterator pEnd, size_t align, uint32_t pNumSub);

  /// doClone
  Stub *clone(InputFile *, Relocation *, eld::IRBuilder *,
              DiagnosticEngine *) override;

  bool isNeeded(const Relocation *pReloc, int64_t targetValue,
                Module &Module) const override;

  bool isRelocInRange(const Relocation *pReloc, int64_t pTargetAddr,
                      int64_t &Offset, Module &Module) const override;

  bool supportsPIC() const override { return true; }

  std::string getStubName(const Relocation &pReloc, bool isClone,
                          bool isSectionRelative, int64_t numBranchIsland,
                          int64_t numClone, uint32_t relocAddend,
                          bool useOldStyleTrampolineName) const override;

  bool isCompatible(Stub *S) const override { return S->name() == m_Name; }

private:
  std::string m_Name;
  static const uint32_t PIC_TEMPLATE[];
  static const uint32_t TEMPLATE[];
  static const uint32_t TEMPLATE_MOV[];
  static const uint32_t TEMPLATE_THUMB1[];
  const uint32_t *m_pData;
  static ARMGNULDBackend *m_Target;
  uint32_t m_NumStub;
  uint32_t m_Type;
};

} // namespace eld

#endif
