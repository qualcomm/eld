//===- SysVHashFragment.cpp------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "eld/Fragment/SysVHashFragment.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "llvm/Object/ELF.h"
#include "llvm/Object/ELFTypes.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// SysVHashFragment
//===----------------------------------------------------------------------===//
template <class ELFT>
SysVHashFragment<ELFT>::SysVHashFragment(ELFSection *O,
                                         std::vector<ResolveInfo *> &R)
    : TargetFragment(TargetFragment::Kind::SysVHash, O, nullptr, 4, 0),
      DynamicSymbols(R) {}

template <class ELFT> SysVHashFragment<ELFT>::~SysVHashFragment() {}

template <class ELFT> const std::string SysVHashFragment<ELFT>::name() const {
  return "Fragment for SysVHash";
}

template <class ELFT> size_t SysVHashFragment<ELFT>::size() const {
  typedef typename ELFT::Word Elf_Word;
  unsigned NumEntries = 2;                     // nbucket and nchain.
  NumEntries += (2 * (DynamicSymbols.size())); // The chain entries.
  return (NumEntries * sizeof(Elf_Word));
}

template <class ELFT>
eld::Expected<void> SysVHashFragment<ELFT>::emit(MemoryRegion &mr, Module &M) {
  unsigned NumSymbols = DynamicSymbols.size();
  typedef typename ELFT::Word Elf_Word;
  auto *P = reinterpret_cast<Elf_Word *>(mr.begin());
  *P++ = NumSymbols; // nbucket
  *P++ = NumSymbols; // nchain

  Elf_Word *Buckets = P;
  Elf_Word *Chains = P + NumSymbols;

  auto It = std::begin(DynamicSymbols);
  for (auto &R : DynamicSymbols) {
    llvm::StringRef Name = R->getNonVersionedName();
    // For the null entry.
    unsigned I = It - DynamicSymbols.begin();
    uint32_t Hash = llvm::object::hashSysV(Name) % NumSymbols;
    Chains[I] = Buckets[Hash];
    Buckets[Hash] = I;
    ++It;
  }
  return {};
}

template class eld::SysVHashFragment<llvm::object::ELF32LE>;
template class eld::SysVHashFragment<llvm::object::ELF32BE>;
template class eld::SysVHashFragment<llvm::object::ELF64LE>;
template class eld::SysVHashFragment<llvm::object::ELF64BE>;
