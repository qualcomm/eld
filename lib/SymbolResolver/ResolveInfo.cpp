//===- ResolveInfo.cpp-----------------------------------------------------===//
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
#include "eld/SymbolResolver/ResolveInfo.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Support/StringRefUtils.h"
#include "eld/Target/GNULDBackend.h"
#include "llvm/Support/ManagedStatic.h"
#include <string>

using namespace eld;

/// g_NullResolveInfo - a pointer to Null
static ResolveInfo GNullResolveInfo;

//===----------------------------------------------------------------------===//
// ResolveInfo
//===----------------------------------------------------------------------===//
ResolveInfo::ResolveInfo()
    : SymbolSize(0), SymbolValue(0), ThisBitField(0), SymbolName(""),
      SymbolAlias(nullptr), SymbolResolvedOrigin(nullptr) {
  OutputSymbol = nullptr;
}

ResolveInfo::ResolveInfo(llvm::StringRef Name)
    : SymbolSize(0), SymbolValue(0), ThisBitField(0), SymbolName(Name),
      SymbolAlias(nullptr), SymbolResolvedOrigin(nullptr) {
  OutputSymbol = nullptr;
}

ResolveInfo::~ResolveInfo() {}

void ResolveInfo::override(const ResolveInfo &PFrom, bool CurOverrideOrigin) {
  SymbolSize = PFrom.SymbolSize;
  setValue(PFrom.value(), false);
  overrideAttributes(PFrom);
  if (CurOverrideOrigin) {
    setResolvedOrigin(PFrom.resolvedOrigin());
    setAlias(PFrom.alias());
  }
}

void ResolveInfo::overrideAttributes(const ResolveInfo &PFrom) {
  // Do not override visibility
  auto V = visibility();
  bool P = shouldPreserve();
  ThisBitField &= ~ResolveMask;
  ThisBitField |= (PFrom.ThisBitField & ResolveMask);
  shouldPreserve(P);
  setVisibility(V);
}

/// overrideVisibility - override the visibility
///   always use the most strict visibility
void ResolveInfo::overrideVisibility(const ResolveInfo &PFrom) {
  //
  // The rule for combining visibility is that we always choose the
  // most constrained visibility.  In order of increasing constraint,
  // visibility goes PROTECTED, HIDDEN, INTERNAL.  This is the reverse
  // of the numeric values, so the effect is that we always want the
  // smallest non-zero value.
  //
  // enum {
  //   STV_DEFAULT = 0,
  //   STV_INTERNAL = 1,
  //   STV_HIDDEN = 2,
  //   STV_PROTECTED = 3
  // };

  assert(!isDyn());
  Visibility FromVis = PFrom.visibility();
  Visibility CurVis = visibility();
  if (0 != FromVis) {
    if (0 == CurVis)
      setVisibility(FromVis);
    else if (CurVis > FromVis)
      setVisibility(FromVis);
  }
}

void ResolveInfo::setRegular() { ThisBitField &= (~DynamicFlag); }

void ResolveInfo::setDynamic() { ThisBitField |= DynamicFlag; }

void ResolveInfo::setSource(bool IsSymbolInDynamicLibrary) {
  if (IsSymbolInDynamicLibrary)
    ThisBitField |= DynamicFlag;
  else
    ThisBitField &= (~DynamicFlag);
}

void ResolveInfo::setExportToDyn() { ThisBitField |= ExportDynFlag; }
void ResolveInfo::clearExportToDyn() { ThisBitField &= ~ExportDynFlag; }

void ResolveInfo::setType(uint32_t Type) {
  ThisBitField &= ~TypeMask;
  ThisBitField |= ((Type << TypeOffset) & TypeMask);
}

void ResolveInfo::setDesc(uint32_t Desc) {
  ThisBitField &= ~DescMask;
  ThisBitField |= ((Desc << DescOffset) & DescMask);
}

