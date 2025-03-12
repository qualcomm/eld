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
static ResolveInfo g_NullResolveInfo;

//===----------------------------------------------------------------------===//
// ResolveInfo
//===----------------------------------------------------------------------===//
ResolveInfo::ResolveInfo()
    : m_Size(0), m_Value(0), m_BitField(0), m_Name(""), m_Alias(nullptr),
      m_pResolvedOrigin(nullptr) {
  m_SymPtr = nullptr;
}

ResolveInfo::ResolveInfo(llvm::StringRef Name)
    : m_Size(0), m_Value(0), m_BitField(0), m_Name(Name), m_Alias(nullptr),
      m_pResolvedOrigin(nullptr) {
  m_SymPtr = nullptr;
}

ResolveInfo::~ResolveInfo() {}

void ResolveInfo::override(const ResolveInfo &pFrom, bool pOverrideOrigin) {
  m_Size = pFrom.m_Size;
  setValue(pFrom.value(), false);
  overrideAttributes(pFrom);
  if (pOverrideOrigin) {
    setResolvedOrigin(pFrom.resolvedOrigin());
    setAlias(pFrom.alias());
  }
}

void ResolveInfo::overrideAttributes(const ResolveInfo &pFrom) {
  // Do not override visibility
  auto v = visibility();
  bool p = shouldPreserve();
  m_BitField &= ~RESOLVE_MASK;
  m_BitField |= (pFrom.m_BitField & RESOLVE_MASK);
  shouldPreserve(p);
  setVisibility(v);
}

/// overrideVisibility - override the visibility
///   always use the most strict visibility
void ResolveInfo::overrideVisibility(const ResolveInfo &pFrom) {
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
  Visibility from_vis = pFrom.visibility();
  Visibility cur_vis = visibility();
  if (0 != from_vis) {
    if (0 == cur_vis)
      setVisibility(from_vis);
    else if (cur_vis > from_vis)
      setVisibility(from_vis);
  }
}

void ResolveInfo::setRegular() { m_BitField &= (~dynamic_flag); }

void ResolveInfo::setDynamic() { m_BitField |= dynamic_flag; }

void ResolveInfo::setSource(bool pIsDyn) {
  if (pIsDyn)
    m_BitField |= dynamic_flag;
  else
    m_BitField &= (~dynamic_flag);
}

void ResolveInfo::setExportToDyn() { m_BitField |= export_dyn_flag; }
void ResolveInfo::clearExportToDyn() { m_BitField &= ~export_dyn_flag; }

void ResolveInfo::setType(uint32_t pType) {
  m_BitField &= ~TYPE_MASK;
  m_BitField |= ((pType << TYPE_OFFSET) & TYPE_MASK);
}

void ResolveInfo::setDesc(uint32_t pDesc) {
  m_BitField &= ~DESC_MASK;
  m_BitField |= ((pDesc << DESC_OFFSET) & DESC_MASK);
}

void ResolveInfo::setBinding(uint32_t pBinding) {
  m_BitField &= ~BINDING_MASK;
  if (pBinding == Local || pBinding == Absolute)
    m_BitField |= local_flag;
  if (pBinding == Weak || pBinding == Absolute)
    m_BitField |= weak_flag;
}

void ResolveInfo::setReserved(uint32_t pReserved) {
  m_BitField &= ~RESERVED_MASK;
  m_BitField |= ((pReserved << RESERVED_OFFSET) & RESERVED_MASK);
}

void ResolveInfo::setOther(uint32_t pOther) {
  setVisibility(static_cast<ResolveInfo::Visibility>(pOther & 0x3));
}

void ResolveInfo::setVisibility(ResolveInfo::Visibility pVisibility) {
  m_BitField &= ~VISIBILITY_MASK;
  m_BitField |= pVisibility << VISIBILITY_OFFSET;
}

void ResolveInfo::setIsSymbol(bool pIsSymbol) {
  if (pIsSymbol)
    m_BitField |= symbol_flag;
  else
    m_BitField &= ~symbol_flag;
}

bool ResolveInfo::isNull() const { return (this == Null()); }

ResolveInfo *ResolveInfo::Null() { return &g_NullResolveInfo; }

bool ResolveInfo::isDyn() const {
  return (dynamic_flag == (m_BitField & DYN_MASK));
}

bool ResolveInfo::isUndef() const {
  return (undefine_flag == (m_BitField & DESC_MASK));
}

bool ResolveInfo::isDefine() const {
  return (define_flag == (m_BitField & DESC_MASK));
}

bool ResolveInfo::isCommon() const {
  return (common_flag == (m_BitField & DESC_MASK));
}

bool ResolveInfo::isHidden() const {
  return visibility() == ResolveInfo::Hidden;
}

bool ResolveInfo::isProtected() const {
  return visibility() == ResolveInfo::Protected;
}

// isGlobal - [L,W] == [0, 0]
bool ResolveInfo::isGlobal() const {
  return (global_flag == (m_BitField & BINDING_MASK));
}

