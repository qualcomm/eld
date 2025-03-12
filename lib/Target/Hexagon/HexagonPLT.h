//===- HexagonPLT.h--------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_TARGET_HEXAGON_PLT_H
#define ELD_TARGET_HEXAGON_PLT_H

#include "HexagonGOT.h"
#include "eld/Fragment/PLT.h"
#include "eld/SymbolResolver/IRBuilder.h"

namespace {

const uint8_t hexagon_plt0[] = {
    0x00, 0x40, 0x00, 0x00, // { immext (#0)
    0x1c, 0xc0, 0x49, 0x6a, //   r28 = add (pc, ##GOT0@PCREL) } # @GOT0
    0x0e, 0x42, 0x9c, 0xe2, // { r14 -= add (r28, #16)  # offset of GOTn
    0x4f, 0x40, 0x9c, 0x91, //   r15 = memw (r28 + #8)  # object ID at GOT2
    0x3c, 0xc0, 0x9c, 0x91, //   r28 = memw (r28 + #4) }# dynamic link at GOT1
    0x0e, 0x42, 0x0e, 0x8c, // { r14 = asr (r14, #2)    # index of PLTn
    0x00, 0xc0, 0x9c, 0x52, //   jumpr r28 }            # call dynamic linker
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const uint8_t hexagon_plt1[] = {
    0x00, 0x40, 0x00, 0x00, // { immext (#0)
    0x0e, 0xc0, 0x49, 0x6a, //   r14 = add (pc, ##GOTn@PCREL) }
    0x1c, 0xc0, 0x8e, 0x91, //   r28 = memw (r14)
    0x00, 0xc0, 0x9c, 0x52, //   jumpr r28
};

} // anonymous namespace

namespace eld {

class HexagonGOT;
class IRBuilder;

class HexagonPLT : public PLT {
public:
  HexagonPLT(PLT::PLTType T, eld::IRBuilder &I, HexagonGOT *G, ELFSection *P,
             ResolveInfo *R, uint32_t Align, uint32_t Size)
      : PLT(T, G, P, R, Align, Size) {}

  virtual ~HexagonPLT() {}

  virtual llvm::ArrayRef<uint8_t> getContent() const override = 0;
};

class HexagonPLT0 : public HexagonPLT {
public:
  HexagonPLT0(HexagonGOT *G, eld::IRBuilder &I, ELFSection *P, ResolveInfo *R,
              uint32_t Align, uint32_t Size)
      : HexagonPLT(PLT::PLT0, I, G, P, R, Align, Size) {}

  virtual ~HexagonPLT0() {}

  virtual llvm::ArrayRef<uint8_t> getContent() const override {
    return hexagon_plt0;
  }

  static HexagonPLT0 *Create(eld::IRBuilder &I, HexagonGOT *G, ELFSection *O,
                             ResolveInfo *R);
};

class HexagonPLTN : public HexagonPLT {
public:
  HexagonPLTN(HexagonGOT *G, eld::IRBuilder &I, ELFSection *P, ResolveInfo *R,
              uint32_t Align, uint32_t Size)
      : HexagonPLT(PLT::PLTN, I, G, P, R, Align, Size) {}

  virtual ~HexagonPLTN() {}

  virtual llvm::ArrayRef<uint8_t> getContent() const override {
    return hexagon_plt1;
  }

  static HexagonPLTN *Create(eld::IRBuilder &I, HexagonGOT *G, ELFSection *O,
                             ResolveInfo *R);
};

} // namespace eld

#endif
