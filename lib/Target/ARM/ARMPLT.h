//===- ARMPLT.h------------------------------------------------------------===//
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
#ifndef ELD_TARGET_ARM_PLT_H
#define ELD_TARGET_ARM_PLT_H

#include "ARMGOT.h"
#include "eld/Fragment/PLT.h"
#include "eld/SymbolResolver/IRBuilder.h"

namespace {

const uint8_t arm_plt0[] = {
    0x04, 0xe0, 0x2d,
    0xe5, // str   lr, [sp, #-4]!
    0x04, 0xe0, 0x9f,
    0xe5, // ldr   lr, [pc, #4]
    0x0e, 0xe0, 0x8f,
    0xe0, // add   lr, pc, lr
    0x08, 0xf0, 0xbe,
    0xe5, // ldr   pc, [lr, #8]!
    0x00, 0x00, 0x00,
    0x00 // &GOT[0] - .
};

const uint8_t arm_plt1[] = {
    0x00, 0xc6, 0x8f,
    0xe2, // add   ip, pc, #0xNN00000
    0x00, 0xca, 0x8c,
    0xe2, // add   ip, ip, #0xNN000
    0x00, 0xf0, 0xbc,
    0xe5 // ldr   pc, [ip, #0xNNN]!
};
} // anonymous namespace

namespace eld {

class ARMGOT;
class IRBuilder;

class ARMPLT : public PLT {
public:
  ARMPLT(PLT::PLTType T, eld::IRBuilder &I, ARMGOT *G, ELFSection *P,
         ResolveInfo *R, uint32_t Align, uint32_t Size)
      : PLT(T, G, P, R, Align, Size) {
    if (P)
      P->addFragmentAndUpdateSize(this);
  }

  virtual ~ARMPLT() {}

  llvm::ArrayRef<uint8_t> getContent() const override = 0;
};

class ARMPLT0 : public ARMPLT {
public:
  ARMPLT0(ARMGOT *G, eld::IRBuilder &I, ELFSection *P, ResolveInfo *R,
          uint32_t Align, uint32_t Size)
      : ARMPLT(PLT::PLT0, I, G, P, R, Align, Size) {}

  virtual ~ARMPLT0() {}

  virtual llvm::ArrayRef<uint8_t> getContent() const override {
    return arm_plt0;
  }

  static ARMPLT0 *Create(eld::IRBuilder &I, ARMGOT *G, ELFSection *O,
                         ResolveInfo *R);
};

class ARMPLTN : public ARMPLT {
public:
  ARMPLTN(ARMGOT *G, eld::IRBuilder &I, ELFSection *P, ResolveInfo *R,
          uint32_t Align, uint32_t Size)
      : ARMPLT(PLT::PLTN, I, G, P, R, Align, Size) {}

  virtual ~ARMPLTN() {}

  virtual llvm::ArrayRef<uint8_t> getContent() const override {
    return arm_plt1;
  }

  static ARMPLTN *Create(eld::IRBuilder &I, ARMGOT *G, ELFSection *O,
                         ResolveInfo *R);
};

} // namespace eld

#endif
