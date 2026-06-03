//===- TemplatePLT.h-------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
// Refer to eld/Fragment/PLT.h for additional hooks.
//===----------------------------------------------------------------------===//

#ifndef ELD_TARGET_TEMPLATE_PLT_H
#define ELD_TARGET_TEMPLATE_PLT_H

#include "TemplateGOT.h"
#include "eld/Fragment/PLT.h"
#include "eld/SymbolResolver/IRBuilder.h"

namespace {

const uint8_t template_plt0[] = {
    0x00,
};

const uint8_t template_plt1[] = {
    0x00,
};

} // anonymous namespace

namespace eld {

class TemplateGOT;
class IRBuilder;

class TemplatePLT : public PLT {
public:
  TemplatePLT(PLT::PLTType T, eld::IRBuilder &I, TemplateGOT *G, ELFSection *P,
              ResolveInfo *R, uint32_t Align, uint32_t Size)
      : PLT(T, G, P, R, Align, Size) {}

  virtual ~TemplatePLT() {}

  virtual llvm::ArrayRef<uint8_t> getContent() const override = 0;
};

class TemplatePLT0 : public TemplatePLT {
public:
  TemplatePLT0(TemplateGOT *G, eld::IRBuilder &I, ELFSection *P, ResolveInfo *R,
               uint32_t Align, uint32_t Size)
      : TemplatePLT(PLT::PLT0, I, G, P, R, Align, Size) {}

  virtual ~TemplatePLT0() {}

  virtual llvm::ArrayRef<uint8_t> getContent() const override {
    return template_plt0;
  }

  static TemplatePLT0 *Create(eld::IRBuilder &I, TemplateGOT *G, ELFSection *O,
                              ResolveInfo *R);
};

class TemplatePLTN : public TemplatePLT {
public:
  TemplatePLTN(TemplateGOT *G, eld::IRBuilder &I, ELFSection *P, ResolveInfo *R,
               uint32_t Align, uint32_t Size)
      : TemplatePLT(PLT::PLTN, I, G, P, R, Align, Size) {}

  virtual ~TemplatePLTN() {}

  virtual llvm::ArrayRef<uint8_t> getContent() const override {
    return template_plt1;
  }

  static TemplatePLTN *Create(eld::IRBuilder &I, TemplateGOT *G, ELFSection *O,
                              ResolveInfo *R);
};

} // namespace eld

#endif
