//===- EhFrameSection.cpp--------------------------------------------------===//
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


#include "eld/Readers/EhFrameSection.h"
#include "eld/Diagnostics/DiagnosticPrinter.h"
#include "eld/Diagnostics/MsgHandler.h"
#include "eld/Fragment/EhFrameFragment.h"
#include "eld/Fragment/RegionFragment.h"
#include "eld/Readers/Relocation.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/Endian.h"

using namespace eld;

namespace {
// CIE records are uniquified by their contents and personality functions
llvm::DenseMap<std::pair<llvm::ArrayRef<uint8_t>, ResolveInfo *>, CIEFragment *>
    CieMap;
} // namespace

EhFrameSection::EhFrameSection(std::string Name, DiagnosticEngine *E,
                               uint32_t Type, uint32_t Flags, uint32_t EntSize,
                               uint64_t Size)
    : ELFSection(Section::Kind::EhFrame, LDFileFormat::EhFrame, Name, Flags,
                 EntSize, /*AddrAlign=*/0, Type, /*Info=*/0, /*Link=*/nullptr,
                 Size, /*PAddr=*/0),
      m_DiagEngine(E) {}

size_t EhFrameSection::readEhRecordSize(size_t Off) {
  llvm::ArrayRef<uint8_t> D = Data.slice(Off);

  if (D.size() < 4) {
    m_DiagEngine->raise(diag::eh_frame_read_error)
        << "CIE/FDE too small" << getInputFile()->getInput()->decoratedPath();
    return -1;
  }

  // First 4 bytes of CIE/FDE is the size of the record.
  // If it is 0xFFFFFFFF, the next 8 bytes contain the size instead,
  // but we do not support that format yet.
  uint64_t V = llvm::support::endian::read32le(D.data());
  if (V == UINT32_MAX) {
    m_DiagEngine->raise(diag::eh_frame_read_error)
        << "CIE/FDE too large" << getInputFile()->getInput()->decoratedPath();
    return -1;
  }
  uint64_t Size = V + 4;
  if (Size > D.size()) {
    m_DiagEngine->raise(diag::eh_frame_read_error)
        << "CIE/FDE ends past the end of the section"
        << getInputFile()->getInput()->decoratedPath();
    return -1;
  }
  return Size;
}

bool EhFrameSection::splitEhFrameSection() {
  if (!size())
    return true;

  if (m_EhFrame == nullptr) {
    m_EhFrame = llvm::dyn_cast<eld::RegionFragment>(Fragments.front());
    Data = llvm::ArrayRef((const uint8_t *)m_EhFrame->getRegion().data(),
                          m_EhFrame->size());
    Fragments.clear();
  }

  if (!m_EhFrame)
    return false;

  llvm::StringRef Data = m_EhFrame->getRegion();
  for (size_t Off = 0, End = Data.size(); Off != End;) {
    size_t Size = readEhRecordSize(Off);
    if (Size == (size_t)-1)
      return false;

    Relocation *Reloc = getReloc(Off, Size);
    m_EhFramePieces.emplace_back(Off, Size, Reloc, this);
    if (Reloc && getDiagPrinter()->isVerbose())
      m_DiagEngine->raise(diag::verbose_ehframe)
          << Reloc->symInfo()->name() << Off << Size;
    // The empty record is the end marker.
    if (Size == 4)
      break;
    Off += Size;
  }
  return true;
}

Relocation *EhFrameSection::getReloc(size_t Off, size_t Size) {
  for (auto &R : Relocations) {
    if (R->getOffset() < Off)
      continue;
    if (R->getOffset() < Off + Size)
      return R;
  }
  return nullptr;
}

bool EhFrameSection::createCIEAndFDEFragments() {
  for (auto &Piece : m_EhFramePieces) {
    if (Piece.getSize() == 4)
      return true;
    size_t Offset = Piece.getOffset();
    uint32_t ID = llvm::support::endian::read32le(Piece.getData().data() + 4);
    if (ID == 0) {
      m_OffsetToCie[Offset] = addCie(Piece);
      ++NumCie;
      continue;
    }

    uint32_t CieOffset = Offset + 4 - ID;
    CIEFragment *Cie = m_OffsetToCie[CieOffset];
    if (!Cie) {
      m_DiagEngine->raise(diag::eh_frame_read_error)
          << "Invalid CIE Reference"
          << getInputFile()->getInput()->decoratedPath();
      return false;
    }

    if (!isFdeLive(Piece)) {
      if (Piece.getRelocation() && getDiagPrinter()->isVerbose())
        m_DiagEngine->raise(diag::verbose_ehframe_remove_fde)
            << std::string("Removing FDE entry for ") +
                   Piece.getRelocation()->symInfo()->name();
      continue;
    }

    if (getDiagPrinter()->isVerbose())
      m_DiagEngine->raise(diag::verbose_ehframe_read_fde)
          << "Reading FDE of size " + std::to_string(Piece.getSize());
    Cie->appendFragment(make<eld::FDEFragment>(Piece, this));
    ++NumFDE;
  }
  return true;
}

CIEFragment *EhFrameSection::addCie(EhFramePiece &P) {
  ResolveInfo *R = nullptr;
  if (P.getRelocation())
    R = P.getRelocation()->symInfo();
  CIEFragment *E = CieMap[{P.getData(), R}];
  if (!E) {
    if (getDiagPrinter()->isVerbose())
      m_DiagEngine->raise(diag::verbose_ehframe_read_cie)
          << "Reading CIE of size " + std::to_string(P.getSize());
    E = make<eld::CIEFragment>(P, this);
    m_CIEFragments.push_back(E);
  }
  return E;
}

// Handle fake eh_frame sections.
void EhFrameSection::finishAddingFragments() {
  if (!size()) {
    setKind(LDFileFormat::Regular);
    return;
  }
  if (!m_CIEFragments.size()) {
    setKind(LDFileFormat::Regular);
    Fragments.push_back(m_EhFrame);
    return;
  }
  for (auto &F : m_CIEFragments)
    Fragments.push_back(F);
}

bool EhFrameSection::isFdeLive(EhFramePiece &P) {
  Relocation *R = P.getRelocation();
  if (!R)
    return false;
  ResolveInfo *symInfo = R->symInfo();
  if (!symInfo->outSymbol()->hasFragRef())
    return false;
  if (R->targetRef()->frag()->getOwningSection()->isIgnore())
    return false;
  if (R->targetRef()->frag()->getOwningSection()->isDiscard())
    return false;
  if (symInfo->outSymbol()->shouldIgnore())
    return false;
  if (symInfo->outSymbol()->fragRef()->isDiscard())
    return false;
  if (symInfo->outSymbol()->fragRef()->frag() &&
      symInfo->getOwningSection()->isIgnore())
    return false;
  return true;
}

DiagnosticPrinter *EhFrameSection::getDiagPrinter() {
  return m_DiagEngine->getPrinter();
}
