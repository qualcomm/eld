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

static LDSymbol GNullSymbol;

//===----------------------------------------------------------------------===//
// LDSymbol
//===----------------------------------------------------------------------===//
LDSymbol::LDSymbol(ResolveInfo *R, bool IsGc)
    : ThisResolveInfo(R), ThisFragRef(nullptr), ThisShndx(0), ThisSymIdx(0),
      ThisSymbolsIsScriptDefined(false), ThisSymbolHasScriptValue(false),
      ThisSymbolGarbageCollected(IsGc) {}

LDSymbol::~LDSymbol() {}

LDSymbol *LDSymbol::null() { return &GNullSymbol; }

void LDSymbol::setFragmentRef(FragmentRef *CurFragmentRef) {
  ThisFragRef = CurFragmentRef;
  Fragment *Frag = CurFragmentRef->frag();
  // Set zero-sized section as wanted if they have any associated symbol.
  if (!Frag || !Frag->isZeroSizedFrag())
    return;
  Frag->getOwningSection()->setWanted(true);
}

void LDSymbol::setResolveInfo(const ResolveInfo &CurInfo) {
  ThisResolveInfo = const_cast<ResolveInfo *>(&CurInfo);
}

bool LDSymbol::isNull() const { return (this == null()); }

bool LDSymbol::hasFragRef() const {
  if (ThisFragRef)
    return !ThisFragRef->isNull() && !ThisFragRef->isDiscard();
  return false;
}

bool LDSymbol::hasFragRefSection() const {
  if (!hasFragRef())
    return false;

  if (!ThisFragRef->frag())
    return false;

  if (!ThisFragRef->frag()->getOwningSection())
    return false;

  return true;
}

uint64_t LDSymbol::getSymbolHash() const {
  return llvm::hash_combine(
      llvm::StringRef(name()), value(),
      resolveInfo()->resolvedOrigin()->getInput()->decoratedPath());
}
