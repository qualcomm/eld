//===- ScriptSymbol.cpp----------------------------------------------------===//
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

#include "eld/Script/ScriptSymbol.h"
#include "eld/Script/SymbolContainer.h"
#include "eld/Script/WildcardPattern.h"
#include "eld/Support/Memory.h"
#include "llvm/ADT/StringRef.h"
#include <cstdint>

using namespace eld;

//===----------------------------------------------------------------------===//
// ScriptSymbol
//===----------------------------------------------------------------------===//
ScriptSymbol::ScriptSymbol(const std::string &PString)
    : WildcardPattern(Saver.save(PString)) {}

eld::Expected<void> ScriptSymbol::activate() {
  assert(!ThisSymbolContainer);
  ThisSymbolContainer = eld::make<SymbolContainer>(*this);
  return eld::Expected<void>();
}

void ScriptSymbol::addResolveInfoToContainer(const ResolveInfo *Info) const {
  assert(ThisSymbolContainer);
  ThisSymbolContainer->addResolveInfo(Info);
}

bool ScriptSymbol::matched(const ResolveInfo &Sym) const {
  llvm::StringRef Name = Sym.name();
  uint64_t Hash = llvm::hash_combine(Name);
  if ((hasHash() && hashValue() == Hash) || WildcardPattern::matched(Name)) {
    addResolveInfoToContainer(&Sym);
    return true;
  }
  return false;
}
