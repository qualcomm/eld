//===- SymbolInfo.cpp------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/SymbolResolver/SymbolInfo.h"
#include "llvm/Support/ErrorHandling.h"
using namespace eld;

SymbolInfo::SymbolInfo(const InputFile *inputFile, size_t size,
                       ResolveInfo::Binding binding, ResolveInfo::Type symType,
                       ResolveInfo::Visibility visibility,
                       ResolveInfo::Desc symDesc, bool isBitcode)
    : m_InputFile(inputFile), m_Size(size) {
  setSymbolBinding(binding);
  setSymbolType(symType);
  setSymbolVisibility(visibility);
  setSymbolSectionIndexKind(binding, symDesc);
  setBitcodeAttribute(isBitcode);
}

void SymbolInfo::setSymbolBinding(ResolveInfo::Binding binding) {
  if (binding == ResolveInfo::Local)
    m_SymbolInfoBitfield.symBinding = SymbolBinding::Local;
  else if (binding == ResolveInfo::Global)
    m_SymbolInfoBitfield.symBinding = SymbolBinding::Global;
  else if (binding == ResolveInfo::Weak)
    m_SymbolInfoBitfield.symBinding = SymbolBinding::Weak;
}

void SymbolInfo::setSymbolType(ResolveInfo::Type symType) {
  m_SymbolInfoBitfield.symType = symType;
}

void SymbolInfo::setSymbolVisibility(ResolveInfo::Visibility visibility) {
  m_SymbolInfoBitfield.symVisibility = visibility;
}

void SymbolInfo::setSymbolSectionIndexKind(ResolveInfo::Binding binding,
                                           ResolveInfo::Desc symDesc) {
  if (binding == ResolveInfo::Absolute)
    m_SymbolInfoBitfield.symSectIndexKind = SectionIndexKind::Abs;
  else if (symDesc == ResolveInfo::Desc::Define)
    m_SymbolInfoBitfield.symSectIndexKind = SectionIndexKind::Def;
  else if (symDesc == ResolveInfo::Desc::Undefined)
    m_SymbolInfoBitfield.symSectIndexKind = SectionIndexKind::Undef;
  else if (symDesc == ResolveInfo::Desc::Common)
    m_SymbolInfoBitfield.symSectIndexKind = SectionIndexKind::Common;
}

void SymbolInfo::setBitcodeAttribute(bool isBitcode) {
  m_SymbolInfoBitfield.isBitcode = isBitcode;
}

llvm::StringRef SymbolInfo::getSymbolBindingAsStr() const {
  SymbolBinding symBinding = getSymbolBinding();
  switch (symBinding) {
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
  ResolveInfo::Type symType = getSymbolType();
  switch (symType) {
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
  ResolveInfo::Visibility symVisibility = getSymbolVisibility();
  switch (symVisibility) {
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
  SectionIndexKind symSectIndexKind = getSymbolSectionIndexKind();
  switch (symSectIndexKind) {
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