void ResolveInfo::setBinding(uint32_t Binding) {
  ThisBitField &= ~BindingMask;
  if (Binding == Local || Binding == Absolute)
    ThisBitField |= LocalFlag;
  if (Binding == Weak || Binding == Absolute)
    ThisBitField |= WeakFlag;
}

void ResolveInfo::setReserved(uint32_t Reserved) {
  ThisBitField &= ~ReservedMask;
  ThisBitField |= ((Reserved << ReservedOffset) & ReservedMask);
}

void ResolveInfo::setOther(uint32_t Other) {
  setVisibility(static_cast<ResolveInfo::Visibility>(Other & 0x3));
}

void ResolveInfo::setVisibility(ResolveInfo::Visibility Visibility) {
  ThisBitField &= ~VisibilityMask;
  ThisBitField |= Visibility << VisibilityOffset;
}

void ResolveInfo::setIsSymbol(bool IsSymbol) {
  if (IsSymbol)
    ThisBitField |= SymbolFlag;
  else
    ThisBitField &= ~SymbolFlag;
}

bool ResolveInfo::isNull() const { return (this == null()); }

ResolveInfo *ResolveInfo::null() { return &GNullResolveInfo; }

bool ResolveInfo::isDyn() const {
  return (DynamicFlag == (ThisBitField & DynMask));
}

bool ResolveInfo::isUndef() const {
  return (UndefineFlag == (ThisBitField & DescMask));
}

bool ResolveInfo::isDefine() const {
  return (DefineFlag == (ThisBitField & DescMask));
}

bool ResolveInfo::isCommon() const {
  return (CommonFlag == (ThisBitField & DescMask));
}

bool ResolveInfo::isHidden() const {
  return visibility() == ResolveInfo::Hidden;
}

bool ResolveInfo::isProtected() const {
  return visibility() == ResolveInfo::Protected;
}

// isGlobal - [L,W] == [0, 0]
bool ResolveInfo::isGlobal() const {
  return (GlobalFlag == (ThisBitField & BindingMask));
}

// isWeak - [L,W] == [0, 1]
bool ResolveInfo::isWeak() const {
  return (WeakFlag == (ThisBitField & BindingMask));
}

// isLocal - [L,W] == [1, 0]
bool ResolveInfo::isLocal() const {
  return (LocalFlag == (ThisBitField & BindingMask));
}

// isAbsolute - [L,W] == [1, 1]
bool ResolveInfo::isAbsolute() const {
  return (AbsoluteFlag == (ThisBitField & BindingMask));
}

bool ResolveInfo::isSymbol() const {
  return (SymbolFlag == (ThisBitField & SymbolMask));
}

bool ResolveInfo::isString() const {
  return (StringFlag == (ThisBitField & SymbolMask));
}

bool ResolveInfo::exportToDyn() const {
  return (ExportDynFlag == (ThisBitField & ExportDynMask));
}

uint32_t ResolveInfo::type() const {
  return (ThisBitField & TypeMask) >> TypeOffset;
}

uint32_t ResolveInfo::desc() const {
  return (ThisBitField & DescMask) >> DescOffset;
}

uint32_t ResolveInfo::binding() const {
  if (ThisBitField & LocalMask) {
    if (ThisBitField & GlobalMask) {
      return ResolveInfo::Absolute;
    }
    return ResolveInfo::Local;
  }
  return ThisBitField & GlobalMask;
}

uint32_t ResolveInfo::reserved() const {
  return (ThisBitField & ReservedMask) >> ReservedOffset;
}

ResolveInfo::Visibility ResolveInfo::visibility() const {
  return static_cast<ResolveInfo::Visibility>((ThisBitField & VisibilityMask) >>
                                              VisibilityOffset);
}

void ResolveInfo::setResolvedOrigin(InputFile *Input) {
  SymbolResolvedOrigin = Input;
}

