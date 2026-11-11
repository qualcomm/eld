//===- EhFrameSection.h----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#ifndef ELD_READERS_EHFRAMESECTION_H
#define ELD_READERS_EHFRAMESECTION_H

#include "eld/Fragment/EhFrameFragment.h"
#include "eld/Readers/ELFSection.h"
#include "llvm/ADT/DenseMap.h"
#include <tuple>

namespace eld {

class DiagnosticEngine;
class DiagnosticPrinter;
class ELFSection;
class Module;
class Relocation;
class ResolveInfo;
class EhFrameSection;
class EhFrameCIE;
class EhFramePiece;
class EhFrameFragment;

using EhFrameCIEMap =
    llvm::DenseMap<std::tuple<llvm::ArrayRef<uint8_t>, ResolveInfo *, uint64_t>,
                   EhFrameCIE *>;

class EhFrameSection : public ELFSection {
public:
  EhFrameSection(std::string Name, DiagnosticEngine *diagEngine, uint32_t pType,
                 uint32_t pFlag, uint32_t entSize, uint64_t pSize);

  static bool classof(const Section *S) {
    return S->getSectionKind() == Section::Kind::EhFrame;
  }

  bool splitEhFrameSection();

  Relocation *getReloc(size_t Off, size_t Size);

  EhFrameFragment *getEhFrameFragment() const { return m_EhFrame; }

  llvm::ArrayRef<uint8_t> getData() const;

  bool createCIEAndFDEFragments(bool DeduplicateCIEs, EhFrameCIEMap &CieMap);

  EhFrameCIE *addCie(EhFramePiece &P, bool DeduplicateCIEs,
                     EhFrameCIEMap &CieMap);

  bool isFdeLive(EhFramePiece &P);

  void finishAddingFragments(Module &M);

private:
  size_t readEhRecordSize(size_t Off);
  DiagnosticPrinter *getDiagPrinter();

private:
  EhFrameFragment *m_EhFrame = nullptr;
  DiagnosticEngine *m_DiagEngine = nullptr;
};
} // namespace eld

#endif
