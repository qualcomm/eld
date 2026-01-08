//===- x86_64PLT.h---------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#ifndef ELD_TARGET_x86_64_PLT_H
#define ELD_TARGET_x86_64_PLT_H

#include "eld/Fragment/PLT.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "x86_64GOT.h"

namespace {

const uint8_t x86_64_plt0[] = {
    0xff, 0x35, 0,    0,    0, 0, // pushq GOTPLT+8(%rip)
    0xff, 0x25, 0,    0,    0, 0, // jmp *GOTPLT+16(%rip)
    0x0f, 0x1f, 0x40, 0x00,       // nop
};

const uint8_t x86_64_plt1[] = {
    0xff, 0x25, 0, 0, 0, 0, // jmpq *got(%rip)
    0x68, 0,    0, 0, 0,    // pushq <relocation index>
    0xe9, 0,    0, 0, 0,    // jmpq plt[0]
};

} // anonymous namespace

namespace eld {

class x86_64GOT;
class IRBuilder;

class x86_64PLT : public PLT {
public:
  x86_64PLT(PLT::PLTType T, eld::IRBuilder &I, x86_64GOT *G, ELFSection *P,
            ResolveInfo *R, uint32_t Align, uint32_t Size)
      : PLT(T, G, P, R, Align, Size) {}

  virtual ~x86_64PLT() {}

  virtual llvm::ArrayRef<uint8_t> getContent() const override = 0;
};

class x86_64PLT0 : public x86_64PLT {
public:
  x86_64PLT0(x86_64GOT *G, eld::IRBuilder &I, ELFSection *P, ResolveInfo *R,
             uint32_t Align, uint32_t Size)
      : x86_64PLT(PLT::PLT0, I, G, P, R, Align, Size) {}

  virtual ~x86_64PLT0() {}

  virtual llvm::ArrayRef<uint8_t> getContent() const override {
    return x86_64_plt0;
  }

  static x86_64PLT0 *Create(eld::IRBuilder &I, x86_64GOT *G, ELFSection *O,
                            ResolveInfo *R, bool HasNow);
};

/** \class x86_64PLTN
 *  \brief per-function PLT entry.
 *
 *  Each external function gets a PLTN entry.
 *  On first call, the dynamic linker resolves the symbol and updates the
 *  corresponding GOTPLTN entry. Subsequent calls jump directly to the function.
 *
 *  The relocation index embedded in the pushq instruction tells the dynamic
 *  linker which .rela.plt entry corresponds to this function.
 */
class x86_64PLTN : public x86_64PLT {
public:
  x86_64PLTN(x86_64GOT *G, eld::IRBuilder &I, ELFSection *P, ResolveInfo *R,
             uint32_t Align, uint32_t Size)
      : x86_64PLT(PLT::PLTN, I, G, P, R, Align, Size), m_RelocIndex(0) {}

  virtual ~x86_64PLTN() {}


  // The relocation index must match the position in .rela.plt so the
  // dynamic linker can find the correct relocation entry when resolving
  // this function on first call.
  void setRelocIndex(uint32_t index) { m_RelocIndex = index; }

  virtual llvm::ArrayRef<uint8_t> getContent() const override {
    memcpy(m_Content, x86_64_plt1, sizeof(x86_64_plt1));

    uint32_t index = m_RelocIndex;
    m_Content[7] = index & 0xFF;
    m_Content[8] = (index >> 8) & 0xFF;
    m_Content[9] = (index >> 16) & 0xFF;
    m_Content[10] = (index >> 24) & 0xFF;

    return llvm::ArrayRef<uint8_t>(m_Content, sizeof(m_Content));
  }

  static x86_64PLTN *Create(eld::IRBuilder &I, x86_64GOT *G, ELFSection *O,
                            ResolveInfo *R, bool HasNow);

private:
  uint32_t m_RelocIndex; // Position in .rela.plt for dynamic linker lookup
  mutable uint8_t m_Content[16];
};

} // namespace eld

#endif
