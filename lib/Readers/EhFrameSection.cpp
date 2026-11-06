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

#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticPrinter.h"
#include "eld/Diagnostics/MsgHandler.h"
#include "eld/Fragment/EhFrameFragment.h"
#include "eld/Fragment/RegionFragment.h"
#include "eld/Readers/EhFrameSection.h"
#include "eld/Readers/Relocation.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/Endian.h"

using namespace eld;

namespace {
// In a final link, several input sections can contain the same CIE. Remember
// the first CIE for each byte sequence and personality function.
llvm::DenseMap<std::pair<llvm::ArrayRef<uint8_t>, ResolveInfo *>, EhFrameCIE *>
    CieMap;
} // namespace

EhFrameSection::EhFrameSection(std::string Name, DiagnosticEngine *E,
                               uint32_t Type, uint32_t Flags, uint32_t EntSize,
                               uint64_t Size)
    : ELFSection(Section::Kind::EhFrame, LinkerSectionKind::EhFrame, Name,
                 Flags, EntSize, /*AddrAlign=*/0, Type, /*Info=*/0,
                 /*Link=*/nullptr, Size, /*PAddr=*/0),
      m_DiagEngine(E) {}

size_t EhFrameSection::readEhRecordSize(size_t Off) {
  llvm::ArrayRef<uint8_t> SectionData = getData();
  llvm::ArrayRef<uint8_t> D = SectionData.slice(Off);

  if (D.size() < 4) {
    m_DiagEngine->raise(Diag::eh_frame_read_error)
        << "CIE/FDE too small" << getInputFile()->getInput()->decoratedPath();
    return -1;
  }

  // First 4 bytes of CIE/FDE is the size of the record.
  // If it is 0xFFFFFFFF, the next 8 bytes contain the size instead,
  // but we do not support that format yet.
  uint64_t V = llvm::support::endian::read32le(D.data());
  if (V == UINT32_MAX) {
    m_DiagEngine->raise(Diag::eh_frame_read_error)
        << "CIE/FDE too large" << getInputFile()->getInput()->decoratedPath();
    return -1;
  }
  uint64_t Size = V + 4;
  if (Size > D.size()) {
    m_DiagEngine->raise(Diag::eh_frame_read_error)
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
    if (Fragments.empty())
      return false;
    assert(Fragments.front()->getKind() == Fragment::Type::Region &&
           "EhFrameSection must start with a Region-like fragment");
    m_EhFrame = static_cast<eld::EhFrameFragment *>(Fragments.front());
    Fragments.clear();
  }

  if (!m_EhFrame)
    return false;

  llvm::StringRef Region = m_EhFrame->getRegion();
  for (size_t Off = 0, End = Region.size(); Off != End;) {
    size_t Size = readEhRecordSize(Off);
    if (Size == (size_t)-1)
      return false;

    Relocation *Reloc = getReloc(Off, Size);
    m_EhFrame->addPiece(Off, Size, Reloc);
    if (Reloc && getDiagPrinter()->isVerbose())
      m_DiagEngine->raise(Diag::verbose_ehframe)
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

bool EhFrameSection::createCIEAndFDEFragments(bool DeduplicateCIEs) {
  // An FDE names its CIE with a backward offset within this input section.
  // Each input section starts at offset zero, so use a new lookup map here.
  llvm::DenseMap<size_t, EhFrameCIE *> OffsetToCie;
  if (!m_EhFrame)
    return true;
  llvm::StringRef Region = m_EhFrame->getRegion();
  for (auto &Piece : m_EhFrame->getPieces()) {
    if (Piece.getSize() == 4)
      return true;
    size_t Offset = Piece.getOffset();
    // A zero ID denotes a CIE. For an FDE, ID is the backward distance from
    // the ID field to its CIE.
    uint32_t ID =
        llvm::support::endian::read32le(Piece.getData(Region).data() + 4);
    if (ID == 0) {
      OffsetToCie[Offset] = addCie(Piece, DeduplicateCIEs);
      continue;
    }

    uint32_t CieOffset = Offset + 4 - ID;
    EhFrameCIE *Cie = OffsetToCie[CieOffset];
    if (!Cie) {
      m_DiagEngine->raise(Diag::eh_frame_read_error)
          << "Invalid CIE Reference"
          << getInputFile()->getInput()->decoratedPath();
      return false;
    }

    if (!isFdeLive(Piece)) {
      if (Piece.getRelocation() && getDiagPrinter()->isVerbose())
        m_DiagEngine->raise(Diag::verbose_ehframe_remove_fde)
            << std::string("Removing FDE entry for ") +
                   Piece.getRelocation()->symInfo()->name();
      continue;
    }

    if (getDiagPrinter()->isVerbose())
      m_DiagEngine->raise(Diag::verbose_ehframe_read_fde)
          << "Reading FDE of size " + std::to_string(Piece.getSize());
    Cie->appendFDE(make<eld::FDEPiece>(Piece, *m_EhFrame));
  }
  return true;
}

EhFrameCIE *EhFrameSection::addCie(EhFramePiece &P, bool DeduplicateCIEs) {
  ResolveInfo *R = nullptr;
  if (P.getRelocation())
    R = P.getRelocation()->symInfo();
  EhFrameCIE *E = nullptr;
  if (DeduplicateCIEs)
    E = CieMap[{P.getData(m_EhFrame->getRegion()), R}];
  if (!E) {
    if (getDiagPrinter()->isVerbose())
      m_DiagEngine->raise(Diag::verbose_ehframe_read_cie)
          << "Reading CIE of size " + std::to_string(P.getSize());
    E = make<eld::EhFrameCIE>(P);
    m_EhFrame->getCIEs().push_back(E);
    if (DeduplicateCIEs)
      CieMap[{P.getData(m_EhFrame->getRegion()), R}] = E;
  } else {
    E->addAlias(P);
  }
  return E;
}

void EhFrameSection::finishAddingFragments(Module &ThisModule) {
  if (!size()) {
    setKind(LinkerSectionKind::Regular);
    return;
  }
  if (ThisModule.getLayoutInfo())
    ThisModule.getLayoutInfo()->recordFragment(getInputFile(), this, m_EhFrame);
  Fragments.push_back(m_EhFrame);
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

llvm::ArrayRef<uint8_t> EhFrameSection::getData() const {
  assert(m_EhFrame && "EhFrame fragment must be initialized");
  llvm::StringRef Region = m_EhFrame->getRegion();
  return llvm::ArrayRef<uint8_t>(
      reinterpret_cast<const uint8_t *>(Region.data()), Region.size());
}

DiagnosticPrinter *EhFrameSection::getDiagPrinter() {
  return m_DiagEngine->getPrinter();
}
