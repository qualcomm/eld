//===- EhFrameFragment.cpp-------------------------------------------------===//
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


#include "eld/Fragment/EhFrameFragment.h"
#include "eld/Core/Module.h"
#include "eld/Input/InputFile.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Readers/Relocation.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Twine.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/Support/Endian.h"
using namespace eld;

EhFramePiece &EhFrameFragment::addPiece(size_t Off, size_t Sz, Relocation *R) {
  Pieces.emplace_back(Off, Sz, R);
  return Pieces.back();
}

eld::Expected<void> FDEPiece::emit(MemoryRegion &Mr, Module &M,
                                   const EhFrameFragment &F) {
  assert(FDE.hasOutputOffset() && "FDE output offset must be assigned");
  uint8_t *Buf = Mr.begin() + FDE.getOutputOffset();
  memcpy(Buf, getContent(F).data(), size());
  llvm::support::endian::write32le(Buf, FDE.getSize() - 4);
  return {};
}

size_t EhFrameCIE::size() const {
  if (!FDEs.size())
    return 0;
  uint32_t Off = CIE.getSize();
  for (auto &F : FDEs)
    Off += F->size();
  return Off;
}

llvm::ArrayRef<uint8_t> EhFrameCIE::getContent(const EhFrameFragment &F) const {
  return CIE.getData(F.getRegion());
}

void EhFrameCIE::setOffset(uint32_t CieOff) {
  if (!FDEs.size())
    return;
  uint32_t Off = CieOff + CIE.getSize();
  for (auto *F : FDEs) {
    F->getFDE().setOutputOffset(Off);
    Off += F->size();
  }
  CIE.setOutputOffset(CieOff);
}

eld::Expected<void> EhFrameCIE::emit(MemoryRegion &Mr, Module &M,
                                     uint32_t CieOff,
                                     const EhFrameFragment &F) {
  if (!FDEs.size())
    return {};
  uint8_t *Buf = Mr.begin() + CieOff;
  memcpy(Buf, getContent(F).data(), CIE.getSize());
  llvm::support::endian::write32le(Buf, CIE.getSize() - 4);
  for (auto *Fde : FDEs) {
    auto ExpEmit = Fde->emit(Mr, M, F);
    if (!ExpEmit)
      return ExpEmit;
    uint32_t FdeOff = Fde->getFDE().getOutputOffset();
    uint8_t *FDEBuf = Mr.begin() + FdeOff;
    llvm::support::endian::write32le(FDEBuf + 4, FdeOff + 4 - CieOff);
  }
  return {};
}

uint8_t EhFrameCIE::getFdeEncoding(const EhFrameFragment &F, bool Is64Bit,
                                   DiagnosticEngine *DiagEngine) {
  return CIE.getFdeEncoding(F.getRegion(), Is64Bit, DiagEngine,
                            *F.getOwningSection());
}

size_t EhFrameFragment::size() const {
  if (CIEs.empty())
    return RegionFragment::size();
  size_t Total = 0;
  for (auto *C : CIEs)
    Total += C->size();
  return Total;
}

void EhFrameFragment::setOffset(uint32_t O) {
  Fragment::setOffset(O);
  if (CIEs.empty())
    return;
  uint32_t Off = O;
  for (auto *C : CIEs) {
    if (!C->size())
      continue;
    C->setOffset(Off);
    Off += C->size();
  }
}

eld::Expected<void> EhFrameFragment::emit(MemoryRegion &Mr, Module &M) {
  if (CIEs.empty())
    return RegionFragment::emit(Mr, M);
  uint32_t Off = getOffset(M.getConfig().getDiagEngine());
  for (auto *C : CIEs) {
    if (!C->size())
      continue;
    auto ExpEmit = C->emit(Mr, M, Off, *this);
    if (!ExpEmit)
      return ExpEmit;
    Off += C->size();
  }
  return {};
}

