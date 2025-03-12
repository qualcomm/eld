//===- RISCVPLT.h----------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_TARGET_RISCV_PLT_H
#define ELD_TARGET_RISCV_PLT_H

#include "RISCVGOT.h"
#include "eld/Fragment/PLT.h"

namespace eld {
class LinkerConfig;
class RISCVGOT;
class RISCVLDBackend;

class RISCVPLT : public PLT {
public:
  RISCVPLT(PLT::PLTType T, RISCVGOT *G, ELFSection *P, ResolveInfo *R,
           uint32_t Align, uint32_t Size)
      : PLT(T, G, P, R, Align, Size) {}

  llvm::ArrayRef<uint8_t> getContent() const override {
    return llvm::ArrayRef<uint8_t>();
  }

  virtual eld::Expected<void> emit(MemoryRegion &mr, Module &M) override = 0;

  virtual ~RISCVPLT() {}

  static RISCVPLT *CreatePLT0(RISCVLDBackend &Backend, RISCVGOT *G,
                              ELFSection *O, bool is32bit);
  static RISCVPLT *CreatePLTN(RISCVGOT *G, ELFSection *O, ResolveInfo *R,
                              bool is32bit);
};

template <typename T, uint32_t Align, uint32_t Size>
class RISCVTPLT : public RISCVPLT {
public:
  // Going to be used by GOTPLT0
  RISCVTPLT(PLT::PLTType pltType, RISCVGOT *G, ELFSection *O, ResolveInfo *R)
      : RISCVPLT(pltType, G, O, R, Align, Size) {}

  virtual ~RISCVTPLT() {}
};

template <typename T, uint32_t Align, uint32_t Size>
class RISCVPLT0 : public RISCVTPLT<T, Align, Size> {
public:
  RISCVPLT0(RISCVGOT *G, RISCVLDBackend &Backend, ELFSection *P)
      : RISCVTPLT<T, Align, Size>(PLT::PLT0, G, P, nullptr),
        m_Backend(Backend) {}

  virtual ~RISCVPLT0() {}

  eld::Expected<void> emit(MemoryRegion &mr, Module &M) override;

private:
  RISCVLDBackend &m_Backend;
};

template <typename T, uint32_t Align, uint32_t Size>
class RISCVPLTN : public RISCVTPLT<T, Align, Size> {
public:
  RISCVPLTN(RISCVGOT *G, ELFSection *P, ResolveInfo *R)
      : RISCVTPLT<T, Align, Size>(PLT::PLTN, G, P, R) {}

  virtual ~RISCVPLTN() {}

  eld::Expected<void> emit(MemoryRegion &mr, Module &M) override;
};

} // namespace eld

#endif
