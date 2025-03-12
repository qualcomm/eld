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
#include "eld/Readers/EhFrameSection.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Twine.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/Support/Endian.h"
using namespace eld;

//
// FDE
//
FDEFragment::FDEFragment(EhFramePiece &P, EhFrameSection *O)
    : Fragment(Fragment::FDE, O, 1), FDE(P) {}

const std::string FDEFragment::name() const { return "FDE"; }

size_t FDEFragment::size() const { return FDE.getSize(); }

llvm::ArrayRef<uint8_t> FDEFragment::getContent() const {
  return FDE.getData();
}

void FDEFragment::dump(llvm::raw_ostream &OS) {}

FDEFragment::~FDEFragment() {}

eld::Expected<void> FDEFragment::emit(MemoryRegion &mr, Module &M) {
  uint8_t *Buf = mr.begin() + getOffset(M.getConfig().getDiagEngine());
  memcpy(Buf, getContent().data(), size());
  llvm::support::endian::write32le(Buf, FDE.getSize() - 4);
  return {};
}

//
// CIE
//

CIEFragment::CIEFragment(EhFramePiece &P, EhFrameSection *O)
    : Fragment(Fragment::CIE, O, 1), CIE(P) {}

const std::string CIEFragment::name() const { return "CIE"; }

size_t CIEFragment::size() const {
  if (!FDEs.size())
    return 0;
  uint32_t Off = CIE.getSize();
  for (auto &F : FDEs)
    Off += F->size();
  return Off;
}

llvm::ArrayRef<uint8_t> CIEFragment::getContent() const {
  return CIE.getData();
}

void CIEFragment::dump(llvm::raw_ostream &OS) {}

CIEFragment::~CIEFragment() {}

void CIEFragment::setOffset(uint32_t O) {
  Fragment::setOffset(O);
  if (!FDEs.size())
    return;
  uint32_t Off = getOffset() + CIE.getSize();
  for (auto &F : FDEs) {
    F->setOffset(Off);
    F->getFDE().setOutputOffset(F->getOffset());
    Off = F->getOffset() + F->size();
  }
  CIE.setOutputOffset(getOffset());
}

eld::Expected<void> CIEFragment::emit(MemoryRegion &mr, Module &M) {
  if (!FDEs.size())
    return {};
  uint8_t *Buf = mr.begin() + getOffset(M.getConfig().getDiagEngine());
  memcpy(Buf, getContent().data(), CIE.getSize());
  llvm::support::endian::write32le(Buf, CIE.getSize() - 4);
  for (auto &F : FDEs) {
    F->emit(mr, M);
    uint8_t *FDEBuf = mr.begin() + F->getOffset(M.getConfig().getDiagEngine());
    llvm::support::endian::write32le(
        FDEBuf + 4, F->getOffset(M.getConfig().getDiagEngine()) + 4 -
                        this->getOffset(M.getConfig().getDiagEngine()));
  }
  return {};
}

uint8_t CIEFragment::getFdeEncoding(bool is64Bit,
                                    DiagnosticEngine *DiagEngine) {
  return CIE.getFdeEncoding(is64Bit, DiagEngine);
}

//
// EhFrameFragment
//
llvm::ArrayRef<uint8_t> EhFramePiece::getData() {
  return {m_Section->getData().data() + m_Offset, m_Size};
}

// Read a byte and advance D by one byte.
// EhFramePiece and EhFrameHdrFragment can take diagnostics.
uint8_t EhFramePiece::readByte(DiagnosticEngine *DiagEngine) {
  if (D.empty()) {
    DiagEngine->raise(diag::eh_frame_read_error)
        << "Unexpected end of CIE"
        << m_Section->getInputFile()->getInput()->decoratedPath();
    return 0;
  }
  uint8_t B = D.front();
  D = D.slice(1);
  return B;
}

void EhFramePiece::skipBytes(size_t Count, DiagnosticEngine *DiagEngine) {
  if (D.size() < Count) {
    DiagEngine->raise(diag::eh_frame_read_error)
        << "CIE is too small"
        << m_Section->getInputFile()->getInput()->decoratedPath();
    return;
  }
  D = D.slice(Count);
}

