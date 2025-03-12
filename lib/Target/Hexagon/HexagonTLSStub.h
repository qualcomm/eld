//===- HexagonTLSStub.h----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_TARGET_HEXAGON_TLSSTUB_H
#define ELD_TARGET_HEXAGON_TLSSTUB_H

#include "eld/Fragment/TargetFragment.h"

const uint8_t hexagon_tls_gd_ie[] = {
    0x1c, 0x40, 0x0a, 0x6a, /*{ r28 = ugp               # the TP              */
    0x00, 0xc0, 0x80, 0x91, /*  r0 = memw (r0 + #0) }   # offset from the TP  */
    0x00, 0x5c, 0x00, 0xf3, /*{ r0 = add (r0, r28)      # address of variable */
    0x00, 0xc0, 0x9f, 0x52  /*  jumpr lr }              # return it           */
};

const uint8_t hexagon_tls_ld_le[] = {
    0x00, 0x40, 0x0a, 0x6a, /*{ r0 = ugp        # get TP    */
    0x00, 0xc0, 0x9f, 0x52  /* jupr lr }        # return it */
};

namespace eld {

class Module;

//===----------------------------------------------------------------------===//
// HexagonTLSStub
//===----------------------------------------------------------------------===//
class HexagonTLSStub : public TargetFragment {
public:
  enum StubType { GD, GDtoIE, LDtoLE };

  HexagonTLSStub(StubType T, ELFSection *O, ResolveInfo *R, uint32_t Align,
                 uint32_t size);

  virtual ~HexagonTLSStub() {}

  static llvm::StringRef stubName(StubType T) {
    switch (T) {
    case GDtoIE:
      return "__hexagon_ie_tls_get_addr";
      break;
    case LDtoLE:
      return "__hexagon_le_tls_get_addr";
      break;
    case GD:
      return "__tls_get_addr";
    }
    return "";
  }

  StubType getStubType() const { return m_StubType; }

private:
  StubType m_StubType;
};

class HexagonGDStub : public HexagonTLSStub {
public:
  HexagonGDStub(ELFSection *O, ResolveInfo *R)
      : HexagonTLSStub(HexagonTLSStub::GD, O, R, 4, 0) {};

  virtual ~HexagonGDStub() {}

  virtual const std::string name() const override {
    return HexagonTLSStub::stubName(HexagonTLSStub::GD).str();
  }

  static HexagonTLSStub *Create(Module &pModule, ELFSection *O);
};

class HexagonGDIEStub : public HexagonTLSStub {
public:
  HexagonGDIEStub(ELFSection *O, ResolveInfo *R)
      : HexagonTLSStub(HexagonTLSStub::GDtoIE, O, R, 4,
                       sizeof(hexagon_tls_gd_ie)) {};

  virtual ~HexagonGDIEStub() {}

  virtual const std::string name() const override {
    return HexagonTLSStub::stubName(HexagonTLSStub::GDtoIE).str();
  }

  virtual llvm::ArrayRef<uint8_t> getContent() const override {
    return hexagon_tls_gd_ie;
  }

  static HexagonTLSStub *Create(Module &pModule, ELFSection *O);
};

class HexagonLDLEStub : public HexagonTLSStub {
public:
  HexagonLDLEStub(ELFSection *O, ResolveInfo *R)
      : HexagonTLSStub(HexagonTLSStub::LDtoLE, O, R, 4,
                       sizeof(hexagon_tls_ld_le)) {};

  virtual ~HexagonLDLEStub() {}

  virtual const std::string name() const override {
    return HexagonTLSStub::stubName(HexagonTLSStub::LDtoLE).str();
  }

  virtual llvm::ArrayRef<uint8_t> getContent() const override {
    return hexagon_tls_ld_le;
  }

  static HexagonTLSStub *Create(Module &pModule, ELFSection *O);
};

} // namespace eld

#endif