// isWeak - [L,W] == [0, 1]
bool ResolveInfo::isWeak() const {
  return (weak_flag == (m_BitField & BINDING_MASK));
}

// isLocal - [L,W] == [1, 0]
bool ResolveInfo::isLocal() const {
  return (local_flag == (m_BitField & BINDING_MASK));
}

// isAbsolute - [L,W] == [1, 1]
bool ResolveInfo::isAbsolute() const {
  return (absolute_flag == (m_BitField & BINDING_MASK));
}

bool ResolveInfo::isSymbol() const {
  return (symbol_flag == (m_BitField & SYMBOL_MASK));
}

bool ResolveInfo::isString() const {
  return (string_flag == (m_BitField & SYMBOL_MASK));
}

bool ResolveInfo::exportToDyn() const {
  return (export_dyn_flag == (m_BitField & EXPORT_DYN_MASK));
}

uint32_t ResolveInfo::type() const {
  return (m_BitField & TYPE_MASK) >> TYPE_OFFSET;
}

uint32_t ResolveInfo::desc() const {
  return (m_BitField & DESC_MASK) >> DESC_OFFSET;
}

uint32_t ResolveInfo::binding() const {
  if (m_BitField & LOCAL_MASK) {
    if (m_BitField & GLOBAL_MASK) {
      return ResolveInfo::Absolute;
    }
    return ResolveInfo::Local;
  }
  return m_BitField & GLOBAL_MASK;
}

uint32_t ResolveInfo::reserved() const {
  return (m_BitField & RESERVED_MASK) >> RESERVED_OFFSET;
}

ResolveInfo::Visibility ResolveInfo::visibility() const {
  return static_cast<ResolveInfo::Visibility>((m_BitField & VISIBILITY_MASK) >>
                                              VISIBILITY_OFFSET);
}

void ResolveInfo::setResolvedOrigin(InputFile *pInput) {
  m_pResolvedOrigin = pInput;
}

std::string ResolveInfo::infoAsString() const {
  uint32_t symType = type();
  std::string resolveType;
  if (isBitCode())
    resolveType = "(BITCODE)";
  else
    resolveType = "(ELF)";
  switch (symType) {
  case NoType:
    resolveType += "(NOTYPE)";
    break;
  case Object:
    resolveType += "(OBJECT)";
    break;
  case Function:
    resolveType += "(FUNCTION)";
    break;
  case File:
    resolveType += "(FILE)";
    break;
  case CommonBlock:
    resolveType += "(COMMONBLOCK)";
    break;
  case Section:
    resolveType += "(SECTION)";
    break;
  case ThreadLocal:
    resolveType += "(TLS)";
    break;
  default:
    resolveType += "Unknown";
  }
  uint32_t symdesc = desc();
  switch (symdesc) {
  case Undefined:
    resolveType += "(UNDEFINED)";
    break;
  case Define:
    resolveType += "(DEFINE)";
    break;
  case Common:
    resolveType += "(COMMON)";
    break;
  default:
    resolveType += "(UNKNOWN)";
    break;
  }
  uint32_t symBinding = binding();
  switch (symBinding) {
  case Global:
    resolveType += "[Global]";
    break;
  case Weak:
    resolveType += "[Weak]";
    break;
  case Local:
    resolveType += "[Local]";
    break;
  case Absolute:
    resolveType += "[Absolute]";
    break;
  default:
    resolveType += "[Unknown]";
    break;
  }
  uint32_t symVisibility = visibility();
  switch (symVisibility) {
  case Default:
    resolveType += "{DEFAULT}";
    break;
  case Internal:
    resolveType += "{INTERNAL}";
    break;
  case Hidden:
    resolveType += "{HIDDEN}";
    break;
  case Protected:
    resolveType += "{PROTECTED}";
    break;
  default:
    break;
  }
  if (isDyn())
    resolveType += "{DYN}";
  if (shouldPreserve())
    resolveType += "{PRESERVE}";
  if (outSymbol() && outSymbol()->shouldIgnore())
    resolveType += "{IGNORE}";
  if (isPatchable())
    resolveType += "{PATCHABLE}";
  return resolveType;
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
  std::string decoratedName = name();
  if (DoDeMangle)
    decoratedName = eld::string::getDemangledName(name());
  std::optional<std::string> auxSymName;
  if (ObjectFile *objFile =
          llvm::dyn_cast_or_null<ObjectFile>(m_pResolvedOrigin))
    if (m_SymPtr)
      auxSymName = objFile->getAuxiliarySymbolName(m_SymPtr->getSymbolIndex());
  if (auxSymName) {
    std::string auxName = auxSymName.value();
    if (DoDeMangle) {
      auxName = eld::string::getDemangledName(auxName);
    }
    decoratedName += "(" + auxName + ")";
  }
  return decoratedName;
}

ELFSection *ResolveInfo::getOwningSection() const {
  return outSymbol()->fragRef()->frag()->getOwningSection();
}
