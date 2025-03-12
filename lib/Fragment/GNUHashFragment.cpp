//===- GNUHashFragment.cpp-------------------------------------------------===//
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

#include "eld/Fragment/GNUHashFragment.h"
#include "eld/Support/Memory.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "llvm/ADT/DenseMap.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// GNUHashFragment
//===----------------------------------------------------------------------===//
template <class ELFT>
GNUHashFragment<ELFT>::GNUHashFragment(ELFSection *O,
                                       std::vector<ResolveInfo *> &R)
    : TargetFragment(TargetFragment::Kind::GNUHash, O, nullptr, 4, 0),
      DynamicSymbols(R), NBuckets(0), MaskWords(0), Shift2(0) {
  sortSymbols();
}

template <class ELFT> GNUHashFragment<ELFT>::~GNUHashFragment() {}

template <class ELFT> const std::string GNUHashFragment<ELFT>::name() const {
  return "Fragment for GNUHash";
}

template <class ELFT>
unsigned GNUHashFragment<ELFT>::calcNBuckets(unsigned NumHashed) const {
  if (!NumHashed)
    return 0;

  // These values are prime numbers which are not greater than 2^(N-1) + 1.
  // In result, for any particular NumHashed we return a prime number
  // which is not greater than NumHashed.
  static const unsigned Primes[] = {
      1,   1,    3,    3,    7,    13,    31,    61,    127,   251,
      509, 1021, 2039, 4093, 8191, 16381, 32749, 65521, 131071};

  return Primes[std::min<unsigned>(llvm::Log2_32_Ceil(NumHashed),
                                   std::size(Primes) - 1)];
}

// Bloom filter estimation: at least 8 bits for each hashed symbol.
// GNU Hash table requirement: it should be a power of 2,
//   the minimum value is 1, even for an empty table.
// Expected results for a 32-bit target:
//   calcMaskWords(0..4)   = 1
//   calcMaskWords(5..8)   = 2
//   calcMaskWords(9..16)  = 4
// For a 64-bit target:
//   calcMaskWords(0..8)   = 1
//   calcMaskWords(9..16)  = 2
//   calcMaskWords(17..32) = 4
template <class ELFT>
unsigned GNUHashFragment<ELFT>::calcMaskWords(unsigned NumHashed) const {
  typedef typename llvm::object::ELFFile<ELFT>::Elf_Off Elf_Off;
  if (!NumHashed)
    return 1;
  return llvm::NextPowerOf2((NumHashed - 1) / sizeof(Elf_Off));
}

template <class ELFT> size_t GNUHashFragment<ELFT>::size() const {
  typedef typename ELFT::Word Elf_Word;
  typedef typename llvm::object::ELFFile<ELFT>::Elf_Off Elf_Off;

  unsigned NumHashed = Symbols.size();
  NBuckets = calcNBuckets(NumHashed);
  MaskWords = calcMaskWords(NumHashed);
  // Second hash shift estimation: just predefined values.
  Shift2 = ELFT::Is64Bits ? 6 : 5;
  return (sizeof(Elf_Word) * 4             // Header
          + sizeof(Elf_Off) * MaskWords    // Bloom Filter
          + sizeof(Elf_Word) * NBuckets    // Hash Buckets
          + sizeof(Elf_Word) * NumHashed); // Hash Values
}

template <class ELFT>
eld::Expected<void> GNUHashFragment<ELFT>::emit(MemoryRegion &mr, Module &M) {
  uint8_t *Buf = reinterpret_cast<uint8_t *>(mr.begin());
  writeHeader(Buf);
  // If there are no symbols that can be hashed, dont do anything.
  if (!Symbols.size())
    return {};
  writeBloomFilter(Buf);
  writeHashTable(Buf);
  return {};
}

