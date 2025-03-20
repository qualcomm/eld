//===- THMToARMStub.cpp----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#include "THMToARMStub.h"
#include "ARMLDBackend.h"
#include "ARMRelocator.h"
#include "eld/Config/GeneralOptions.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Readers/Relocation.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "llvm/BinaryFormat/ELF.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// THMToARMStub
//===----------------------------------------------------------------------===//
const uint32_t THMToARMStub::PIC_TEMPLATE[] = {
    0x46c04778, // bx    pc ... nop
    0xe59fc000, // ldr   ip, [pc, #0]
    0xe08cf00f, // add   pc, ip, pc
    0x0         // dcd   R_ARM_REL32(X)
};

const uint32_t THMToARMStub::TEMPLATE[] = {
    0x46c04778, // bx    pc ... nop
    0xe51ff004, // ldr   pc, [pc, #-4]
    0x0         // dcd   R_ARM_ABS32(X)
};

const uint32_t THMToARMStub::TEMPLATE_MOV[] = {
    0x0c00f240, // movw  ip, R_ARM_THM_MOVW_ABS_NC(X)
    0x0c00f2c0, // movt  ip, R_ARM_THM_MOVT_ABS(X)
    0x46c04760  // bx    ip ... nop
};

ARMGNULDBackend *THMToARMStub::m_Target = nullptr;

THMToARMStub::THMToARMStub(uint32_t type, ARMGNULDBackend *Target)
    : Stub(), m_Name("T2A_veneer"), m_pData(nullptr), m_NumStub(0),
      m_Type(type) {
  if (type == ARMGNULDBackend::VENEER_PIC) {
    m_pData = PIC_TEMPLATE;
    ThisSize = sizeof(PIC_TEMPLATE);
    addFixup(12u, -4, llvm::ELF::R_ARM_REL32);
  } else if (type == ARMGNULDBackend::VENEER_MOV) {
    m_pData = TEMPLATE_MOV;
    ThisSize = sizeof(TEMPLATE_MOV);
    addFixup(0u, 0x0, llvm::ELF::R_ARM_THM_MOVW_ABS_NC);
    addFixup(4u, 0x0, llvm::ELF::R_ARM_THM_MOVT_ABS);
  } else {
    m_pData = TEMPLATE;
    ThisSize = sizeof(TEMPLATE);
    addFixup(8u, 0x0, llvm::ELF::R_ARM_ABS32);
  }
  Alignment = alignment();
  m_Target = Target;
}

/// for doClone
THMToARMStub::THMToARMStub(const uint32_t *pData, size_t pSize,
                           const_fixup_iterator pBegin,
                           const_fixup_iterator pEnd, size_t align,
                           uint32_t pNumStub)
    : Stub(), m_Name("T2A_veneer"), m_pData(pData), m_NumStub(pNumStub) {
  ThisSize = pSize;
  Alignment = align;
  for (auto it = pBegin, ie = pEnd; it != ie; ++it)
    addFixup(**it);
}

THMToARMStub::~THMToARMStub() {}

// Find out if this stub is the appropriate
// stub for the StubFactory to use when creating
// a branch island.
bool THMToARMStub::isNeeded(const Relocation *pReloc, int64_t pTargetValue,
                            Module &Module) const {
  int64_t Offset = 0;
  // Microcontrollers cannot branch to ARM.
  if (pReloc && m_Target->isMicroController())
    return false;
  if ((pTargetValue & 0x1) != 0 &&
      !(pReloc->symInfo()->reserved() & Relocator::ReservePLT))
    return false;
  // always need a stub to switch mode for JUMp24
  if (pReloc->type() == llvm::ELF::R_ARM_THM_JUMP24) {
    return true;
  }
  // Other relocation needs stub only if it cannot reach
  // the target (a code rewrite is not enough)
  return !isRelocInRange(pReloc, pTargetValue, Offset, Module);
}

bool THMToARMStub::isRelocInRange(const class Relocation *pReloc,
                                  int64_t pTargetValue, int64_t &Offset,
                                  Module &Module) const {
  int addend = pReloc->addend() + 4;
  Offset = pTargetValue + addend - pReloc->place(Module);
  switch (pReloc->type()) {
  case llvm::ELF::R_ARM_THM_JUMP24:
  case llvm::ELF::R_ARM_THM_CALL: {
    if (m_Target->isJ1J2BranchEncoding())
      return llvm::isInt<THM2_MAX_BRANCH_BITS>(Offset);
    return llvm::isInt<THM_MAX_BRANCH_BITS>(Offset);
  }
  default:
    break;
  }
  return true;
}

const std::string &THMToARMStub::name() const { return m_Name; }

const uint8_t *THMToARMStub::getContent() const {
  return reinterpret_cast<const uint8_t *>(m_pData);
}

size_t THMToARMStub::alignment() const { return 4u; }

// for T bit of this stub
uint64_t THMToARMStub::initSymValue() const { return 0x1; }

Stub *THMToARMStub::clone(InputFile *F, Relocation *R, eld::IRBuilder *pBuilder,
                          DiagnosticEngine *DiagEngine) {
  if (R && m_Target->isMicroController()) {
    const GeneralOptions &options = pBuilder->getConfig().options();
    DiagEngine->raise(Diag::branch_to_arm_code_not_allowed)
        << R->symInfo()->name()
        << Relocation::getFragmentPath(nullptr, R->targetRef()->frag(),
                                       options);
    return nullptr;
  }
  Stub *S = make<THMToARMStub>(m_pData, ThisSize, fixupBegin(), fixupEnd(),
                               Alignment, m_NumStub++);
  uint32_t DataOffset = 0;
  pBuilder->addLinkerInternalLocalSymbol(F,
                                         "$t.t2a." + std::to_string(m_NumStub),
                                         eld::make<FragmentRef>(*S, 0), 0);
  if (m_Type == ARMGNULDBackend::VENEER_PIC) {
    DataOffset = 12;
    pBuilder->addLinkerInternalLocalSymbol(
        F, "$a.t2a." + std::to_string(m_NumStub), eld::make<FragmentRef>(*S, 4),
        0);
  } else if (m_Type == ARMGNULDBackend::VENEER_MOV)
    DataOffset = 0;
  else {
    pBuilder->addLinkerInternalLocalSymbol(
        F, "$a.t2a." + std::to_string(m_NumStub), eld::make<FragmentRef>(*S, 4),
        0);
    DataOffset = 8;
  }

  if (DataOffset)
    pBuilder->addLinkerInternalLocalSymbol(
        F, "$d.t2a." + std::to_string(m_NumStub),
        eld::make<FragmentRef>(*S, DataOffset), 0);
  return S;
}

std::string THMToARMStub::getStubName(const Relocation &pReloc, bool isClone,
                                      bool isSectionRelative,
                                      int64_t numBranchIsland, int64_t numClone,
                                      uint32_t relocAddend,
                                      bool useOldStyleTrampolineName) const {
  (void)isClone;
  (void)isSectionRelative;
  (void)numBranchIsland;
  (void)numClone;
  (void)relocAddend;
  std::string name("__");
  const ResolveInfo *pInfo = pReloc.symInfo();
  name += pInfo->name() + std::string("_") + m_Name + std::string("@island-") +
          std::to_string(m_NumStub);
  return name;
}
