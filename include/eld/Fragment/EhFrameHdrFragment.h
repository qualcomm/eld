//===- EhFrameHdrFragment.h------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_FRAGMENT_EHFRAMEHDRFRAGMENT_H
#define ELD_FRAGMENT_EHFRAMEHDRFRAGMENT_H

#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Fragment/Fragment.h"
#include "eld/Readers/EhFrameHdrSection.h"
#include "llvm/ADT/ArrayRef.h"
#include <string>
#include <vector>

namespace eld {

class DiagnosticEngine;
class EhFrameHdrSection;
class LinkerConfig;
class CIEFragment;
class FDEFragment;
class Relocation;

class EhFrameHdrFragment : public Fragment {
public:
  struct FdeData {
    uint32_t PcRel;
    uint32_t FdeVARel;
  };

  EhFrameHdrFragment(EhFrameHdrSection *O, bool createTable, bool is64Bit);

  virtual ~EhFrameHdrFragment();

  /// name - name of this stub
  const std::string name() const;

  virtual size_t size() const override;

  llvm::ArrayRef<uint8_t> getContent() const;

  static bool classof(const Fragment *F) {
    return F->getKind() == Fragment::EhFrameHdr;
  }

  static bool classof(const EhFrameHdrFragment *) { return true; }

  virtual eld::Expected<void> emit(MemoryRegion &mr, Module &M) override;

  virtual void dump(llvm::raw_ostream &OS) override;

private:
  std::vector<FdeData> getFdeData(uint8_t *D, DiagnosticEngine *DiagEngine);

  uint64_t readFdeAddr(uint8_t *Buf, int Size, DiagnosticEngine *DiagEngine);

  uint64_t getFdePc(uint8_t *, FDEFragment *, uint8_t Enc,
                    DiagnosticEngine *DiagEngine);

  bool m_is64Bit = false;

  bool m_createTable = true;
};

} // namespace eld

#endif
