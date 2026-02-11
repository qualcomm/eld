//===- EhFrameFragment.h---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_FRAGMENT_EHFRAMEFRAGMENT_H
#define ELD_FRAGMENT_EHFRAMEFRAGMENT_H

#include "eld/Fragment/RegionFragment.h"
#include "llvm/ADT/ArrayRef.h"
#include <string>
#include <vector>

namespace eld {

class DiagnosticEngine;
class EhFrameFragment;
class ELFSection;
class FDEPiece;
class LinkerConfig;
class Relocation;

class EhFramePiece {
public:
  EhFramePiece(size_t Off, size_t Sz, Relocation *R)
      : MOffset(Off), ThisSize(Sz), MRelocation(R) {}

  size_t getSize() const { return ThisSize; }

  size_t getOffset() const { return MOffset; }

  bool hasOutputOffset() const { return MOutputOffset != (size_t)-1; };

  size_t getOutputOffset() const { return MOutputOffset; }

  void setOutputOffset(size_t Off) { MOutputOffset = Off; }

  Relocation *getRelocation() const { return MRelocation; }

  llvm::ArrayRef<uint8_t> getData(llvm::StringRef Region) const;

  uint8_t readByte(DiagnosticEngine *DiagEngine, const ELFSection &S);

  void skipBytes(size_t Count, DiagnosticEngine *DiagEngine,
                 const ELFSection &S);

  llvm::StringRef readString(DiagnosticEngine *DiagEngine, const ELFSection &S);

  void skipLeb128(DiagnosticEngine *DiagEngine, const ELFSection &S);

  size_t getAugPSize(unsigned Enc, bool Is64Bit, DiagnosticEngine *DiagEngine);

  void skipAugP(bool Is64Bit, DiagnosticEngine *DiagEngine,
                const ELFSection &S);

  uint8_t getFdeEncoding(llvm::StringRef Region, bool Is64Bit,
                         DiagnosticEngine *DiagEngine, const ELFSection &S);

private:
  llvm::ArrayRef<uint8_t> D;
  size_t MOffset = 0;
  size_t MOutputOffset = -1;
  size_t ThisSize = 0;
  Relocation *MRelocation = nullptr;
};

class EhFrameCIE {
public:
  explicit EhFrameCIE(EhFramePiece &P) : CIE(P) {}

  const std::string name() const { return "CIE"; }

  size_t size() const;

  EhFramePiece &getCIE() { return CIE; }

  const EhFramePiece &getCIE() const { return CIE; }

  llvm::ArrayRef<uint8_t> getContent(const EhFrameFragment &F) const;

  eld::Expected<void> emit(MemoryRegion &Mr, Module &M, uint32_t CieOff,
                           const EhFrameFragment &F);

  void dump(llvm::raw_ostream &OS) {}

  void appendFDE(FDEPiece *F) { FDEs.push_back(F); }

  const std::vector<FDEPiece *> &getFDEs() const { return FDEs; }

  void setOffset(uint32_t CieOff);

  size_t getNumFDE() const { return FDEs.size(); }

  uint8_t getFdeEncoding(const EhFrameFragment &F, bool Is64Bit,
                         DiagnosticEngine *DiagEngine);

private:
  EhFramePiece &CIE;
  std::vector<FDEPiece *> FDEs;
};

class EhFrameFragment : public RegionFragment {
public:
  EhFrameFragment(llvm::StringRef Region, ELFSection *O, uint32_t Align = 1)
      : RegionFragment(Region, O, Fragment::Type::Region, Align) {}

  EhFramePiece &addPiece(size_t Off, size_t Sz, Relocation *R);

  std::vector<EhFramePiece> &getPieces() { return Pieces; }
  const std::vector<EhFramePiece> &getPieces() const { return Pieces; }

  std::vector<EhFrameCIE *> &getCIEs() { return CIEs; }
  const std::vector<EhFrameCIE *> &getCIEs() const { return CIEs; }

  size_t size() const override;

  void setOffset(uint32_t O) override;

  eld::Expected<void> emit(MemoryRegion &Mr, Module &M) override;

  void dump(llvm::raw_ostream &OS) override;

private:
  std::vector<EhFramePiece> Pieces;
  std::vector<EhFrameCIE *> CIEs;
};

class FDEPiece {
public:
  explicit FDEPiece(EhFramePiece &P) : FDE(P) {}

  size_t size() const { return FDE.getSize(); }

  llvm::ArrayRef<uint8_t> getContent(const EhFrameFragment &F) const {
    return FDE.getData(F.getRegion());
  }

  eld::Expected<void> emit(MemoryRegion &Mr, Module &M,
                           const EhFrameFragment &F);

  EhFramePiece &getFDE() { return FDE; }
  const EhFramePiece &getFDE() const { return FDE; }

private:
  EhFramePiece &FDE;
};

} // namespace eld

#endif