template <class ELFT> void GNUHashFragment<ELFT>::writeHeader(uint8_t *&Buf) {
  typedef typename ELFT::Word Elf_Word;
  auto *P = reinterpret_cast<Elf_Word *>(Buf);
  *P++ = NBuckets;
  *P++ = DynamicSymbols.size() - Symbols.size();
  *P++ = MaskWords;
  *P++ = Shift2;
  Buf = reinterpret_cast<uint8_t *>(P);
}

template <class ELFT>
void GNUHashFragment<ELFT>::writeBloomFilter(uint8_t *&Buf) {
  typedef typename llvm::object::ELFFile<ELFT>::Elf_Off Elf_Off;
  typedef typename llvm::object::ELFFile<ELFT>::uintX_t uintX_t;

  unsigned C = sizeof(Elf_Off) * 8;

  auto *Masks = reinterpret_cast<Elf_Off *>(Buf);
  for (auto &Sym : Symbols) {
    size_t Pos = (Sym->Hash / C) & (MaskWords - 1);
    uintX_t V = (uintX_t(1) << (Sym->Hash % C)) |
                (uintX_t(1) << ((Sym->Hash >> Shift2) % C));
    Masks[Pos] |= V;
  }
  Buf += sizeof(Elf_Off) * MaskWords;
}

template <class ELFT>
void GNUHashFragment<ELFT>::writeHashTable(uint8_t *&Buf) {
  typedef typename ELFT::Word Elf_Word;

  Elf_Word *Buckets = reinterpret_cast<Elf_Word *>(Buf);
  Elf_Word *Values = Buckets + NBuckets;

  int PrevBucket = -1;
  int I = 0;

  for (auto &Sym : Symbols) {
    int Bucket = Sym->Hash % NBuckets;
    assert(PrevBucket <= Bucket);
    if (Bucket != PrevBucket) {
      Buckets[Bucket] = Sym->DynSymIndex;
      PrevBucket = Bucket;
      if (I > 0)
        Values[I - 1] |= 1;
    }
    Values[I] = Sym->Hash & ~1;
    ++I;
  }
  if (I > 0)
    Values[I - 1] |= 1;
}

// Add symbols to this symbol hash table. Note that this function
// destructively sort a given vector -- which is needed because
// GNU-style hash table places some sorting requirements.
template <class ELFT> void GNUHashFragment<ELFT>::sortSymbols() {
  // Ideally this will just be 'auto' but GCC 6.1 is not able
  // to deduce it correctly.
  std::vector<ResolveInfo *>::iterator Mid = std::stable_partition(
      DynamicSymbols.begin(), DynamicSymbols.end(),
      [](const ResolveInfo *S) { return (S->isDyn() || S->isUndef()); });
  if (Mid == DynamicSymbols.end())
    return;
  for (auto I = Mid, E = DynamicSymbols.end(); I != E; ++I) {
    Symbols.push_back(make<SymbolData>(*I, I - DynamicSymbols.begin(),
                                       hashGnu(llvm::StringRef((*I)->name()))));
  }

  unsigned NBuckets = calcNBuckets(Symbols.size());
  std::stable_sort(Symbols.begin(), Symbols.end(),
                   [&](const SymbolData *L, const SymbolData *R) {
                     return L->Hash % NBuckets < R->Hash % NBuckets;
                   });

  DynamicSymbols.erase(Mid, DynamicSymbols.end());

  // Setup the DynSymIndex.
  uint32_t DynamicSymbolsCount = DynamicSymbols.size();
  llvm::DenseMap<ResolveInfo *, uint32_t> DynSymToIndex;
  for (auto &Sym : Symbols) {
    DynSymToIndex[Sym->R] = DynamicSymbolsCount++;
    DynamicSymbols.push_back(Sym->R);
  }

  // Update the DynSymIndex.
  for (auto &S : Symbols)
    S->DynSymIndex = DynSymToIndex[S->R];
}

template class eld::GNUHashFragment<llvm::object::ELF32LE>;
template class eld::GNUHashFragment<llvm::object::ELF32BE>;
template class eld::GNUHashFragment<llvm::object::ELF64LE>;
template class eld::GNUHashFragment<llvm::object::ELF64BE>;
