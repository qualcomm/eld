//===- ARMToARMStub.cpp----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#include "ARMToARMStub.h"
#include "ARMLDBackend.h"
#include "eld/Readers/Relocation.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "llvm/BinaryFormat/ELF.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// ARMToARMStub
//===----------------------------------------------------------------------===//
const uint32_t ARMToARMStub::PIC_TEMPLATE[] = {
    0xe59fc000, // ldr   r12, [pc]
    0xe08ff00c, // add   pc, pc, ip
    0x0         // dcd   R_ARM_REL32(X-4)
};

const uint32_t ARMToARMStub::TEMPLATE[] = {
    0xe51ff004, // ldr   pc, [pc, #-4]
    0x0         // dcd   R_ARM_ABS32(X)
};

ARMGNULDBackend *ARMToARMStub::m_Target = nullptr;

ARMToARMStub::ARMToARMStub(uint32_t type, ARMGNULDBackend *Target)
    : Stub(), m_Name("A2A_veneer"), m_pData(nullptr), m_NumStub(0),
      m_Type(type) {
  if (type == ARMGNULDBackend::VENEER_PIC) {
    m_pData = PIC_TEMPLATE;
    m_Size = sizeof(PIC_TEMPLATE);
    addFixup(8u, -4, llvm::ELF::R_ARM_REL32);
  } else {
    m_pData = TEMPLATE;
    m_Size = sizeof(TEMPLATE);
    addFixup(4u, 0x0, llvm::ELF::R_ARM_ABS32);
  }
  m_Alignment = alignment();
  m_Target = Target;
}

ARMToARMStub::ARMToARMStub(const uint32_t *pData, size_t pSize,
                           const_fixup_iterator pBegin,
                           const_fixup_iterator pEnd, size_t align,
                           uint32_t pNumStub)
    : Stub(), m_Name("A2A_veneer"), m_pData(pData), m_NumStub(pNumStub) {
  m_Size = pSize;
  m_Alignment = align;
  for (auto it = pBegin, ie = pEnd; it != ie; ++it)
    addFixup(**it);
}

ARMToARMStub::~ARMToARMStub() {}

// Find out if this stub is the appropriate
// stub for the StubFactory to use when creating
// a branch island.
bool ARMToARMStub::isNeeded(const Relocation *pReloc, int64_t pTargetValue,
                            Module &Module) const {
  int64_t Offset = 0;
  // If target is thumb, ARM to ARM stub is of no use
  if ((pTargetValue & 0x1) != 0)
    return false;
  // This stub is useful only if the target is ARM and
  // unreachable by the reloc.
  return !isRelocInRange(pReloc, pTargetValue, Offset, Module);
}

bool ARMToARMStub::isRelocInRange(const Relocation *pReloc,
                                  int64_t pTargetValue, int64_t &Offset,
                                  Module &Module) const {
  switch (pReloc->type()) {
  case llvm::ELF::R_ARM_PC24:
  case llvm::ELF::R_ARM_CALL:
  case llvm::ELF::R_ARM_JUMP24:
  case llvm::ELF::R_ARM_PLT32: {
    // Check if the branch target is too far
    // TODO: find addend stored in opcode
    // 8 is the bias addend for branch target
    int addend = pReloc->addend() + 8;
    int64_t Offset = pTargetValue + addend - pReloc->place(Module);
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

const std::string &ARMToARMStub::name() const { return m_Name; }

const uint8_t *ARMToARMStub::getContent() const {
  return reinterpret_cast<const uint8_t *>(m_pData);
}

size_t ARMToARMStub::alignment() const { return 4u; }

Stub *ARMToARMStub::clone(InputFile *F, Relocation *, eld::IRBuilder *pBuilder,
                          DiagnosticEngine *) {
  Stub *S = make<ARMToARMStub>(m_pData, m_Size, fixup_begin(), fixup_end(),
                               m_Alignment, m_NumStub++);
  pBuilder->addLinkerInternalLocalSymbol(F,
                                         "$a.a2a." + std::to_string(m_NumStub),
                                         eld::make<FragmentRef>(*S, 0), 0);
  uint32_t DataOffset = 4;

  if (m_Type == ARMGNULDBackend::VENEER_PIC)
    DataOffset = 8;
  if (DataOffset)
    pBuilder->addLinkerInternalLocalSymbol(
        F, "$d.a2a." + std::to_string(m_NumStub),
        eld::make<FragmentRef>(*S, DataOffset), 0);
  return S;
}

std::string ARMToARMStub::getStubName(const Relocation &pReloc, bool isClone,
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