std::string ResolveInfo::infoAsString() const {
  uint32_t SymType = type();
  std::string ResolveType;
  if (isBitCode())
    ResolveType = "(BITCODE)";
  else
    ResolveType = "(ELF)";
  switch (SymType) {
  case NoType:
    ResolveType += "(NOTYPE)";
    break;
  case Object:
    ResolveType += "(OBJECT)";
    break;
  case Function:
    ResolveType += "(FUNCTION)";
    break;
  case File:
    ResolveType += "(FILE)";
    break;
  case CommonBlock:
    ResolveType += "(COMMONBLOCK)";
    break;
  case Section:
    ResolveType += "(SECTION)";
    break;
  case ThreadLocal:
    ResolveType += "(TLS)";
    break;
  default:
    ResolveType += "Unknown";
  }
  uint32_t Symdesc = desc();
  switch (Symdesc) {
  case Undefined:
    ResolveType += "(UNDEFINED)";
    break;
  case Define:
    ResolveType += "(DEFINE)";
    break;
  case Common:
    ResolveType += "(COMMON)";
    break;
  default:
    ResolveType += "(UNKNOWN)";
    break;
  }
  uint32_t SymBinding = binding();
  switch (SymBinding) {
  case Global:
    ResolveType += "[Global]";
    break;
  case Weak:
    ResolveType += "[Weak]";
    break;
  case Local:
    ResolveType += "[Local]";
    break;
  case Absolute:
    ResolveType += "[Absolute]";
    break;
  default:
    ResolveType += "[Unknown]";
    break;
  }
  uint32_t SymVisibility = visibility();
  switch (SymVisibility) {
  case Default:
    ResolveType += "{DEFAULT}";
    break;
  case Internal:
    ResolveType += "{INTERNAL}";
    break;
  case Hidden:
    ResolveType += "{HIDDEN}";
    break;
  case Protected:
    ResolveType += "{PROTECTED}";
    break;
  default:
    break;
  }
  if (isDyn())
    ResolveType += "{DYN}";
  if (shouldPreserve())
    ResolveType += "{PRESERVE}";
  if (outSymbol() && outSymbol()->shouldIgnore())
    ResolveType += "{IGNORE}";
  if (isPatchable())
    ResolveType += "{PATCHABLE}";
  return ResolveType;
}

llvm::StringRef ResolveInfo::getVisibilityString() const {
  switch (visibility()) {
  case Internal:
    return "internal";
  case Protected:
    return "protected";
  case Hidden:
    return "hidden";
  default:
    break;
  }
  return "default";
}

bool ResolveInfo::hasContextualLabel() const {
  return resolvedOrigin() && resolvedOrigin()->getInput() && outSymbol() &&
         outSymbol()->hasFragRef() && getOwningSection();
}

std::string ResolveInfo::getContextualLabel() const {
  return std::to_string(resolvedOrigin()->getInput()->getInputOrdinal()) + "_" +
         std::to_string(getOwningSection()->getIndex()) + "_";
}

std::string ResolveInfo::getDecoratedName(bool DoDeMangle) const {
  std::string DecoratedName = name();
  if (DoDeMangle)
    DecoratedName = eld::string::getDemangledName(name());
  std::optional<std::string> AuxSymName;
  if (ObjectFile *ObjFile =
          llvm::dyn_cast_or_null<ObjectFile>(SymbolResolvedOrigin))
    if (OutputSymbol)
      AuxSymName =
          ObjFile->getAuxiliarySymbolName(OutputSymbol->getSymbolIndex());
  if (AuxSymName) {
    std::string AuxName = AuxSymName.value();
    if (DoDeMangle) {
      AuxName = eld::string::getDemangledName(AuxName);
    }
    DecoratedName += "(" + AuxName + ")";
  }
  return DecoratedName;
}

ELFSection *ResolveInfo::getOwningSection() const {
  return outSymbol()->fragRef()->frag()->getOwningSection();
}
