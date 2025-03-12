//===- RISCVPLT.cpp--------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "RISCVPLT.h"
#include "RISCVLDBackend.h"
#include "RISCVRelocationHelper.h"
#include "eld/Core/Module.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Support/Memory.h"
#include "llvm/Support/Endian.h"

using namespace eld;

// PLT0
RISCVPLT *RISCVPLT::CreatePLT0(RISCVLDBackend &Backend, RISCVGOT *G,
                               ELFSection *O, bool is32bit) {
  RISCVPLT *P = nullptr;
  // RISCV PLT size is 32 bytes
  if (is32bit)
    P = make<RISCVPLT0<uint32_t, 4, 32>>(G, Backend, O);
  else
    P = make<RISCVPLT0<uint64_t, 8, 32>>(G, Backend, O);
  O->addFragmentAndUpdateSize(P);
  return P;
}

// PLTN
RISCVPLT *RISCVPLT::CreatePLTN(RISCVGOT *G, ELFSection *O, ResolveInfo *R,
                               bool is32bit) {
  RISCVPLT *P = nullptr;
  // RISCV PLT size is 32 bytes
  if (is32bit)
    P = make<RISCVPLTN<uint32_t, 4, 16>>(G, O, R);
  else
    P = make<RISCVPLTN<uint32_t, 8, 16>>(G, O, R);
  O->addFragmentAndUpdateSize(P);
  return P;
}

template <typename T, uint32_t Align, uint32_t Size>
eld::Expected<void> RISCVPLT0<T, Align, Size>::emit(MemoryRegion &mr,
                                                    Module &M) {
  // 1: auipc t2, %pcrel_hi(.got.plt)
  // sub t1, t1, t3
  // l[wd] t3, %pcrel_lo(1b)(t2); t3 = _dl_runtime_resolve
  // addi t1, t1, -pltHeaderSize-12; t1 = &.plt[i] - &.plt[0]
  // addi t0, t2, %pcrel_lo(1b)
  // srli t1, t1, (rv64?1:2); t1 = &.got.plt[i] - &.got.plt[0]
  // l[wd] t0, Wordsize(t0); t0 = link_map
  // jr t3
  uint8_t *buf = mr.begin() + this->getOffset(M.getConfig().getDiagEngine());
  uint32_t offset = m_Backend.getGOTPLT()->addr() - m_Backend.getPLT()->addr();
  bool is32bit = (Align == 4);
  uint32_t load = is32bit ? LW : LD;
  llvm::support::endian::write32le(buf + 0, utype(AUIPC, X_T2, hi20(offset)));
  llvm::support::endian::write32le(buf + 4, rtype(SUB, X_T1, X_T1, X_T3));
  llvm::support::endian::write32le(buf + 8,
                                   itype(load, X_T3, X_T2, lo12(offset)));
  llvm::support::endian::write32le(
      buf + 12, itype(ADDI, X_T1, X_T1, -32 /* PLT size */ - 12));
  llvm::support::endian::write32le(buf + 16,
                                   itype(ADDI, X_T0, X_T2, lo12(offset)));
  llvm::support::endian::write32le(buf + 20,
                                   itype(SRLI, X_T1, X_T1, is32bit ? 2 : 1));
  llvm::support::endian::write32le(buf + 24,
                                   itype(load, X_T0, X_T0, is32bit ? 4 : 8));
  llvm::support::endian::write32le(buf + 28, itype(JALR, 0, X_T3, 0));
  return {};
}

template <typename T, uint32_t Align, uint32_t Size>
eld::Expected<void> RISCVPLTN<T, Align, Size>::emit(MemoryRegion &mr,
                                                    Module &M) {
  bool is32bit = (Align == 4);
  uint8_t *buf = mr.begin() + this->getOffset(M.getConfig().getDiagEngine());
  uint32_t offset = this->getGOT()->getAddr(M.getConfig().getDiagEngine()) -
                    this->getAddr(M.getConfig().getDiagEngine());
  llvm::support::endian::write32le(buf + 0, utype(AUIPC, X_T3, hi20(offset)));
  llvm::support::endian::write32le(
      buf + 4, itype(is32bit ? LW : LD, X_T3, X_T3, lo12(offset)));
  llvm::support::endian::write32le(buf + 8, itype(JALR, X_T1, X_T3, 0));
  llvm::support::endian::write32le(buf + 12, itype(ADDI, 0, 0, 0));
  return {};
}
