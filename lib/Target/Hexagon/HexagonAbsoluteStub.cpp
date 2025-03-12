//===- HexagonAbsoluteStub.cpp---------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#include "HexagonAbsoluteStub.h"
#include "HexagonLDBackend.h"
#include "HexagonRelocator.h"
#include "eld/Fragment/FragUtils.h"
#include "eld/Fragment/RegionFragment.h"
#include "eld/Fragment/RegionFragmentEx.h"
#include "eld/Readers/Relocation.h"
#include "eld/Support/Memory.h"
#include "eld/Support/MsgHandling.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "llvm/BinaryFormat/ELF.h"

using namespace eld;

class IRBuilder;

//===----------------------------------------------------------------------===//
// HexagonAbsoluteStub
//===----------------------------------------------------------------------===//

const uint32_t HexagonAbsoluteStub::TEMPLATE[] = {
    0x00004000, /*  { immext(#) */
    0x5800c000, /*    jump ## } */
};

const uint32_t HexagonAbsoluteStub::TEMPLATE_PIC[] = {
    0x00004000, // {  immext(#0)
    0x6a49c00e, //   r14 = add(pc,##0) }
    0x528ec000, // {  jumpr r14 }
};

HexagonAbsoluteStub::HexagonAbsoluteStub(bool pIsOutputPIC)
    : Stub(), m_Name("__trampoline"), m_pData(nullptr) {
  if (pIsOutputPIC) {
    m_pData = (const uint8_t *)TEMPLATE_PIC;
    m_Size = sizeof(TEMPLATE_PIC);
  } else {
    m_pData = (const uint8_t *)TEMPLATE;
    m_Size = sizeof(TEMPLATE);
  }
  m_Alignment = alignment();
  addFixup(0u, 0x0, llvm::ELF::R_HEX_B32_PCREL_X);
  if (pIsOutputPIC)
    addFixup(4u, 0x4, llvm::ELF::R_HEX_6_PCREL_X);
  else
    addFixup(4u, 0x4, llvm::ELF::R_HEX_B22_PCREL_X);
}

HexagonAbsoluteStub::HexagonAbsoluteStub(const uint8_t *pData, size_t pSize,
                                         const_fixup_iterator pBegin,
                                         const_fixup_iterator pEnd,
                                         size_t align)
    : Stub(), m_Name("__trampoline"), m_pData(pData) {
  m_Size = pSize;
  m_Alignment = align;
  for (auto it = pBegin, ie = pEnd; it != ie; ++it)
    addFixup(**it);
}

HexagonAbsoluteStub::HexagonAbsoluteStub(const uint8_t *pData, size_t pSize,
                                         uint32_t align)
    : Stub(), m_Name("__copy_from"), m_pData(pData) {
  m_Size = pSize;
  m_Alignment = align;
}

HexagonAbsoluteStub::~HexagonAbsoluteStub() {}

bool HexagonAbsoluteStub::isRelocInRange(const Relocation *pReloc,
                                         int64_t targetAddr, int64_t &Offset,
                                         Module &Module) const {
  Offset = targetAddr - pReloc->place(Module) + pReloc->addend();
  switch (pReloc->type()) {
  case llvm::ELF::R_HEX_B22_PCREL:
  case llvm::ELF::R_HEX_PLT_B22_PCREL:
  case llvm::ELF::R_HEX_GD_PLT_B22_PCREL:
  case llvm::ELF::R_HEX_LD_PLT_B22_PCREL:
    return llvm::isInt<22>(Offset >> 2);
  case llvm::ELF::R_HEX_B15_PCREL:
    return llvm::isInt<15>(Offset >> 2);
    break;
  case llvm::ELF::R_HEX_B13_PCREL:
    return llvm::isInt<13>(Offset >> 2);
    break;
  case llvm::ELF::R_HEX_B9_PCREL:
    return llvm::isInt<9>(Offset >> 2);
  default:
    break;
  }
  return true;
}

const std::string &HexagonAbsoluteStub::name() const { return m_Name; }

const uint8_t *HexagonAbsoluteStub::getContent() const {
  return reinterpret_cast<const uint8_t *>(m_pData);
}

size_t HexagonAbsoluteStub::alignment() const { return 4u; }

Stub *HexagonAbsoluteStub::clone(InputFile *, Relocation *, eld::IRBuilder *,
                                 DiagnosticEngine *) {
  std::lock_guard<std::mutex> Guard(Mutex);
  return make<HexagonAbsoluteStub>(m_pData, m_Size, fixup_begin(), fixup_end(),
                                   m_Alignment);
}

Stub *HexagonAbsoluteStub::clone(InputFile *, Relocation *, Fragment *frag,
                                 eld::IRBuilder *,
                                 DiagnosticEngine *DiagEngine) {
  if (frag->getKind() != Fragment::Region) {
    DiagEngine->raise(diag::clone_is_not_supported)
        << frag->getOwningSection()->name();
    return nullptr;
  }
  assert(frag->getKind() == Fragment::Region ||
         frag->getKind() == Fragment::RegionFragmentEx);
  llvm::StringRef RegionStr = getRegionFromFragment(frag);
  return make<HexagonAbsoluteStub>((const uint8_t *)RegionStr.data(),
                                   frag->size(), m_Alignment);
}

uint32_t
HexagonAbsoluteStub::getRealAddend(const Relocation &pReloc,
                                   DiagnosticEngine *DiagEngine) const {
  const Fragment *frag = pReloc.targetRef()->frag();
  if ((frag->getKind() != Fragment::Region) &&
      (frag->getKind() != Fragment::RegionFragmentEx)) {
    DiagEngine->raise(diag::addend_not_supported)
        << frag->getOwningSection()->name();
    return 0;
  }
  llvm::StringRef RegionStr = getRegionFromFragment(frag);
  uint32_t offset = pReloc.targetRef()->offset();
  assert(offset < frag->size() && "Offset is greater than fragment size!");
  const char *start = RegionStr.data();
  const char *regionAtOffset = start + offset;
  uint32_t maxInstructionsToCheck = 0;

  while ((regionAtOffset != start) && (maxInstructionsToCheck < 4)) {
    uint32_t wordAtRegion = 0;
    std::memcpy(&wordAtRegion, regionAtOffset - sizeof(uint32_t),
                sizeof(wordAtRegion));
    if (((wordAtRegion & MASK_END_PACKET) == END_OF_PACKET) ||
        ((wordAtRegion & MASK_END_PACKET) == END_OF_DUPLEX))
      break;
    regionAtOffset -= sizeof(uint32_t);
  }
  return ((start + offset) - regionAtOffset);
}
