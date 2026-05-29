//===- TemplateGOT.h-------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_TARGET_TEMPLATE_GOT_H
#define ELD_TARGET_TEMPLATE_GOT_H

#include "eld/Fragment/GOT.h"
#include "eld/Support/Memory.h"
#include "eld/Target/GNULDBackend.h"

namespace eld {

/** \class TemplateGOT
 *  \brief Template Global Offset Table.
 */

class TemplateGOT : public GOT {
public:
  // Going to be used by GOTPLT0
  TemplateGOT(GOTType T, ELFSection *O, ResolveInfo *R, uint32_t Align,
              uint32_t Size)
      : GOT(T, O, R, Align, Size) {
    if (O)
      O->addFragmentAndUpdateSize(this);
  }

  // Helper constructor for GOT.
  TemplateGOT(GOTType T, ELFSection *O, ResolveInfo *R) : GOT(T, O, R, 4, 4) {
    if (O)
      O->addFragmentAndUpdateSize(this);
  }

  virtual ~TemplateGOT() {}

  virtual TemplateGOT *getFirst() { return this; }

  virtual TemplateGOT *getNext() { return nullptr; }

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

  static TemplateGOT *Create(ELFSection *O, ResolveInfo *R) {
    return make<TemplateGOT>(GOT::Regular, O, R);
  }

private:
  uint8_t Value[4] = {0};
};

class TemplateGOTPLT0 : public TemplateGOT {
public:
  TemplateGOTPLT0(ELFSection *O, ResolveInfo *R)
      : TemplateGOT(GOT::GOTPLT0, O, R, 4, 16) {}

  TemplateGOT *getFirst() override { return this; }

  TemplateGOT *getNext() override { return nullptr; }

  static TemplateGOTPLT0 *Create(ELFSection *O, ResolveInfo *R);

  llvm::ArrayRef<uint8_t> getContent() const override {
    return llvm::ArrayRef(Value);
  }

private:
  uint8_t Value[16] = {0};
};

class TemplateGOTPLTN : public TemplateGOT {
public:
  TemplateGOTPLTN(ELFSection *O, ResolveInfo *R)
      : TemplateGOT(GOT::GOTPLTN, O, R, 4, 4) {}

  TemplateGOT *getFirst() override { return this; }

  TemplateGOT *getNext() override { return nullptr; }

  static TemplateGOTPLTN *Create(ELFSection *O, ResolveInfo *R, Fragment *PLT);
};

class TemplateGDGOT : public TemplateGOT {
public:
  TemplateGDGOT(ELFSection *O, ResolveInfo *R)
      : TemplateGOT(GOT::TLS_GD, O, R),
        Other(make<TemplateGOT>(GOT::TLS_GD, O, R)) {}

  TemplateGOT *getFirst() override { return this; }

  TemplateGOT *getNext() override { return Other; }

  static TemplateGOT *Create(ELFSection *O, ResolveInfo *R) {
    return make<TemplateGDGOT>(O, R);
  }

private:
  TemplateGOT *Other;
};

class TemplateLDGOT : public TemplateGOT {
public:
  TemplateLDGOT(ELFSection *O, ResolveInfo *R)
      : TemplateGOT(GOT::TLS_LD, O, R),
        Other(make<TemplateGOT>(GOT::TLS_LD, O, R)) {}

  TemplateGOT *getFirst() override { return this; }

  TemplateGOT *getNext() override { return Other; }

  static TemplateGOT *Create(ELFSection *O, ResolveInfo *R) {
    return make<TemplateLDGOT>(O, R);
  }

private:
  TemplateGOT *Other;
};

class TemplateIEGOT : public TemplateGOT {
public:
  TemplateIEGOT(ELFSection *O, ResolveInfo *R)
      : TemplateGOT(GOT::TLS_LE, O, R) {}

  TemplateGOT *getFirst() override { return this; }

  TemplateGOT *getNext() override { return nullptr; }

  static TemplateGOT *Create(ELFSection *O, ResolveInfo *R) {
    return make<TemplateIEGOT>(O, R);
  }
};
} // namespace eld

#endif
