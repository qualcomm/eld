//===- ARMToTHMStub.cpp----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#include "ARMToTHMStub.h"
#include "ARMLDBackend.h"
#include "eld/Readers/Relocation.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "llvm/BinaryFormat/ELF.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// ARMToTHMStub
//===----------------------------------------------------------------------===//
const uint32_t ARMToTHMStub::PIC_TEMPLATE[] = {
    0xe59fc004, // ldr   r12, [pc, #4]
    0xe08fc00c, // add   ip, pc, ip
    0xe12fff1c, // bx    ip
    0x0         // dcd   R_ARM_REL32(X)
};

const uint32_t ARMToTHMStub::TEMPLATE[] = {
    0xe59fc000, // ldr   ip, [pc, #0]
    0xe12fff1c, // bx    ip
    0x0         // dcd   R_ARM_ABS32(X)
};

const uint32_t ARMToTHMStub::TEMPLATE_MOV[] = {
    0xe300c000, // movw  ip, R_ARM_MOVW_ABS_NC(X)
    0xe340c000, // movt  ip, R_ARM_MOVT_ABS(X)
    0xe12fff1c, // bx    ip
};

ARMGNULDBackend *ARMToTHMStub::m_Target = nullptr;

ARMToTHMStub::ARMToTHMStub(uint32_t type, ARMGNULDBackend *Target)
    : Stub(), m_Name("A2T_veneer"), m_pData(nullptr), m_NumStub(0),
      m_Type(type) {
  if (type == ARMGNULDBackend::VENEER_PIC) {
    m_pData = PIC_TEMPLATE;
    m_Size = sizeof(PIC_TEMPLATE);
    addFixup(12u, 0x0, llvm::ELF::R_ARM_REL32);
  } else if (type == ARMGNULDBackend::VENEER_MOV) {
    m_pData = TEMPLATE_MOV;
    m_Size = sizeof(TEMPLATE_MOV);
    addFixup(0u, 0x0, llvm::ELF::R_ARM_MOVW_ABS_NC);
    addFixup(4u, 0x0, llvm::ELF::R_ARM_MOVT_ABS);
  } else {
    m_pData = TEMPLATE;
    m_Size = sizeof(TEMPLATE);
    addFixup(8u, 0x0, llvm::ELF::R_ARM_ABS32);
  }
  m_Alignment = alignment();
  m_Target = Target;
}

/// for doClone
ARMToTHMStub::ARMToTHMStub(const uint32_t *pData, size_t pSize,
                           const_fixup_iterator pBegin,
                           const_fixup_iterator pEnd, size_t align,
                           uint32_t pNumStub)
    : Stub(), m_Name("A2T_veneer"), m_pData(pData), m_NumStub(pNumStub) {
  m_Size = pSize;
  m_Alignment = align;
  for (auto it = pBegin, ie = pEnd; it != ie; ++it)
    addFixup(**it);
}

ARMToTHMStub::~ARMToTHMStub() {}

// Find out if this stub is the appropriate
// stub for the StubFactory to use when creating
// a branch island.
bool ARMToTHMStub::isNeeded(const Relocation *pReloc, int64_t pTargetValue,
                            Module &Module) const {
  int64_t Offset = 0;
  // Cannot have ARM to THM stub for an ARM target
  if ((pTargetValue & 0x1) == 0)
    return false;
  // Below relocations are for opcode that cannot be rewritten,
  // so this stub can be used in these cases. The stub will be cloned
  // only if it is unreachable
  switch (pReloc->type()) {
  case llvm::ELF::R_ARM_PC24:
  case llvm::ELF::R_ARM_JUMP24:
  case llvm::ELF::R_ARM_PLT32:
    return true;
  }
  return !isRelocInRange(pReloc, pTargetValue, Offset, Module);
}

// Check is the stub can be reused or a new one is needed.
bool ARMToTHMStub::isRelocInRange(const Relocation *pReloc,
                                  int64_t pTargetValue, int64_t &Offset,
                                  Module &Module) const {
  switch (pReloc->type()) {
  case llvm::ELF::R_ARM_CALL:
  case llvm::ELF::R_ARM_PC24:
  case llvm::ELF::R_ARM_JUMP24:
  case llvm::ELF::R_ARM_PLT32: {
    // FIXME: Assuming blx is available (i.e., target is armv5 or above!)
    // then, we do not need a stub unless the branch target is too far.
    // TODO: Find real addend from opcode and add to bias below
    int addend = pReloc->addend() + 8;
    Offset = pTargetValue + addend - pReloc->place(Module);
    if ((Offset > ARMGNULDBackend::ARM_MAX_FWD_BRANCH_OFFSET) ||
        (Offset < ARMGNULDBackend::ARM_MAX_BWD_BRANCH_OFFSET)) {
      return false;
    }
    break;
  }
  default:
    break;
  }
  return true;
}

const std::string &ARMToTHMStub::name() const { return m_Name; }

const uint8_t *ARMToTHMStub::getContent() const {
  return reinterpret_cast<const uint8_t *>(m_pData);
}

size_t ARMToTHMStub::alignment() const { return 4u; }

Stub *ARMToTHMStub::clone(InputFile *F, Relocation *, eld::IRBuilder *pBuilder,
                          DiagnosticEngine *) {
  Stub *S = make<ARMToTHMStub>(m_pData, m_Size, fixup_begin(), fixup_end(),
                               m_Alignment, m_NumStub++);
  pBuilder->addLinkerInternalLocalSymbol(F,
                                         "$a.a2t." + std::to_string(m_NumStub),
                                         eld::make<FragmentRef>(*S, 0), 0);
  uint32_t DataOffset = 0;
  if (m_Type == ARMGNULDBackend::VENEER_PIC)
    DataOffset = 12;
  else if (m_Type == ARMGNULDBackend::VENEER_MOV)
    DataOffset = 0;
  else
    DataOffset = 8;

  if (DataOffset)
    pBuilder->addLinkerInternalLocalSymbol(
        F, "$d.a2t." + std::to_string(m_NumStub),
        eld::make<FragmentRef>(*S, DataOffset), 0);
  return S;
}

std::string ARMToTHMStub::getStubName(const Relocation &pReloc, bool isClone,
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
