//===- EhFrameSection.h----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#ifndef ELD_READERS_EHFRAMESECTION_H
#define ELD_READERS_EHFRAMESECTION_H

#include "eld/Fragment/EhFrameFragment.h"
#include "eld/Readers/ELFSection.h"

namespace eld {

class DiagnosticEngine;
class DiagnosticPrinter;
class ELFSection;
class Relocation;
class RegionFragment;
class EhFrameSection;
class CIEFragment;
class EhFramePiece;

class EhFrameSection : public ELFSection {
public:
  EhFrameSection(std::string Name, DiagnosticEngine *diagEngine, uint32_t pType,
                 uint32_t pFlag, uint32_t entSize, uint64_t pSize);

  static bool classof(const Section *S) {
    return S->getSectionKind() == Section::Kind::EhFrame;
  }

  bool splitEhFrameSection();

  Relocation *getReloc(size_t Off, size_t Size);

  RegionFragment *getEhFrameFragment() const { return m_EhFrame; }

  llvm::ArrayRef<uint8_t> getData() const { return Data; }

  bool createCIEAndFDEFragments();

  CIEFragment *addCie(EhFramePiece &P);

  bool isFdeLive(EhFramePiece &P);

  std::vector<EhFramePiece> &getPieces() { return m_EhFramePieces; }

  std::vector<CIEFragment *> &getCIEs() { return m_CIEFragments; }

  void finishAddingFragments();

private:
  size_t readEhRecordSize(size_t Off);
  DiagnosticPrinter *getDiagPrinter();

private:
  RegionFragment *m_EhFrame = nullptr;
  llvm::ArrayRef<uint8_t> Data;
  std::vector<EhFramePiece> m_EhFramePieces;
  llvm::DenseMap<size_t, CIEFragment *> m_OffsetToCie;
  std::vector<CIEFragment *> m_CIEFragments;
  size_t NumCie = 0;
  size_t NumFDE = 0;
  DiagnosticEngine *m_DiagEngine = nullptr;
};
} // namespace eld

#endif
