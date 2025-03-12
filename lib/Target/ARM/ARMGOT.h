//===- ARMGOT.h------------------------------------------------------------===//
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
#include "eld/Target/GNULDBackend.h"
#include "llvm/Support/Allocator.h"

namespace eld {

/** \class ARMGOT
 *  \brief ARM Global Offset Table.
 */

class ARMGOT : public GOT {
public:
  // Going to be used by GOTPLT0
  ARMGOT(GOTType T, ELFSection *O, ResolveInfo *R, uint32_t Align,
         uint32_t Size)
      : GOT(T, O, R, Align, Size), m_ReservedValue(UINT32_MAX),
        Value{0, 0, 0, 0} {
    if (O)
      O->addFragmentAndUpdateSize(this);
  }

  // Helper constructor for GOT.
  ARMGOT(GOTType T, ELFSection *O, ResolveInfo *R)
      : GOT(T, O, R, 4, 4), m_ReservedValue(UINT32_MAX) {
    if (O)
      O->addFragmentAndUpdateSize(this);
  }

  virtual ~ARMGOT() {}

  virtual ARMGOT *getFirst() { return this; }

  virtual ARMGOT *getNext() { return nullptr; }

  llvm::ArrayRef<uint8_t> getContent() const override {
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
    if (getValueType() == GOT::TLSStaticSymbolValue) {
      if (m_ReservedValue != UINT32_MAX)
        Content.a = m_ReservedValue;
      else if (symInfo()) {
        if ((m_GOTType == GOT::TLS_GD) || (m_GOTType == GOT::TLS_LD))
          Content.a = symInfo()->outSymbol()->value();
        else
          Content.a = 0x8 + symInfo()->outSymbol()->value();
      }
    }
    std::memcpy((void *)Value, (void *)&Content.a, sizeof(Value));
    return llvm::ArrayRef(Value);
  }

  void setReservedValue(uint32_t val) { m_ReservedValue = val; }

  static ARMGOT *Create(ELFSection *O, ResolveInfo *R) {
    return make<ARMGOT>(GOT::Regular, O, R);
  }

private:
  uint32_t m_ReservedValue;
  uint8_t Value[4];
};

class ARMGOTPLT0 : public ARMGOT {
public:
  ARMGOTPLT0(ELFSection *O, ResolveInfo *R)
      : ARMGOT(GOT::GOTPLT0, O, R, 4, 12), Value() {
    std::memset(Value, 0, sizeof(Value));
  }

  // Consists of three GOT slots.
  virtual llvm::ArrayRef<uint8_t> getContent() const override {
    return llvm::ArrayRef(Value);
  }

  ARMGOT *getFirst() override { return this; }

  ARMGOT *getNext() override { return nullptr; }

  static ARMGOTPLT0 *Create(ELFSection *O, ResolveInfo *R);

private:
  uint8_t Value[12];
};

class ARMGOTPLTN : public ARMGOT {
public:
  ARMGOTPLTN(ELFSection *O, ResolveInfo *R)
      : ARMGOT(GOT::GOTPLTN, O, R, 4, 4) {}

  virtual llvm::ArrayRef<uint8_t> getContent() const override {
    uint32_t Val = 0;
    // Fill the value for IFUNC symbols.
    if (getValueType() == GOT::SymbolValue)
      Val = symInfo()->outSymbol()->value();
    std::memcpy((void *)Value, (void *)&Val, sizeof(Value));
    return llvm::ArrayRef(Value);
  }

  ARMGOT *getFirst() override { return this; }

  ARMGOT *getNext() override { return nullptr; }

  static ARMGOTPLTN *Create(ELFSection *O, ResolveInfo *R, Fragment *PLT);

private:
  uint8_t Value[4];
};

class ARMGDGOT : public ARMGOT {
public:
  ARMGDGOT(ELFSection *O, ResolveInfo *R)
      : ARMGOT(GOT::TLS_GD, O, R), Other(make<ARMGOT>(GOT::TLS_GD, O, R)) {}

  ARMGOT *getFirst() override { return this; }

  ARMGOT *getNext() override { return Other; }

  static ARMGOT *Create(ELFSection *O, ResolveInfo *R) {
    return (make<ARMGDGOT>(O, R));
  }

private:
  ARMGOT *Other;
};

class ARMLDGOT : public ARMGOT {
public:
  ARMLDGOT(ELFSection *O, ResolveInfo *R)
      : ARMGOT(GOT::TLS_LD, O, R), Other(make<ARMGOT>(GOT::TLS_LD, O, R)) {}

  ARMGOT *getFirst() override { return this; }

  ARMGOT *getNext() override { return Other; }

  static ARMGOT *Create(ELFSection *O, ResolveInfo *R) {
    return (make<ARMLDGOT>(O, R));
  }

private:
  ARMGOT *Other;
};

class ARMIEGOT : public ARMGOT {
public:
  ARMIEGOT(ELFSection *O, ResolveInfo *R) : ARMGOT(GOT::TLS_LE, O, R) {}

  ARMGOT *getFirst() override { return this; }

  ARMGOT *getNext() override { return nullptr; }

  static ARMGOT *Create(ELFSection *O, ResolveInfo *R) {
    return (make<ARMIEGOT>(O, R));
  }
};
} // namespace eld

#endif
