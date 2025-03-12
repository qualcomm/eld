//===- AArch64GOT.h--------------------------------------------------------===//
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
#ifndef ELD_TARGET_AARCH64_GOT_H
#define ELD_TARGET_AARCH64_GOT_H

#include "eld/Fragment/GOT.h"
#include "eld/Support/Memory.h"
#include "eld/Target/GNULDBackend.h"

namespace eld {

/** \class AArch64GOT
 *  \brief AArch64 Global Offset Table.
 */

class AArch64GOT : public GOT {
public:
  // Going to be used by GOTPLT0
  AArch64GOT(GOTType T, ELFSection *O, ResolveInfo *R, uint32_t Align,
             uint32_t Size)
      : GOT(T, O, R, Align, Size) {
    if (O)
      O->addFragmentAndUpdateSize(this);
  }

  // Helper constructor for GOT.
  AArch64GOT(GOTType T, ELFSection *O, ResolveInfo *R)
      : GOT(T, O, R, 8, 8), Value{0, 0, 0, 0, 0, 0, 0, 0} {
    if (O)
      O->addFragmentAndUpdateSize(this);
  }

  virtual ~AArch64GOT() {}

  virtual AArch64GOT *getFirst() { return this; }

  virtual AArch64GOT *getNext() { return nullptr; }

  llvm::ArrayRef<uint8_t> getContent() const override {
    // Convert uint32_t to ArrayRef.
    typedef union {
      uint64_t a;
      uint8_t b[8];
    } C;
    C Content;
    Content.a = 0;
    // If the GOT contents needs to reflect a symbol value, then we use the
    // symbol value.
    if (getValueType() == GOT::SymbolValue)
      Content.a = symInfo()->outSymbol()->value();
    if (getValueType() == GOT::TLSStaticSymbolValue)
      Content.a = 0x10 + symInfo()->outSymbol()->value();
    std::memcpy((void *)Value, (void *)&Content.a, sizeof(Value));
    return llvm::ArrayRef(Value);
  }

  static AArch64GOT *Create(ELFSection *O, ResolveInfo *R) {
    return (make<AArch64GOT>(GOT::Regular, O, R));
  }

private:
  uint8_t Value[8];
};

class AArch64GOTPLT0 : public AArch64GOT {
public:
  AArch64GOTPLT0(ELFSection *O, ResolveInfo *R)
      : AArch64GOT(GOT::GOTPLT0, O, R, 8, 24), Value() {
    std::memset(Value, 0, sizeof(Value));
  }

  // Consists of three GOT slots.
  virtual llvm::ArrayRef<uint8_t> getContent() const override {
    return llvm::ArrayRef(Value);
  }

  AArch64GOT *getFirst() override { return this; }

  AArch64GOT *getNext() override { return nullptr; }

  static AArch64GOTPLT0 *Create(ELFSection *O, ResolveInfo *R);

private:
  uint8_t Value[24];
};

class AArch64GOTPLTN : public AArch64GOT {
public:
  AArch64GOTPLTN(ELFSection *O, ResolveInfo *R)
      : AArch64GOT(GOT::GOTPLTN, O, R, 8, 8) {}

  virtual llvm::ArrayRef<uint8_t> getContent() const override {
    uint64_t Val = 0;
    // Fill the value for IFUNC symbols.
    if (getValueType() == GOT::SymbolValue)
      Val = symInfo()->outSymbol()->value();
    std::memcpy((void *)Value, (void *)&Val, sizeof(Value));
    return llvm::ArrayRef(Value);
  }

  AArch64GOT *getFirst() override { return this; }

  AArch64GOT *getNext() override { return nullptr; }

  static AArch64GOTPLTN *Create(ELFSection *O, ResolveInfo *R, Fragment *PLT);

private:
  uint8_t Value[8];
};

class AArch64TLSDESCGOT : public AArch64GOT {
public:
  AArch64TLSDESCGOT(ELFSection *O, ResolveInfo *R)
      : AArch64GOT(GOT::TLS_DESC, O, R),
        Other(make<AArch64GOT>(GOT::TLS_DESC, O, R)) {}

  AArch64GOT *getFirst() override { return this; }

  AArch64GOT *getNext() override { return Other; }

  static AArch64GOT *Create(ELFSection *O, ResolveInfo *R) {
    return (make<AArch64TLSDESCGOT>(O, R));
  }

private:
  AArch64GOT *Other;
};

class AArch64IEGOT : public AArch64GOT {
public:
  AArch64IEGOT(ELFSection *O, ResolveInfo *R) : AArch64GOT(GOT::TLS_LE, O, R) {}

  AArch64GOT *getFirst() override { return this; }

  AArch64GOT *getNext() override { return nullptr; }

  static AArch64GOT *Create(ELFSection *O, ResolveInfo *R) {
    return (make<AArch64IEGOT>(O, R));
  }
};
} // namespace eld

#endif
