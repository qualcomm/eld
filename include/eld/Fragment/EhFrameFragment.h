//===- EhFrameFragment.h---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_FRAGMENT_EHFRAMEFRAGMENT_H
#define ELD_FRAGMENT_EHFRAMEFRAGMENT_H

#include "eld/Fragment/Fragment.h"
#include "eld/Readers/EhFrameSection.h"
#include "llvm/ADT/ArrayRef.h"
#include <string>
#include <vector>

namespace eld {

class DiagnosticEngine;
class EhFrameSection;
class LinkerConfig;
class Relocation;

class EhFramePiece {
public:
  EhFramePiece(size_t Off, size_t Sz, Relocation *R, EhFrameSection *O)
      : m_Offset(Off), m_Size(Sz), m_Relocation(R), m_Section(O) {}

  size_t getSize() const { return m_Size; }

  size_t getOffset() const { return m_Offset; }

  bool hasOutputOffset() const { return m_OutputOffset != (size_t)-1; };

  size_t getOutputOffset() const { return m_OutputOffset; }

  void setOutputOffset(size_t Off) { m_OutputOffset = Off; }

  Relocation *getRelocation() const { return m_Relocation; }

  EhFrameSection *getOwningSection() const { return m_Section; }

  llvm::ArrayRef<uint8_t> getData();

  uint8_t readByte(DiagnosticEngine *DiagEngine);

  void skipBytes(size_t Count, DiagnosticEngine *DiagEngine);

  llvm::StringRef readString(DiagnosticEngine *DiagEngine);

  void skipLeb128(DiagnosticEngine *DiagEngine);

  size_t getAugPSize(unsigned Enc, bool is64Bit, DiagnosticEngine *DiagEngine);

  void skipAugP(bool is64Bit, DiagnosticEngine *DiagEngine);

  uint8_t getFdeEncoding(bool is64Bit, DiagnosticEngine *DiagEngine);

private:
  llvm::ArrayRef<uint8_t> D;
  size_t m_Offset = 0;
  size_t m_OutputOffset = -1;
  size_t m_Size = 0;
  Relocation *m_Relocation = nullptr;
  EhFrameSection *m_Section = nullptr;
};

class FDEFragment : public Fragment {
public:
  FDEFragment(EhFramePiece &P, EhFrameSection *O);

  virtual ~FDEFragment();

  /// name - name of this stub
  const std::string name() const;

  virtual size_t size() const override;

  llvm::ArrayRef<uint8_t> getContent() const;

  static bool classof(const Fragment *F) {
    return F->getKind() == Fragment::CIE;
  }

  static bool classof(const FDEFragment *) { return true; }

  virtual eld::Expected<void> emit(MemoryRegion &mr, Module &M) override;

  virtual void dump(llvm::raw_ostream &OS) override;

  EhFramePiece &getFDE() { return FDE; }

  size_t getSize() const;

private:
  EhFramePiece &FDE;
};

class CIEFragment : public Fragment {
public:
  CIEFragment(EhFramePiece &P, EhFrameSection *O);

  virtual ~CIEFragment();

  /// name - name of this stub
  const std::string name() const;

  virtual size_t size() const override;

  llvm::ArrayRef<uint8_t> getContent() const;

  static bool classof(const Fragment *F) {
    return F->getKind() == Fragment::CIE;
  }

  static bool classof(const CIEFragment *) { return true; }

  virtual eld::Expected<void> emit(MemoryRegion &mr, Module &M) override;

  virtual void dump(llvm::raw_ostream &OS) override;

  void appendFragment(FDEFragment *F) { FDEs.push_back(F); }

  const std::vector<FDEFragment *> &getFDEs() const { return FDEs; }

  void setOffset(uint32_t Offset) override;

  size_t getNumFDE() const { return FDEs.size(); }

  uint8_t getFdeEncoding(bool is64Bit, DiagnosticEngine *DiagEngine);

protected:
  EhFramePiece &CIE;
  std::vector<FDEFragment *> FDEs;
};

} // namespace eld

#endif
