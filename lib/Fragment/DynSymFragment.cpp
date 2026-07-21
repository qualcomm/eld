//===- DynSymFragment.cpp--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Fragment/DynSymFragment.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Target/GNULDBackend.h"
#include "llvm/BinaryFormat/ELF.h"
#include <optional>

using namespace eld;

DynSymFragment::DynSymFragment(ELFSection *S,
                               const std::vector<ResolveInfo *> &DynSyms,
                               bool Is32Bits, uint32_t Align)
    : Fragment(Fragment::Type::DynSym, S, Align), DynamicSymbols(DynSyms),
      Is32Bits(Is32Bits) {}

size_t DynSymFragment::size() const {
  return DynamicSymbols.size() * (Is32Bits ? sizeof(llvm::ELF::Elf32_Sym)
                                           : sizeof(llvm::ELF::Elf64_Sym));
}

eld::Expected<void> DynSymFragment::emit(MemoryRegion &Mr, Module &M) {
  ELFSection *DynStrSect = M.getBackend().getDynStrSection();
  if (!DynStrSect || DynStrSect->isIgnore() || DynStrSect->isDiscard() ||
      !DynStrSect->size()) {
    M.getConfig().raise(Diag::section_ignored) << ".dynstr";
    return {};
  }

  uint8_t *Buf = Mr.begin() + getOffset(M.getConfig().getDiagEngine());
  auto *Symtab32 = reinterpret_cast<llvm::ELF::Elf32_Sym *>(Buf);
  auto *Symtab64 = reinterpret_cast<llvm::ELF::Elf64_Sym *>(Buf);

  std::optional<size_t> FirstNonLocal;
  size_t SymIdx = 0;
  for (ResolveInfo *DynSym : DynamicSymbols) {
    if (Is32Bits)
      M.getBackend().emitSymbol32(Symtab32[SymIdx], DynSym->outSymbol(),
                                  nullptr, 0, SymIdx,
                                  /*isDynSymTab=*/true);
    else
      M.getBackend().emitSymbol64(Symtab64[SymIdx], DynSym->outSymbol(),
                                  nullptr, 0, SymIdx,
                                  /*isDynSymTab=*/true);
    if ((DynSym->isGlobal() || DynSym->isWeak()) && !FirstNonLocal)
      FirstNonLocal = SymIdx;
    ++SymIdx;
  }

  if (FirstNonLocal) {
    if (ELFSection *Out = getOutputELFSection())
      Out->setInfo(*FirstNonLocal);
    getOwningSection()->setInfo(*FirstNonLocal);
  }
  return {};
}
