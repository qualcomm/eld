//===- EhFrameHdrFragment.cpp----------------------------------------------===//
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


#include "eld/Fragment/EhFrameHdrFragment.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Fragment/EhFrameFragment.h"
#include "eld/Readers/EhFrameHdrSection.h"
#include "llvm/ADT/Twine.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/Support/Endian.h"
#include <algorithm>

using namespace eld;

//
// EhFrameHdr
//
EhFrameHdrFragment::EhFrameHdrFragment(EhFrameHdrSection *P, bool CreateTable,
                                       bool Is64bit)
    : Fragment(Fragment::EhFrameHdr, P, P->getAddrAlign()), Is64Bit(Is64bit),
      CreateTable(CreateTable) {}

const std::string EhFrameHdrFragment::name() const { return "EhFrameHdr"; }

size_t EhFrameHdrFragment::size() const {
  EhFrameHdrSection *S =
      llvm::dyn_cast<eld::EhFrameHdrSection>(getOwningSection());
  if (!CreateTable)
    return S->sizeOfHeader();
  return S->sizeOfHeader() + S->getNumFDE() * 8;
}

void EhFrameHdrFragment::dump(llvm::raw_ostream &OS) {}

EhFrameHdrFragment::~EhFrameHdrFragment() {}

eld::Expected<void> EhFrameHdrFragment::emit(MemoryRegion &Mr, Module &M) {
  std::vector<FdeData> Fdes;
  uint8_t *Buf = Mr.begin() +
                 getOwningSection()->getOutputELFSection()->offset() +
                 getOffset(M.getConfig().getDiagEngine());
  Buf[0] = 1;
  Buf[1] = llvm::dwarf::DW_EH_PE_pcrel | llvm::dwarf::DW_EH_PE_sdata4;

  if (CreateTable) {
    Fdes = getFdeData(Mr.begin(), M.getConfig().getDiagEngine());
    Buf[2] = llvm::dwarf::DW_EH_PE_udata4;
    Buf[3] = llvm::dwarf::DW_EH_PE_datarel | llvm::dwarf::DW_EH_PE_sdata4;
  } else {
    Buf[2] = llvm::dwarf::DW_EH_PE_omit;
    Buf[3] = llvm::dwarf::DW_EH_PE_omit;
  }

  EhFrameHdrSection *S =
      llvm::dyn_cast<eld::EhFrameHdrSection>(getOwningSection());
  if (!S->getNumCIE())
    return {};

  llvm::support::endian::write32le(
      Buf + 4,
      S->getCIEs().front()->getOwningSection()->getOutputELFSection()->addr() -
          getOwningSection()->getOutputELFSection()->addr() - 4);
  llvm::support::endian::write32le(Buf + 8, Fdes.size());
  Buf += 12;

  for (FdeData &Fde : Fdes) {
    llvm::support::endian::write32le(Buf, Fde.PcRel);
    llvm::support::endian::write32le(Buf + 4, Fde.FdeVARel);
    Buf += 8;
  }
  return {};
}

std::vector<EhFrameHdrFragment::FdeData>
EhFrameHdrFragment::getFdeData(uint8_t *Data, DiagnosticEngine *DiagEngine) {
  std::vector<FdeData> Ret;
  uint64_t VA = getOwningSection()->getOutputELFSection()->addr();
  for (CIEFragment *CIE :
       llvm::dyn_cast<eld::EhFrameHdrSection>(getOwningSection())->getCIEs()) {
    uint8_t Enc = CIE->getFdeEncoding(Is64Bit, DiagEngine);
    for (FDEFragment *FDE : CIE->getFDEs()) {
      uint64_t PC = getFdePc(Data, FDE, Enc, DiagEngine);
      uint64_t FdeVA = FDE->getOwningSection()->getOutputELFSection()->addr() +
                       FDE->getOffset(DiagEngine);
      // With noinhbit-exec there may be an issue that the VA might not fit,
      // just because the symbol is not resolved.
      if (!llvm::isInt<32>(PC - VA)) {
        DiagEngine->raise(Diag::eh_frame_read_warn)
            << "PC Offset is too large in " +
                   std::string(FDE->getOwningSection()->name())
            << FDE->getOwningSection()
                   ->getInputFile()
                   ->getInput()
                   ->decoratedPath();
      }
      Ret.push_back({uint32_t(PC - VA), uint32_t(FdeVA - VA)});
    }
  }

  // Sort the FDE list by their PC and uniqueify. Usually there is only
  // one FDE for a PC (i.e. function), but if ICF merges two functions
  // into one, there can be more than one FDEs pointing to the address.
  auto Less = [](const FdeData &A, const FdeData &B) {
    return A.PcRel < B.PcRel;
  };

  std::stable_sort(Ret.begin(), Ret.end(), Less);
  auto Eq = [](const FdeData &A, const FdeData &B) {
    return A.PcRel == B.PcRel;
  };

  Ret.erase(std::unique(Ret.begin(), Ret.end(), Eq), Ret.end());

  return Ret;
}

uint64_t EhFrameHdrFragment::readFdeAddr(uint8_t *Buf, int Size,
                                         DiagnosticEngine *DiagEngine) {
  switch (Size) {
  case llvm::dwarf::DW_EH_PE_udata2:
    return llvm::support::endian::read16le(Buf);
  case llvm::dwarf::DW_EH_PE_sdata2:
    return (int16_t)llvm::support::endian::read16le(Buf);
  case llvm::dwarf::DW_EH_PE_udata4:
    return llvm::support::endian::read32le(Buf);
  case llvm::dwarf::DW_EH_PE_sdata4:
    return (int32_t)llvm::support::endian::read32le(Buf);
  case llvm::dwarf::DW_EH_PE_udata8:
  case llvm::dwarf::DW_EH_PE_sdata8:
    return llvm::support::endian::read64le(Buf);
  case llvm::dwarf::DW_EH_PE_absptr:
    if (Is64Bit)
      return llvm::support::endian::read64le(Buf);
    else
      return llvm::support::endian::read32le(Buf);
  }
  DiagEngine->raise(Diag::eh_frame_read_error) << "unsupported"
                                               << "eh_frame_hdr";
  return 0;
}

// Returns the VA to which a given FDE (on a mmap'ed buffer) is applied to.
// We need it to create .eh_frame_hdr section.
uint64_t EhFrameHdrFragment::getFdePc(uint8_t *Buf, FDEFragment *F, uint8_t Enc,
                                      DiagnosticEngine *DiagEngine) {
  // The starting address to which this FDE applies is
  // stored at FDE + 8 byte.
  ELFSection *S = F->getOwningSection()->getOutputELFSection();
  size_t Off = S->offset() + F->getOffset(DiagEngine) + 8;
  uint64_t Addr = readFdeAddr(Buf + Off, Enc & 0xf, DiagEngine);
  if ((Enc & 0x70) == llvm::dwarf::DW_EH_PE_absptr)
    return Addr;
  if ((Enc & 0x70) == llvm::dwarf::DW_EH_PE_pcrel) {
    if (Is64Bit)
      return (int64_t)Addr + S->addr() + F->getOffset(DiagEngine) + 8;
    return (int32_t)Addr + S->addr() + F->getOffset(DiagEngine) + 8;
  }
  return 0;
}