// Read a null-terminated string.
llvm::StringRef EhFramePiece::readString(DiagnosticEngine *DiagEngine) {
  const uint8_t *End = std::find(D.begin(), D.end(), '\0');
  if (End == D.end()) {
    DiagEngine->raise(diag::eh_frame_read_error)
        << "corrupted CIE (failed to read string)"
        << m_Section->getInputFile()->getInput()->decoratedPath();
    return llvm::StringRef("");
  }
  llvm::StringRef S = llvm::toStringRef(D.slice(0, End - D.begin()));
  D = D.slice(S.size() + 1);
  return S;
}

// Skip an integer encoded in the LEB128 format.
// Actual number is not of interest because only the runtime needs it.
// But we need to be at least able to skip it so that we can read
// the field that follows a LEB128 number.
void EhFramePiece::skipLeb128(DiagnosticEngine *DiagEngine) {
  while (!D.empty()) {
    uint8_t Val = D.front();
    D = D.slice(1);
    if ((Val & 0x80) == 0)
      return;
  }
  DiagEngine->raise(diag::eh_frame_read_error)
      << "corrupted CIE (failed to read LEB128)"
      << m_Section->getInputFile()->getInput()->decoratedPath();
}

size_t EhFramePiece::getAugPSize(unsigned Enc, bool is64Bit,
                                 DiagnosticEngine *DiagEngine) {
  switch (Enc & 0x0f) {
  case llvm::dwarf::DW_EH_PE_absptr:
  case llvm::dwarf::DW_EH_PE_signed:
    return is64Bit ? 8 : 4;
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

void EhFramePiece::skipAugP(bool is64Bit, DiagnosticEngine *DiagEngine) {
  uint8_t Enc = readByte(DiagEngine);
  if ((Enc & 0xf0) == llvm::dwarf::DW_EH_PE_aligned) {
    DiagEngine->raise(diag::eh_frame_read_error)
        << "llvm::dwarf::DW_EH_PE_aligned encoding is not supported"
        << m_Section->getInputFile()->getInput()->decoratedPath();
    return;
  }
  size_t Size = getAugPSize(Enc, is64Bit, DiagEngine);
  if (Size == 0) {
    DiagEngine->raise(diag::eh_frame_read_error)
        << "unknown FDE encoding"
        << m_Section->getInputFile()->getInput()->decoratedPath();
    return;
  }
  if (Size >= D.size()) {
    DiagEngine->raise(diag::eh_frame_read_error)
        << "corrupted CIE (failed to skipAugP)"
        << m_Section->getInputFile()->getInput()->decoratedPath();
    return;
  }
  D = D.slice(Size);
}

uint8_t EhFramePiece::getFdeEncoding(bool is64Bit,
                                     DiagnosticEngine *DiagEngine) {
  D = getData();
  skipBytes(8, DiagEngine);
  int Version = readByte(DiagEngine);
  if (Version != 1 && Version != 3) {
    DiagEngine->raise(diag::eh_frame_read_error)
        << "FDE version 1 or 3 expected, but got " + llvm::Twine(Version).str()
        << m_Section->getInputFile()->getInput()->decoratedPath();
    return -1;
  }

  llvm::StringRef Aug = readString(DiagEngine);

  // Skip code and data alignment factors.
  skipLeb128(DiagEngine);
  skipLeb128(DiagEngine);

  // Skip the return address register. In CIE version 1 this is a single
  // byte. In CIE version 3 this is an unsigned LEB128.
  if (Version == 1)
    readByte(DiagEngine);
  else
    skipLeb128(DiagEngine);

  // We only care about an 'R' value, but other records may precede an 'R'
  // record. Unfortunately records are not in TLV (type-length-value) format,
  // so we need to teach the linker how to skip records for each type.
  for (char C : Aug) {
    if (C == 'R')
      return readByte(DiagEngine);
    if (C == 'z')
      skipLeb128(DiagEngine);
    else if (C == 'P')
      skipAugP(is64Bit, DiagEngine);
    else if (C == 'L')
      readByte(DiagEngine);
    else if (C != 'B' && C != 'S') {
      DiagEngine->raise(diag::eh_frame_read_error)
          << "unknown .eh_frame augmentation string: " + std::string(Aug)
          << m_Section->getInputFile()->getInput()->decoratedPath();
    }
  }
  return llvm::dwarf::DW_EH_PE_absptr;
}
