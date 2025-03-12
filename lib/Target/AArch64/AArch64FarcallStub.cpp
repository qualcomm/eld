//===- AArch64FarcallStub.cpp----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "AArch64FarcallStub.h"
#include "AArch64LDBackend.h"
#include "AArch64Relocator.h"
#include "eld/Readers/Relocation.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "llvm/BinaryFormat/ELF.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// AArch64FarcallStub
//===----------------------------------------------------------------------===//

const uint32_t AArch64FarcallStub::TEMPLATE[] = {
    0x10000090,     // adr  x16, #16
    0xf9400210,     // ldr  x16, [x16]
    0xd61f0200,     // br   x16
    0x00000000,     // alignment fillment
    0x0,        0x0 // dcd  R_AARCH64_ABS64(X)
};

const uint32_t AArch64FarcallStub::TEMPLATE_PIC[] = {
    0x58000090, // ldr     x16, #16
    0x10000011, // adr     x17, <pc>
    0x8b110210, // add     x16, x16, x17
    0xd61f0200, // br      x16
    0x0,        0x0};

#define FITS_IN_NBITS(D, B)                                                    \
  ((uint64_t)llabs(D) < (~(~(uint64_t)0 << ((B) - 1))))

AArch64FarcallStub::AArch64FarcallStub(bool pIsOutputPIC)
    : Stub(), m_Name("__trampoline"), m_pData(nullptr) {
  if (pIsOutputPIC) {
    m_pData = TEMPLATE_PIC;
    m_Size = sizeof(TEMPLATE_PIC);
  } else {
    m_pData = TEMPLATE;
    m_Size = sizeof(TEMPLATE);
  }
  m_Alignment = alignment();
  if (pIsOutputPIC)
    addFixup(16u, 12, llvm::ELF::R_AARCH64_PREL64);
  else
    addFixup(16u, 0x0, llvm::ELF::R_AARCH64_ABS64);
}

AArch64FarcallStub::AArch64FarcallStub(const uint32_t *pData, size_t pSize,
                                       const_fixup_iterator pBegin,
                                       const_fixup_iterator pEnd, size_t align)
    : Stub(), m_Name("__trampoline"), m_pData(pData) {
  m_Size = pSize;
  m_Alignment = align;
  for (auto it = pBegin, ie = pEnd; it != ie; ++it)
    addFixup(**it);
}

AArch64FarcallStub::~AArch64FarcallStub() {}

bool AArch64FarcallStub::isRelocInRange(const Relocation *pReloc,
                                        int64_t targetAddr, int64_t &Offset,
                                        Module &Module) const {
  int nbits = 0;
  Offset = 0;
  switch (pReloc->type()) {
  case llvm::ELF::R_AARCH64_CALL26:
  case llvm::ELF::R_AARCH64_JUMP26:
    nbits = 26;
    break;
  default:
    return true;
  }

  // don't generate trampoline if it is undefined weak symbol.
  if (pReloc->symInfo()->isWeak() && pReloc->symInfo()->isUndef() &&
      !pReloc->symInfo()->isDyn() &&
      !(pReloc->symInfo()->reserved() & Relocator::ReservePLT))
    return true;

  Offset = targetAddr - pReloc->place(Module) + pReloc->addend();

  // The branch range is really +/- 128MB. But the 2 bits are not encoded, if
  // you see the actual application site. The value here needs to be shifted to
  // take into account that the relocation can really fit only 2^26bits
  // (Signed). Also this change removes Hexagon specific code in FITS_IN_NBITS.
  if (FITS_IN_NBITS((Offset >> 2), nbits))
    return true;

  return false;
}

const std::string &AArch64FarcallStub::name() const { return m_Name; }

const uint8_t *AArch64FarcallStub::getContent() const {
  return reinterpret_cast<const uint8_t *>(m_pData);
}

size_t AArch64FarcallStub::alignment() const { return 8u; }

Stub *AArch64FarcallStub::clone(InputFile *, Relocation *, eld::IRBuilder *,
                                DiagnosticEngine *) {
  return make<AArch64FarcallStub>(m_pData, m_Size, fixup_begin(), fixup_end(),
                                  m_Alignment);
}
