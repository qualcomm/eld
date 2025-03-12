//===- AArch64PLT.h--------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef ELD_TARGET_AARCH64_PLT_H
#define ELD_TARGET_AARCH64_PLT_H

#include "AArch64GOT.h"
#include "eld/Fragment/PLT.h"
#include "eld/SymbolResolver/IRBuilder.h"

namespace {

const uint8_t aarch64_plt0[] = {
    0xf0, 0x7b, 0xbf, 0xa9, /* stp x16, x30, [sp, #-16]!  */
    0x10, 0x00, 0x00, 0x90, /* adrp x16, (PLT_GOT+0x10)  */
    0x11, 0x0A, 0x40, 0xf9, /* ldr x17, [x16, #PLT_GOT+0x10]  */
    0x10, 0x42, 0x00, 0x91, /* add x16, x16,#PLT_GOT+0x10   */
    0x20, 0x02, 0x1f, 0xd6, /* br x17  */
    0x1f, 0x20, 0x03, 0xd5, /* nop */
    0x1f, 0x20, 0x03, 0xd5, /* nop */
    0x1f, 0x20, 0x03, 0xd5  /* nop */
};

const uint8_t aarch64_plt1[] = {
    0x10, 0x00, 0x00, 0x90, /* adrp x16, PLTGOT + n * 8  */
    0x11, 0x02, 0x40, 0xf9, /* ldr x17, [x16, PLTGOT + n * 8] */
    0x10, 0x02, 0x00, 0x91, /* add x16, x16, :lo12:PLTGOT + n * 8  */
    0x20, 0x02, 0x1f, 0xd6  /* br x17.  */
};

} // anonymous namespace

namespace eld {

class AArch64GOT;
class IRBuilder;

class AArch64PLT : public PLT {
public:
  AArch64PLT(PLT::PLTType T, eld::IRBuilder &I, AArch64GOT *G, ELFSection *P,
             ResolveInfo *R, uint32_t Align, uint32_t Size)
      : PLT(T, G, P, R, Align, Size) {
    if (P)
      P->addFragmentAndUpdateSize(this);
  }

  virtual ~AArch64PLT() {}

  llvm::ArrayRef<uint8_t> getContent() const override = 0;
};

class AArch64PLT0 : public AArch64PLT {
public:
  AArch64PLT0(AArch64GOT *G, eld::IRBuilder &I, ELFSection *P, ResolveInfo *R,
              uint32_t Align, uint32_t Size)
      : AArch64PLT(PLT::PLT0, I, G, P, R, Align, Size) {}

  virtual ~AArch64PLT0() {}

  virtual llvm::ArrayRef<uint8_t> getContent() const override {
    return aarch64_plt0;
  }

  static AArch64PLT0 *Create(eld::IRBuilder &I, AArch64GOT *G, ELFSection *O,
                             ResolveInfo *R);
};

class AArch64PLTN : public AArch64PLT {
public:
  AArch64PLTN(AArch64GOT *G, eld::IRBuilder &I, ELFSection *P, ResolveInfo *R,
              uint32_t Align, uint32_t Size)
      : AArch64PLT(PLT::PLTN, I, G, P, R, Align, Size) {}

  virtual ~AArch64PLTN() {}

  virtual llvm::ArrayRef<uint8_t> getContent() const override {
    return aarch64_plt1;
  }

  static AArch64PLTN *Create(eld::IRBuilder &I, AArch64GOT *G, ELFSection *O,
                             ResolveInfo *R);
};

} // namespace eld

#endif
