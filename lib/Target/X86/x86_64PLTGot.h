//===- x86_64PLTGot.h------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_TARGET_X86_64_PLTGOT_H
#define ELD_TARGET_X86_64_PLTGOT_H

#include "eld/Fragment/PLT.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "x86_64GOT.h"

namespace {

// 8-byte compact .plt.got stub:
//   ff 25 <disp32>   jmpq *sym@GOT(%rip)  — 6 bytes
//   66 90            nop (2-byte)          — 2 bytes
const uint8_t x86_64_pltgot[] = {
    0xff, 0x25, 0, 0, 0, 0, // jmpq *sym@GOT(%rip)
    0x66, 0x90,             // nop
};

} // anonymous namespace

namespace eld {

class IRBuilder;

// x86_64PLTGot — compact 8-byte .plt.got stub.
//
// Reads the resolved function address from the symbol's .got slot (filled
// eagerly by R_X86_64_GLOB_DAT at startup) and jumps directly there.
// No lazy-binding protocol; no JUMP_SLOT reloc; no back-jump to PLT0.
class x86_64PLTGot : public PLT {
public:
  x86_64PLTGot(x86_64GOT *G, ELFSection *O, ResolveInfo *R)
      : PLT(PLT::PLTGot, G, O, R, /*Align=*/8, /*Size=*/8) {}

  virtual ~x86_64PLTGot() {}

  virtual llvm::ArrayRef<uint8_t> getContent() const override {
    return x86_64_pltgot;
  }

  static x86_64PLTGot *Create(eld::IRBuilder &I, x86_64GOT *GotSlot,
                              ELFSection *O, ResolveInfo *R);
};

} // namespace eld

#endif
