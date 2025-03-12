//===- THMToARMStub.h------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#ifndef TARGET_ARM_THMTOARMSTUB_H
#define TARGET_ARM_THMTOARMSTUB_H

#include "eld/Fragment/Stub.h"
#include "llvm/Support/DataTypes.h"
#include <string>
#include <vector>

namespace eld {

class ARMGNULDBackend;
class DiagnosticEngine;
class Relocation;
class ResolveInfo;

/** \class THMToARMStub
 *  \brief ARM stub for long call from ARM source to ARM target
 *
 */
class THMToARMStub : public Stub {
public:
  THMToARMStub(uint32_t type, ARMGNULDBackend *Target);

  ~THMToARMStub();

  // observers
  const std::string &name() const override;

  const uint8_t *getContent() const override;

  size_t alignment() const override;

  // for T bit of this stub
  uint64_t initSymValue() const override;

  THMToARMStub(const THMToARMStub &);

  THMToARMStub &operator=(const THMToARMStub &);

  /// for doClone
  THMToARMStub(const uint32_t *pData, size_t pSize, const_fixup_iterator pBegin,
               const_fixup_iterator pEnd, size_t align, uint32_t pNumStub);

  /// doClone
  Stub *clone(InputFile *, Relocation *, eld::IRBuilder *,
              DiagnosticEngine *DiagEngine) override;

  // Check if this is valid stub for state of execution and if the
  // range is beyond branch range.
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
  const uint32_t *m_pData;
  static ARMGNULDBackend *m_Target;
  uint32_t m_NumStub;
  uint32_t m_Type;
};

} // namespace eld

#endif
