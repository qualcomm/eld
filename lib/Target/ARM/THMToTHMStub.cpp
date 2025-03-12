//===- THMToTHMStub.cpp----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#include "THMToTHMStub.h"
#include "ARMLDBackend.h"
#include "eld/Readers/Relocation.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "llvm/BinaryFormat/ELF.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// THMToTHMStub
//===----------------------------------------------------------------------===//
const uint32_t THMToTHMStub::PIC_TEMPLATE[] = {
    0x46c04778, // bx    pc ... nop
    0xe59fc004, // ldr   r12, [pc, #4]
    0xe08fc00c, // add   ip, pc, ip
    0xe12fff1c, // bx    ip
    0x0         // dcd   R_ARM_REL32(X)
};

const uint32_t THMToTHMStub::TEMPLATE[] = {
    0x46c04778, // bx    pc ... nop
    0xe59fc000, // ldr   ip, [pc, #0]
    0xe12fff1c, // bx    ip
    0x0         // dcd   R_ARM_ABS32(X)
};

const uint32_t THMToTHMStub::TEMPLATE_MOV[] = {
    0x0c00f240, // movw  ip, R_ARM_THM_MOVW_ABS_NC(X)
    0x0c00f2c0, // movt  ip, R_ARM_THM_MOVT_ABS(X)
    0xdefe4760, // bx    ip ... trap
};

const uint32_t THMToTHMStub::TEMPLATE_THUMB1[] = {
    0x4801b403, // push {r0,r1}  ... ldr r0, [pc, #4]
    0xbd019001, // str r0, [sp, #4] ... pop {r0, pc}
    0x0         // dcd R_ARM_ABS32(X)
};

ARMGNULDBackend *THMToTHMStub::m_Target = nullptr;

THMToTHMStub::THMToTHMStub(uint32_t type, ARMGNULDBackend *Target)
    : Stub(), m_Name("T2T_veneer"), m_pData(nullptr), m_NumStub(0),
      m_Type(type) {
  if (type == ARMGNULDBackend::VENEER_PIC) {
    m_pData = PIC_TEMPLATE;
    m_Size = sizeof(PIC_TEMPLATE);
    addFixup(16u, 0x0, llvm::ELF::R_ARM_REL32);
  } else if (type == ARMGNULDBackend::VENEER_MOV) {
    m_pData = TEMPLATE_MOV;
    m_Size = sizeof(TEMPLATE_MOV);
    addFixup(0u, 0x0, llvm::ELF::R_ARM_THM_MOVW_ABS_NC);
    addFixup(4u, 0x0, llvm::ELF::R_ARM_THM_MOVT_ABS);
  } else if (type == ARMGNULDBackend::VENEER_THUMB1) {
    m_pData = TEMPLATE_THUMB1;
    m_Size = sizeof(TEMPLATE_THUMB1);
    addFixup(8u, 0x0, llvm::ELF::R_ARM_ABS32);
  } else {
    m_pData = TEMPLATE;
    m_Size = sizeof(TEMPLATE);
    addFixup(12u, 0x0, llvm::ELF::R_ARM_ABS32);
  }
  m_Alignment = alignment();
  m_Target = Target;
}

/// for doClone
THMToTHMStub::THMToTHMStub(const uint32_t *pData, size_t pSize,
                           const_fixup_iterator pBegin,
                           const_fixup_iterator pEnd, size_t align,
                           uint32_t pNumStub)
    : Stub(), m_Name("T2T_veneer"), m_pData(pData), m_NumStub(pNumStub) {
  m_Size = pSize;
  m_Alignment = align;
  for (auto it = pBegin, ie = pEnd; it != ie; ++it)
    addFixup(**it);
}

THMToTHMStub::~THMToTHMStub() {}

// Find out if this stub is the appropriate
// stub for the StubFactory to use when creating
// a branch island.
bool THMToTHMStub::isNeeded(const Relocation *pReloc, int64_t pTargetValue,
                            Module &Module) const {
  int64_t Offset = 0;
  // This stub cannot be used for ARM target.
  if ((pTargetValue & 0x1) == 0)
    return false;
  // The stub is needed only if the target is unreachable
  return !isRelocInRange(pReloc, pTargetValue, Offset, Module);
}

bool THMToTHMStub::isRelocInRange(const Relocation *pReloc,
                                  int64_t pTargetValue, int64_t &Offset,
                                  Module &Module) const {
  // TODO: find addend stored in opcode
  //  is the bias addend for branch target
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

const std::string &THMToTHMStub::name() const { return m_Name; }

const uint8_t *THMToTHMStub::getContent() const {
  return reinterpret_cast<const uint8_t *>(m_pData);
}

size_t THMToTHMStub::alignment() const { return 4u; }

uint64_t THMToTHMStub::initSymValue() const { return 0x1; }

Stub *THMToTHMStub::clone(InputFile *F, Relocation *, eld::IRBuilder *pBuilder,
                          DiagnosticEngine *) {
  Stub *S = make<THMToTHMStub>(m_pData, m_Size, fixup_begin(), fixup_end(),
                               m_Alignment, m_NumStub++);
  uint32_t DataOffset = 0;
  if (m_Type == ARMGNULDBackend::VENEER_PIC) {
    pBuilder->addLinkerInternalLocalSymbol(
        F, "$a.t2t." + std::to_string(m_NumStub), eld::make<FragmentRef>(*S, 0),
        0);
    pBuilder->addLinkerInternalLocalSymbol(
        F, "$t.t2t." + std::to_string(m_NumStub), eld::make<FragmentRef>(*S, 4),
        0);
    DataOffset = 16;
  } else if (m_Type == ARMGNULDBackend::VENEER_MOV) {
    pBuilder->addLinkerInternalLocalSymbol(
        F, "$t.t2t." + std::to_string(m_NumStub), eld::make<FragmentRef>(*S, 0),
        0);
    DataOffset = 0;
  } else if (m_Type == ARMGNULDBackend::VENEER_THUMB1) {
    pBuilder->addLinkerInternalLocalSymbol(
        F, "$t.t2t." + std::to_string(m_NumStub), eld::make<FragmentRef>(*S, 0),
        0);
    DataOffset = 8;
  } else {
    pBuilder->addLinkerInternalLocalSymbol(
        F, "$a.t2t." + std::to_string(m_NumStub), eld::make<FragmentRef>(*S, 0),
        0);
    pBuilder->addLinkerInternalLocalSymbol(
        F, "$t.t2t." + std::to_string(m_NumStub), eld::make<FragmentRef>(*S, 4),
        0);
    DataOffset = 12;
  }

  if (DataOffset)
    pBuilder->addLinkerInternalLocalSymbol(
        F, "$d.t2t." + std::to_string(m_NumStub),
        eld::make<FragmentRef>(*S, DataOffset), 0);
  return S;
}

std::string THMToTHMStub::getStubName(const Relocation &pReloc, bool isClone,
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
