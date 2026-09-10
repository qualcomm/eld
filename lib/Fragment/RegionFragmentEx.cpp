//===- RegionFragmentEx.cpp------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#include "eld/Fragment/RegionFragmentEx.h"
#include "eld/Core/Module.h"
#include "eld/Readers/ELFSection.h"
#include <cassert>

using namespace eld;

//===----------------------------------------------------------------------===//
// RegionFragmentEx
//===----------------------------------------------------------------------===//
RegionFragmentEx::RegionFragmentEx(const char *Buf, size_t Sz, ELFSection *O,
                                   uint32_t Align)
    : Fragment(Fragment::Type::RegionFragmentEx, O, Align), Data(Buf),
      Size(Sz) {}

RegionFragmentEx::~RegionFragmentEx() {}

bool RegionFragmentEx::replaceInstruction(uint32_t Offset, Relocation *Reloc,
                                          uint8_t *Instr, uint8_t Size) {
  std::memcpy((void *)(Data + Offset), Instr, Size);
  return true;
}

size_t RegionFragmentEx::size() const { return Size - Plan.totalShift(); }

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

void RegionFragmentEx::recordDelete(Relocation *Reloc, uint32_t DeleteOffset,
                                    uint32_t DeleteBytes, uint32_t NopBytes) {
  RelaxEdit E;
  E.Reloc = Reloc;
  E.OrigOffset = DeleteOffset;
  E.DeleteOffset = DeleteOffset;
  E.DeleteBytes = DeleteBytes;
  E.NopBytes = NopBytes;
  Plan.addEdit(E);
}

void RegionFragmentEx::attachDecision(Relocation *Reloc,
                                      Relocation::Type NewType,
                                      std::optional<Relocation::DWord> NewInstr,
                                      uint32_t InstrOffset,
                                      uint8_t NewInstrSize) {
  RelaxEdit *E = Plan.find(Reloc);
  assert(E && "attachDecision requires an edit already recorded for Reloc");
  E->NewType = NewType;
  if (NewInstr) {
    E->OrigOffset = InstrOffset;
    E->NewInstr = NewInstr;
    E->NewInstrSize = NewInstrSize;
  }
}

void RegionFragmentEx::recordRewrite(
    Relocation *Reloc, uint32_t InstrOffset, Relocation::Type NewType,
    Relocation::DWord NewInstr, uint8_t NewInstrSize,
    std::optional<Relocation::Address> NewAddend) {
  RelaxEdit E;
  E.Reloc = Reloc;
  E.OrigOffset = InstrOffset;
  E.DeleteOffset = InstrOffset;
  E.DeleteBytes = 0;
  E.NewType = NewType;
  E.NewInstr = NewInstr;
  E.NewInstrSize = NewInstrSize;
  E.NewAddend = NewAddend;
  Plan.addEdit(E);
}

void RegionFragmentEx::dropEdit(const Relocation *Reloc) {
  Plan.removeEdit(Reloc);
}

void RegionFragmentEx::commitRelaxEdits() {
  if (Plan.empty())
    return;

  // Encode before compacting, while OrigOffset still addresses the
  // untouched buffer. Relocation::target() is a separate cache from these
  // bytes, so it needs updating too.
  for (const RelaxEdit &E : Plan.edits()) {
    if (E.NewInstr) {
      std::memcpy((void *)(Data + E.OrigOffset), &*E.NewInstr, E.NewInstrSize);
      E.Reloc->setTargetData(*E.NewInstr);
    }
  }

  size_t OrigSize = Size;

  for (auto &Reloc : getOwningSection()->getRelocations()) {
    FragmentRef *Ref = Reloc->targetRef();
    FragmentRef::Offset Off = Ref->offset();
    if (Off < OrigSize) {
      uint32_t Shift = Plan.shiftAt(Off);
      if (Shift)
        Ref->setOffset(Off - Shift);
    }
  }

  for (ResolveInfo *Info : Symbols) {
    FragmentRef *Ref = Info->outSymbol()->fragRef();
    FragmentRef::Offset Off = Ref->offset();
    uint32_t SymbolSize = Info->outSymbol()->size();
    if (!Info->isSection()) {
      for (const RelaxEdit &E : Plan.edits()) {
        if (E.DeleteOffset >= Off && (E.DeleteOffset - Off) < SymbolSize) {
          SymbolSize -= E.DeleteBytes;
          Info->outSymbol()->setSize(SymbolSize);
        }
      }
    }
    if (Off <= OrigSize) {
      uint32_t Shift = Plan.shiftAt(Off);
      if (Shift)
        Ref->setOffset(Off - Shift);
    }
  }

  // One forward walk instead of one memmove per edit.
  uint32_t Dst = 0;
  uint32_t Src = 0;
  for (const RelaxEdit &E : Plan.edits()) {
    std::memmove((void *)(Data + Dst), (void *)(Data + Src),
                 E.DeleteOffset - Src);
    Dst += E.DeleteOffset - Src;
    Src = E.DeleteOffset + E.DeleteBytes;
  }
  std::memmove((void *)(Data + Dst), (void *)(Data + Src), OrigSize - Src);
  Size = OrigSize - Plan.totalShift();

  for (const RelaxEdit &E : Plan.edits()) {
    if (E.NewType)
      E.Reloc->setType(*E.NewType);
    if (E.NewAddend)
      E.Reloc->setAddend(*E.NewAddend);
  }

  Plan.clear();
}
