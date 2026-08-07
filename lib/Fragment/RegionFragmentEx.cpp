//===- RegionFragmentEx.cpp------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#include "eld/Fragment/RegionFragmentEx.h"
#include "eld/Core/Module.h"
#include "eld/Readers/ELFSection.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// RegionFragmentEx
//===----------------------------------------------------------------------===//
RegionFragmentEx::RegionFragmentEx(const char *Buf, size_t Sz, ELFSection *O,
                                   uint32_t Align)
    : Fragment(Fragment::Type::RegionFragmentEx, O, Align), Data(Buf), Size(Sz)
#ifndef NDEBUG
      ,
      Capacity(Sz)
#endif
{
}

RegionFragmentEx::~RegionFragmentEx() {}

bool RegionFragmentEx::replaceInstruction(uint32_t Offset, Relocation *Reloc,
                                          uint8_t *Instr, uint8_t Size) {
  std::memcpy((void *)(Data + Offset), Instr, Size);
  return true;
}

void RegionFragmentEx::deleteInstruction(uint32_t DeleteOffset,
                                         uint32_t DeleteSize) {

  // Fixup relocations.
  for (auto &Reloc : getOwningSection()->getRelocations()) {
    // Get the source offset of the relocation
    FragmentRef *Ref = Reloc->targetRef();
    FragmentRef::Offset Off = Ref->offset();
    if (Off > DeleteOffset && Off < Size)
      Ref->setOffset(Off - DeleteSize);
  }

  // Fixup symbols.
  for (ResolveInfo *Info : Symbols) {
    FragmentRef *Ref = Info->outSymbol()->fragRef();
    FragmentRef::Offset Off = Ref->offset();
    if (Off > DeleteOffset && Off <= Size)
      Ref->setOffset(Off - DeleteSize);
    uint32_t SymbolSize = Info->outSymbol()->size();
    // If the symbol falls in between where we are deleting instructions
    // and where the symbol is actually pointing, update symbol size
    // ...
    // ... ==> symbol offset
    // ...
    // ... ===> delete offset
    // ...
    // ... ===> symbol size
    if (!Info->isSection() && (DeleteOffset >= Off) &&
        ((DeleteOffset - Off) < SymbolSize))
      Info->outSymbol()->setSize(SymbolSize - DeleteSize);
  }

  std::memmove((void *)(Data + DeleteOffset),
               (void *)(Data + DeleteOffset + DeleteSize),
               Size - DeleteOffset - DeleteSize);

  Size = Size - DeleteSize;
}

void RegionFragmentEx::insertInstruction(uint32_t InsertOffset,
                                         uint32_t InsertSize) {
#ifndef NDEBUG
  assert(Size + InsertSize <= Capacity &&
         "insertInstruction would exceed original buffer allocation");
#endif
  std::memmove((void *)(Data + InsertOffset + InsertSize),
               (void *)(Data + InsertOffset), Size - InsertOffset);
  std::memset((void *)(Data + InsertOffset), 0, InsertSize);
  Size = Size + InsertSize;

  // Shift relocation offsets that are at or after the insertion point.
  for (auto &Reloc : getOwningSection()->getRelocations()) {
    FragmentRef *Ref = Reloc->targetRef();
    FragmentRef::Offset Off = Ref->offset();
    if (Off >= InsertOffset && Off < Size)
      Ref->setOffset(Off + InsertSize);
  }

  // Shift symbol offsets similarly.
  for (ResolveInfo *Info : Symbols) {
    FragmentRef *Ref = Info->outSymbol()->fragRef();
    FragmentRef::Offset Off = Ref->offset();
    if (Off >= InsertOffset && Off <= Size)
      Ref->setOffset(Off + InsertSize);
  }
}

size_t RegionFragmentEx::size() const { return Size; }

eld::Expected<void> RegionFragmentEx::emit(MemoryRegion &Mr, Module &M) {
  uint8_t *Out = Mr.begin() + getOffset(M.getConfig().getDiagEngine());
  memcpy(Out, getRegion().begin(), Size);
  return {};
}

void RegionFragmentEx::copyData(void *PDest, uint32_t PNBytes,
                                uint64_t POffset) const {
  std::memcpy(PDest, this->getRegion().begin() + POffset, PNBytes);
}

void RegionFragmentEx::addSymbol(ResolveInfo *R) { Symbols.push_back(R); }

void RegionFragmentEx::addRequiredNops(uint32_t Offset, uint32_t NumNopsToAdd) {
  uint32_t I = 0;
  uint32_t NOP = 0x13;
  unsigned short CNOP = 0x1;
  for (I = 0; I < (NumNopsToAdd & -4); I += 4)
    std::memcpy((void *)(Data + Offset + I), &NOP, sizeof(NOP));
  if (NumNopsToAdd % 4)
    std::memcpy((void *)(Data + Offset + I), &CNOP, sizeof(CNOP));
}
