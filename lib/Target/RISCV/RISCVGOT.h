//===- RISCVGOT.h----------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_TARGET_RISCV_GOT_H
#define ELD_TARGET_RISCV_GOT_H

#include "eld/Fragment/GOT.h"
#include "eld/Support/Memory.h"
#include "eld/Target/GNULDBackend.h"

namespace eld {

/** \class RISCVGOT
 *  \brief RISCV Global Offset Table.
 */

class RISCVGOT : public GOT {
public:
  // Going to be used by GOTPLT0
  RISCVGOT(GOTType T, ELFSection *O, ResolveInfo *R, uint32_t Align,
           uint32_t Size)
      : GOT(T, O, R, Align, Size) {
    if (O)
      O->addFragmentAndUpdateSize(this);
  }

  llvm::ArrayRef<uint8_t> getContent() const override = 0;

  virtual bool isReserved() const = 0;

  virtual void setReservedValue(uint32_t) {}

  virtual void setReservedValue(uint64_t) {}

  template <typename T> T getGOTContent(T ReservedValue) const {
    T Content = 0;
    // If the GOT contents needs to reflect a symbol value, then we use the
    // symbol value.
    if (getValueType() == GOT::SymbolValue)
      Content = symInfo()->outSymbol()->value();
    if (getValueType() == GOT::TLSStaticSymbolValue) {
      if (isReserved())
        Content = ReservedValue;
      else if (symInfo()) {
        if ((m_GOTType == GOT::TLS_GD) || (m_GOTType == GOT::TLS_LD))
          Content = symInfo()->outSymbol()->value() - 0x800;
        else
          Content = symInfo()->outSymbol()->value();
      }
    }
    return Content;
  }

  virtual ~RISCVGOT() {}

  virtual RISCVGOT *getFirst() { return this; }

  virtual RISCVGOT *getNext() { return nullptr; }

  static RISCVGOT *Create(ELFSection *O, ResolveInfo *R, bool is32bit);
  static RISCVGOT *CreateGOT0(ELFSection *O, ResolveInfo *R, bool is32bit);
  static RISCVGOT *CreateGOTPLT0(ELFSection *O, ResolveInfo *R, bool is32bit);
  static RISCVGOT *CreateGOTPLTN(ELFSection *O, ResolveInfo *R, bool is32bit);
  static RISCVGOT *CreateGD(ELFSection *O, ResolveInfo *R, bool is32bit);
  static RISCVGOT *CreateLD(ELFSection *O, ResolveInfo *R, bool is32bit);
  static RISCVGOT *CreateIE(ELFSection *O, ResolveInfo *R, bool is32bit);
};

template <typename T, uint32_t Align, uint32_t Size>
class RISCVTGOT : public RISCVGOT {
public:
  // Going to be used by GOTPLT0
  RISCVTGOT(GOTType gotType, ELFSection *O, ResolveInfo *R)
      : RISCVGOT(gotType, O, R, Align, Size),
        m_ReservedValue(std::numeric_limits<T>::max()) {
    memset(Value, 0, sizeof(Value));
  }

  bool isReserved() const override {
    return (m_ReservedValue != std::numeric_limits<T>::max());
  }

  void setReservedValue(T val) override { m_ReservedValue = val; }

  llvm::ArrayRef<uint8_t> getContent() const override {
    T Content = getGOTContent<T>(m_ReservedValue);
    std::memcpy((void *)Value, (void *)&Content, sizeof(Content));
    return llvm::ArrayRef<uint8_t>(Value);
  }

  virtual ~RISCVTGOT() {}

private:
  uint8_t Value[Size];
  T m_ReservedValue;
};

template <typename T, uint32_t Align, uint32_t Size>
class RISCVGOTPLTN : public RISCVTGOT<T, Align, Size> {
public:
  RISCVGOTPLTN(ELFSection *O, ResolveInfo *R)
      : RISCVTGOT<T, Align, Size>(GOT::GOTPLTN, O, R) {}

  RISCVGOT *getFirst() override { return this; }

  RISCVGOT *getNext() override { return nullptr; }
};

template <typename T>
class RISCVGOTPLT0 : public RISCVTGOT<T, sizeof(T), sizeof(T) * 2> {
public:
  RISCVGOTPLT0(ELFSection *O, ResolveInfo *R)
      : RISCVTGOT<T, sizeof(T), sizeof(T) * 2>(GOT::GOTPLT0, O, R) {}

  RISCVGOT *getFirst() override { return this; }

  RISCVGOT *getNext() override { return nullptr; }
};

template <typename T, uint32_t Align, uint32_t Size>
class RISCVGDGOT : public RISCVTGOT<T, Align, Size> {
public:
  RISCVGDGOT(ELFSection *O, ResolveInfo *R)
      : RISCVTGOT<T, Align, Size>(GOT::TLS_GD, O, R),
        Other(make<RISCVTGOT<T, Align, Size>>(GOT::TLS_GD, O, R)) {}

  RISCVGOT *getFirst() override { return this; }

  RISCVGOT *getNext() override { return Other; }

private:
  RISCVGOT *Other = nullptr;
};

template <typename T, uint32_t Align, uint32_t Size>
class RISCVLDGOT : public RISCVTGOT<T, Align, Size> {
public:
  RISCVLDGOT(ELFSection *O, ResolveInfo *R)
      : RISCVTGOT<T, Align, Size>(GOT::TLS_LD, O, R),
        Other(make<RISCVTGOT<T, Align, Size>>(GOT::TLS_LD, O, R)) {}

  RISCVGOT *getFirst() override { return this; }

  RISCVGOT *getNext() override { return Other; }

private:
  RISCVGOT *Other = nullptr;
};

template <typename T, uint32_t Align, uint32_t Size>
class RISCVIEGOT : public RISCVTGOT<T, Align, Size> {
public:
  RISCVIEGOT(ELFSection *O, ResolveInfo *R)
      : RISCVTGOT<T, Align, Size>(GOT::TLS_IE, O, R) {}

  RISCVGOT *getFirst() override { return this; }

  RISCVGOT *getNext() override { return nullptr; }
};

} // namespace eld

#endif