void EhFrameFragment::dump(llvm::raw_ostream &OS) {
  if (Pieces.empty() && CIEs.empty())
    return;

  auto PrintRelocSym = [&](const EhFramePiece &P) {
    if (Relocation *R = P.getRelocation()) {
      if (R->symInfo())
        OS << "\tsym=" << R->symInfo()->name();
    }
  };

  if (!CIEs.empty()) {
    for (const EhFrameCIE *C : CIEs) {
      const EhFramePiece &CiePiece = C->getCIE();
      OS << "#\tCIE";
      if (CiePiece.hasOutputOffset()) {
        OS << "\tout=0x";
        OS.write_hex(CiePiece.getOutputOffset());
      } else {
        OS << "\tout=<unassigned>";
      }
      OS << "\tsize=0x";
      OS.write_hex(CiePiece.getSize());
      OS << "\tin=0x";
      OS.write_hex(CiePiece.getOffset());
      OS << "\tfdes=" << C->getNumFDE();
      PrintRelocSym(CiePiece);
      OS << "\n";

      for (const FDEPiece *F : C->getFDEs()) {
        const EhFramePiece &FdePiece = F->getFDE();
        OS << "#\tFDE";
        if (FdePiece.hasOutputOffset()) {
          OS << "\tout=0x";
          OS.write_hex(FdePiece.getOutputOffset());
        } else {
          OS << "\tout=<unassigned>";
        }
        OS << "\tsize=0x";
        OS.write_hex(FdePiece.getSize());
        OS << "\tin=0x";
        OS.write_hex(FdePiece.getOffset());
        PrintRelocSym(FdePiece);
        OS << "\n";
      }
    }
    return;
  }

  llvm::StringRef Region = getRegion();
  for (const EhFramePiece &P : Pieces) {
    OS << "#\tREC";
    if (P.hasOutputOffset()) {
      OS << "\tout=0x";
      OS.write_hex(P.getOutputOffset());
    } else {
      OS << "\tout=<unassigned>";
    }
    OS << "\tsize=0x";
    OS.write_hex(P.getSize());
    OS << "\tin=0x";
    OS.write_hex(P.getOffset());

    if (P.getSize() == 4) {
      OS << "\tEND";
      PrintRelocSym(P);
      OS << "\n";
      continue;
    }

    if (P.getSize() >= 8 && P.getOffset() + 8 <= Region.size()) {
      uint32_t ID = llvm::support::endian::read32le(
          reinterpret_cast<const uint8_t *>(Region.data()) + P.getOffset() + 4);
      OS << (ID == 0 ? "\tCIE" : "\tFDE");
    }
    PrintRelocSym(P);
    OS << "\n";
  }
}

llvm::ArrayRef<uint8_t> EhFramePiece::getData(llvm::StringRef Region) const {
  return {reinterpret_cast<const uint8_t *>(Region.data()) + MOffset, ThisSize};
}

// Read a byte and advance D by one byte.
// EhFramePiece and EhFrameHdrFragment can take diagnostics.
uint8_t EhFramePiece::readByte(DiagnosticEngine *DiagEngine,
                               const ELFSection &S) {
  if (D.empty()) {
    DiagEngine->raise(Diag::eh_frame_read_error)
        << "Unexpected end of CIE"
        << S.getInputFile()->getInput()->decoratedPath();
    return 0;
  }
  uint8_t B = D.front();
  D = D.slice(1);
  return B;
}

void EhFramePiece::skipBytes(size_t Count, DiagnosticEngine *DiagEngine,
                             const ELFSection &S) {
  if (D.size() < Count) {
    DiagEngine->raise(Diag::eh_frame_read_error)
        << "CIE is too small" << S.getInputFile()->getInput()->decoratedPath();
    return;
  }
  D = D.slice(Count);
}

// Read a null-terminated string.
llvm::StringRef EhFramePiece::readString(DiagnosticEngine *DiagEngine,
                                         const ELFSection &S) {
  const uint8_t *End = std::find(D.begin(), D.end(), '\0');
  if (End == D.end()) {
    DiagEngine->raise(Diag::eh_frame_read_error)
        << "corrupted CIE (failed to read string)"
        << S.getInputFile()->getInput()->decoratedPath();
    return llvm::StringRef("");
  }
  llvm::StringRef Str = llvm::toStringRef(D.slice(0, End - D.begin()));
  D = D.slice(Str.size() + 1);
  return Str;
}

