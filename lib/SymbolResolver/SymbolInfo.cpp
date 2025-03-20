//===- SymbolInfo.cpp------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/SymbolResolver/SymbolInfo.h"
#include "llvm/Support/ErrorHandling.h"
using namespace eld;

SymbolInfo::SymbolInfo(const InputFile *InputFile, size_t Size,
                       ResolveInfo::Binding Binding, ResolveInfo::Type SymType,
                       ResolveInfo::Visibility Visibility,
                       ResolveInfo::Desc SymDesc, bool IsBitcode)
    : SymbolOrigin(InputFile), SymbolSize(Size) {
  setSymbolBinding(Binding);
  setSymbolType(SymType);
  setSymbolVisibility(Visibility);
  setSymbolSectionIndexKind(Binding, SymDesc);
  setBitcodeAttribute(IsBitcode);
}

void SymbolInfo::setSymbolBinding(ResolveInfo::Binding Binding) {
  if (Binding == ResolveInfo::Local)
    SymbolInfoBitfield.SymBinding = SymbolBinding::Local;
  else if (Binding == ResolveInfo::Global)
    SymbolInfoBitfield.SymBinding = SymbolBinding::Global;
  else if (Binding == ResolveInfo::Weak)
    SymbolInfoBitfield.SymBinding = SymbolBinding::Weak;
}

void SymbolInfo::setSymbolType(ResolveInfo::Type SymType) {
  SymbolInfoBitfield.SymType = SymType;
}

void SymbolInfo::setSymbolVisibility(ResolveInfo::Visibility Visibility) {
  SymbolInfoBitfield.SymVisibility = Visibility;
}

void SymbolInfo::setSymbolSectionIndexKind(ResolveInfo::Binding Binding,
                                           ResolveInfo::Desc SymDesc) {
  if (Binding == ResolveInfo::Absolute)
    SymbolInfoBitfield.SymSectIndexKind = SectionIndexKind::Abs;
  else if (SymDesc == ResolveInfo::Desc::Define)
    SymbolInfoBitfield.SymSectIndexKind = SectionIndexKind::Def;
  else if (SymDesc == ResolveInfo::Desc::Undefined)
    SymbolInfoBitfield.SymSectIndexKind = SectionIndexKind::Undef;
  else if (SymDesc == ResolveInfo::Desc::Common)
    SymbolInfoBitfield.SymSectIndexKind = SectionIndexKind::Common;
}

void SymbolInfo::setBitcodeAttribute(bool IsBitcode) {
  SymbolInfoBitfield.IsBitcode = IsBitcode;
}

llvm::StringRef SymbolInfo::getSymbolBindingAsStr() const {
  SymbolBinding SymBinding = getSymbolBinding();
  switch (SymBinding) {
#define ADD_CASE(binding)                                                      \
  case SymbolBinding::binding:                                                 \
    return #binding;
    ADD_CASE(SB_None);
    ADD_CASE(Local);
    ADD_CASE(Global);
    ADD_CASE(Weak);
#undef ADD_CASE
  }
  return "UnknownBinding";
}

llvm::StringRef SymbolInfo::getSymbolTypeAsStr() const {
  ResolveInfo::Type SymType = getSymbolType();
  switch (SymType) {
#define ADD_CASE(type)                                                         \
  case ResolveInfo::Type::type:                                                \
    return #type;
    ADD_CASE(NoType);
    ADD_CASE(Object);
    ADD_CASE(Function);
    ADD_CASE(Section);
    ADD_CASE(File);
    ADD_CASE(CommonBlock);
    ADD_CASE(ThreadLocal);
    ADD_CASE(IndirectFunc);
  default:
    return "UnknownType";
#undef ADD_CASE
  }
}

llvm::StringRef SymbolInfo::getSymbolVisibilityAsStr() const {
  ResolveInfo::Visibility SymVisibility = getSymbolVisibility();
  switch (SymVisibility) {
#define ADD_CASE(visibility)                                                   \
  case ResolveInfo::Visibility::visibility:                                    \
    return #visibility;
    ADD_CASE(Default);
    ADD_CASE(Internal);
    ADD_CASE(Hidden);
    ADD_CASE(Protected);
#undef ADD_CASE
  }
  return "UnknownVisibility";
}

llvm::StringRef SymbolInfo::getSymbolSectionIndexKindAsStr() const {
  SectionIndexKind SymSectIndexKind = getSymbolSectionIndexKind();
  switch (SymSectIndexKind) {
#define ADD_CASE(sectIndexKind)                                                \
  case SectionIndexKind::sectIndexKind:                                        \
    return #sectIndexKind;
    ADD_CASE(SIK_None);
    ADD_CASE(Undef);
    ADD_CASE(Def);
    ADD_CASE(Abs);
    ADD_CASE(Common);
#undef ADD_CASE
  }
  return "UnknownSymbolSectionIndex";
}