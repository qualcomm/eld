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
ScriptSymbol::ScriptSymbol(const std::string &pString)
    : WildcardPattern(Saver.save(pString)) {}

eld::Expected<void> ScriptSymbol::activate() {
  assert(!m_SymbolContainer);
  m_SymbolContainer = eld::make<SymbolContainer>(*this);
  return eld::Expected<void>();
}

void ScriptSymbol::addResolveInfoToContainer(const ResolveInfo *info) const {
  assert(m_SymbolContainer);
  m_SymbolContainer->addResolveInfo(info);
}

bool ScriptSymbol::matched(const ResolveInfo &sym) const {
  llvm::StringRef name = sym.name();
  uint64_t hash = llvm::hash_combine(name);
  if ((hasHash() && hashValue() == hash) || WildcardPattern::matched(name)) {
    addResolveInfoToContainer(&sym);
    return true;
  }
  return false;
}
