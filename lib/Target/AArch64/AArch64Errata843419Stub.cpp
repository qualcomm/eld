//===- AArch64Errata843419Stub.cpp-----------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#include "AArch64Errata843419Stub.h"
#include "AArch64InsnHelpers.h"
#include "AArch64LDBackend.h"
#include "AArch64RelocationHelpers.h"
#include "AArch64Relocator.h"
#include "eld/BranchIsland/BranchIsland.h"
#include "eld/Core/Module.h"
#include "eld/Fragment/FragmentRef.h"
#include "eld/Readers/Relocation.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/BinaryFormat/ELF.h"

namespace eld {

#define FITS_IN_NBITS(D, B)                                                    \
  ((uint64_t)labs(D) < (~(~(uint64_t)0 << ((B) - 1)) & -(4 * 4)))

//===----------------------------------------------------------------------===//
// AArch64Errata843419Stub
//===----------------------------------------------------------------------===//
const uint32_t AArch64Errata843419Stub::TEMPLATE[] = {
    0x00000000, // Placeholder for erratum insn
    0x00000000, // Placeholder for branch instruction
};

AArch64Errata843419Stub::AArch64Errata843419Stub()
    : m_Name("erratum_prototype"), m_pData(TEMPLATE), m_Size(0x0) {
  m_Size = sizeof(TEMPLATE);
  addFixup(0x0, 0, R_AARCH64_COPY_INSN);
  addFixup(0x4, 0, llvm::ELF::R_AARCH64_JUMP26);
}

/// for doClone
AArch64Errata843419Stub::AArch64Errata843419Stub(const uint32_t *pData,
                                                 size_t pSize,
                                                 const_fixup_iterator pBegin,
                                                 const_fixup_iterator pEnd,
                                                 size_t align)
    : m_Name("__errata"), m_pData(pData) {
  m_Size = pSize;
  Alignment = align;
  for (auto it = pBegin, ie = pEnd; it != ie; ++it)
    addFixup(**it);
}

bool AArch64Errata843419Stub::isRelocInRange(const Relocation *reloc,
                                             int64_t fragAddr, int64_t &Offset,
                                             Module &Module) const {
  int nbits = 26;
  Offset = fragAddr;
  if (FITS_IN_NBITS(fragAddr, nbits))
    return true;
  return false;
}

const uint8_t *AArch64Errata843419Stub::getContent() const {
  return reinterpret_cast<const uint8_t *>(m_pData);
}

size_t AArch64Errata843419Stub::size() const { return m_Size; }

size_t AArch64Errata843419Stub::alignment() const { return 8u; }

const std::string &AArch64Errata843419Stub::name() const { return m_Name; }

Stub *AArch64Errata843419Stub::clone(InputFile *, Relocation *R,
                                     eld::IRBuilder *, DiagnosticEngine *) {
  return make<AArch64Errata843419Stub>(m_pData, m_Size, fixupBegin(),
                                       fixupEnd(), Alignment);
}

} // namespace eld