// Skip an integer encoded in the LEB128 format.
// Actual number is not of interest because only the runtime needs it.
// But we need to be at least able to skip it so that we can read
// the field that follows a LEB128 number.
void EhFramePiece::skipLeb128(DiagnosticEngine *DiagEngine,
                              const ELFSection &S) {
  while (!D.empty()) {
    uint8_t Val = D.front();
    D = D.slice(1);
    if ((Val & 0x80) == 0)
      return;
  }
  DiagEngine->raise(Diag::eh_frame_read_error)
      << "corrupted CIE (failed to read LEB128)"
      << S.getInputFile()->getInput()->decoratedPath();
}

size_t EhFramePiece::getAugPSize(unsigned Enc, bool Is64Bit,
                                 DiagnosticEngine *DiagEngine) {
  switch (Enc & 0x0f) {
  case llvm::dwarf::DW_EH_PE_absptr:
  case llvm::dwarf::DW_EH_PE_signed:
    return Is64Bit ? 8 : 4;
  case llvm::dwarf::DW_EH_PE_udata2:
  case llvm::dwarf::DW_EH_PE_sdata2:
    return 2;
  case llvm::dwarf::DW_EH_PE_udata4:
  case llvm::dwarf::DW_EH_PE_sdata4:
    return 4;
  case llvm::dwarf::DW_EH_PE_udata8:
  case llvm::dwarf::DW_EH_PE_sdata8:
    return 8;
  }
  return 0;
}

void EhFramePiece::skipAugP(bool Is64Bit, DiagnosticEngine *DiagEngine,
                            const ELFSection &S) {
  uint8_t Enc = readByte(DiagEngine, S);
  if ((Enc & 0xf0) == llvm::dwarf::DW_EH_PE_aligned) {
    DiagEngine->raise(Diag::eh_frame_read_error)
        << "llvm::dwarf::DW_EH_PE_aligned encoding is not supported"
        << S.getInputFile()->getInput()->decoratedPath();
    return;
  }
  size_t Size = getAugPSize(Enc, Is64Bit, DiagEngine);
  if (Size == 0) {
    DiagEngine->raise(Diag::eh_frame_read_error)
        << "unknown FDE encoding"
        << S.getInputFile()->getInput()->decoratedPath();
    return;
  }
  if (Size >= D.size()) {
    DiagEngine->raise(Diag::eh_frame_read_error)
        << "corrupted CIE (failed to skipAugP)"
        << S.getInputFile()->getInput()->decoratedPath();
    return;
  }
  D = D.slice(Size);
}

uint8_t EhFramePiece::getFdeEncoding(llvm::StringRef Region, bool Is64Bit,
                                     DiagnosticEngine *DiagEngine,
                                     const ELFSection &S) {
  D = getData(Region);
  skipBytes(8, DiagEngine, S);
  int Version = readByte(DiagEngine, S);
  if (Version != 1 && Version != 3) {
    DiagEngine->raise(Diag::eh_frame_read_error)
        << "FDE version 1 or 3 expected, but got " + llvm::Twine(Version).str()
        << S.getInputFile()->getInput()->decoratedPath();
    return -1;
  }

  llvm::StringRef Aug = readString(DiagEngine, S);

  // Skip code and data alignment factors.
  skipLeb128(DiagEngine, S);
  skipLeb128(DiagEngine, S);

  // Skip the return address register. In CIE version 1 this is a single
  // byte. In CIE version 3 this is an unsigned LEB128.
  if (Version == 1)
    readByte(DiagEngine, S);
  else
    skipLeb128(DiagEngine, S);

  // We only care about an 'R' value, but other records may precede an 'R'
  // record. Unfortunately records are not in TLV (type-length-value) format,
  // so we need to teach the linker how to skip records for each type.
  for (char C : Aug) {
    if (C == 'R')
      return readByte(DiagEngine, S);
    if (C == 'z')
      skipLeb128(DiagEngine, S);
    else if (C == 'P')
      skipAugP(Is64Bit, DiagEngine, S);
    else if (C == 'L')
      readByte(DiagEngine, S);
    else if (C != 'B' && C != 'S') {
      DiagEngine->raise(Diag::eh_frame_read_error)
          << "unknown .eh_frame augmentation string: " + std::string(Aug)
          << S.getInputFile()->getInput()->decoratedPath();
    }
  }
  return llvm::dwarf::DW_EH_PE_absptr;
}
