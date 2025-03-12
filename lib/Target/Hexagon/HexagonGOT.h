//===- HexagonGOT.h--------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_TARGET_HEXAGON_GOT_H
#define ELD_TARGET_HEXAGON_GOT_H

#include "eld/Fragment/GOT.h"
#include "eld/Support/Memory.h"
#include "eld/Target/GNULDBackend.h"

namespace eld {

/** \class HexagonGOT
 *  \brief Hexagon Global Offset Table.
 */

class HexagonGOT : public GOT {
public:
  // Going to be used by GOTPLT0
  HexagonGOT(GOTType T, ELFSection *O, ResolveInfo *R, uint32_t Align,
             uint32_t Size)
      : GOT(T, O, R, Align, Size) {
    if (O)
      O->addFragmentAndUpdateSize(this);
  }

  // Helper constructor for GOT.
  HexagonGOT(GOTType T, ELFSection *O, ResolveInfo *R) : GOT(T, O, R, 4, 4) {
    if (O)
      O->addFragmentAndUpdateSize(this);
  }

  virtual ~HexagonGOT() {}

  virtual HexagonGOT *getFirst() { return this; }

  virtual HexagonGOT *getNext() { return nullptr; }

  virtual llvm::ArrayRef<uint8_t> getContent() const override {
    // Convert uint32_t to ArrayRef.
    typedef union {
      uint32_t a;
      uint8_t b[4];
    } C;
    C Content;
    Content.a = 0;
    // If the GOT contents needs to reflect a symbol value, then we use the
    // symbol value.
    if (getValueType() == GOT::SymbolValue)
      Content.a = symInfo()->outSymbol()->value();
    if (getValueType() == GOT::TLSStaticSymbolValue)
      Content.a =
          symInfo()->outSymbol()->value() - GNULDBackend::getTLSTemplateSize();
    std::memcpy((void *)Value, (void *)&Content.a, sizeof(Value));
    return llvm::ArrayRef(Value);
  }

  static HexagonGOT *Create(ELFSection *O, ResolveInfo *R) {
    return make<HexagonGOT>(GOT::Regular, O, R);
  }

private:
  uint8_t Value[4] = {0};
};

class HexagonGOTPLT0 : public HexagonGOT {
public:
  HexagonGOTPLT0(ELFSection *O, ResolveInfo *R)
      : HexagonGOT(GOT::GOTPLT0, O, R, 4, 16) {}

  HexagonGOT *getFirst() override { return this; }

  HexagonGOT *getNext() override { return nullptr; }

  static HexagonGOTPLT0 *Create(ELFSection *O, ResolveInfo *R);

  llvm::ArrayRef<uint8_t> getContent() const override {
    return llvm::ArrayRef(Value);
  }

private:
  uint8_t Value[16] = {0};
};

class HexagonGOTPLTN : public HexagonGOT {
public:
  HexagonGOTPLTN(ELFSection *O, ResolveInfo *R)
      : HexagonGOT(GOT::GOTPLTN, O, R, 4, 4) {}

  HexagonGOT *getFirst() override { return this; }

  HexagonGOT *getNext() override { return nullptr; }

  static HexagonGOTPLTN *Create(ELFSection *O, ResolveInfo *R, Fragment *PLT);
};

class HexagonGDGOT : public HexagonGOT {
public:
  HexagonGDGOT(ELFSection *O, ResolveInfo *R)
      : HexagonGOT(GOT::TLS_GD, O, R),
        Other(make<HexagonGOT>(GOT::TLS_GD, O, R)) {}

  HexagonGOT *getFirst() override { return this; }

  HexagonGOT *getNext() override { return Other; }

  static HexagonGOT *Create(ELFSection *O, ResolveInfo *R) {
    return make<HexagonGDGOT>(O, R);
  }

private:
  HexagonGOT *Other;
};

class HexagonLDGOT : public HexagonGOT {
public:
  HexagonLDGOT(ELFSection *O, ResolveInfo *R)
      : HexagonGOT(GOT::TLS_LD, O, R),
        Other(make<HexagonGOT>(GOT::TLS_LD, O, R)) {}

  HexagonGOT *getFirst() override { return this; }

  HexagonGOT *getNext() override { return Other; }

  static HexagonGOT *Create(ELFSection *O, ResolveInfo *R) {
    return make<HexagonLDGOT>(O, R);
  }

private:
  HexagonGOT *Other;
};

class HexagonIEGOT : public HexagonGOT {
public:
  HexagonIEGOT(ELFSection *O, ResolveInfo *R) : HexagonGOT(GOT::TLS_LE, O, R) {}

  HexagonGOT *getFirst() override { return this; }

  HexagonGOT *getNext() override { return nullptr; }

  static HexagonGOT *Create(ELFSection *O, ResolveInfo *R) {
    return make<HexagonIEGOT>(O, R);
  }
};
} // namespace eld

#endif
