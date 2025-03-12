//===- LDSymbol.cpp--------------------------------------------------------===//
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

#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/Config/Config.h"
#include "eld/Fragment/FillFragment.h"
#include "eld/Fragment/FragmentRef.h"
#include "eld/Fragment/MergeStringFragment.h"
#include "eld/Input/InputFile.h"
#include "eld/Readers/ELFSection.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ManagedStatic.h"
#include <cstring>

using namespace eld;

static LDSymbol g_NullSymbol;

//===----------------------------------------------------------------------===//
// LDSymbol
//===----------------------------------------------------------------------===//
LDSymbol::LDSymbol(ResolveInfo *R, bool isGC)
    : m_pResolveInfo(R), m_pFragRef(nullptr), m_Shndx(0), m_SymIdx(0),
      m_ScriptDefined(false), m_ScriptValueDefined(false),
      m_ShouldIgnore(isGC) {}

LDSymbol::~LDSymbol() {}

LDSymbol *LDSymbol::Null() { return &g_NullSymbol; }

void LDSymbol::setFragmentRef(FragmentRef *pFragmentRef) {
  m_pFragRef = pFragmentRef;
  Fragment *frag = pFragmentRef->frag();
  // Set zero-sized section as wanted if they have any associated symbol.
  if (!frag || !frag->isZeroSizedFrag())
    return;
  frag->getOwningSection()->setWanted(true);
}

void LDSymbol::setResolveInfo(const ResolveInfo &pInfo) {
  m_pResolveInfo = const_cast<ResolveInfo *>(&pInfo);
}

bool LDSymbol::isNull() const { return (this == Null()); }

bool LDSymbol::hasFragRef() const {
  if (m_pFragRef)
    return !m_pFragRef->isNull() && !m_pFragRef->isDiscard();
  return false;
}

bool LDSymbol::hasFragRefSection() const {
  if (!hasFragRef())
    return false;

  if (!m_pFragRef->frag())
    return false;

  if (!m_pFragRef->frag()->getOwningSection())
    return false;

  return true;
}

uint64_t LDSymbol::getSymbolHash() const {
  return llvm::hash_combine(
      llvm::StringRef(name()), value(),
      resolveInfo()->resolvedOrigin()->getInput()->decoratedPath());
}
