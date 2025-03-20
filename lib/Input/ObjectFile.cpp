//===- ObjectFile.cpp------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Input/ObjectFile.h"
#include "eld/Fragment/Fragment.h"
#include "eld/Readers/ELFSection.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/Casting.h"
#include <cstdint>
#include <utility>

using namespace eld;

void ObjectFile::addSection(Section *S) { MSectionTable.push_back(S); }

LDSymbol *ObjectFile::getLocalSymbol(unsigned int PIdx) const {
  if (PIdx >= MLocalSymTab.size())
    return nullptr;
  return MLocalSymTab[PIdx];
}

LDSymbol *ObjectFile::getSymbol(unsigned int PIdx) const {
  if (PIdx >= SymTab.size())
    return nullptr;
  return SymTab[PIdx];
}

LDSymbol *ObjectFile::getSymbol(std::string PName) const {
  for (auto *Sym : SymTab) {
    if (Sym->name() == PName)
      return Sym;
  }
  return nullptr;
}

LDSymbol *ObjectFile::getLocalSymbol(llvm::StringRef PName) const {
  for (auto *Lsym : MLocalSymTab) {
    if (Lsym->name() == PName)
      return Lsym;
  }
  return nullptr;
}

void ObjectFile::addSymbol(LDSymbol *S) {
  SymTab.push_back(S);
  const ResolveInfo *Info = S->resolveInfo();
  if (!Info)
    return;
  if (!Info->isLocal() || Info->isSection())
    return;
  LocalSymbolInfoMap[std::make_pair(S->sectionIndex(), S->value())] = S;
}

bool ObjectFile::classof(const InputFile *E) {
  return E->isObjectFile() || E->isBitcode() || E->isDynamicLibrary() ||
         E->isInternal() || E->isExecutableELFFile() || E->isBinaryFile();
}

std::string ObjectFile::getFeaturesStr() const {
  return llvm::join(Features.begin(), Features.end(), ",");
}

const ResolveInfo *ObjectFile::getMatchingLocalSymbol(uint64_t SectionIndex,
                                                      uint64_t Value) const {
  std::pair<uint64_t, uint64_t> Entry = std::make_pair(SectionIndex, Value);
  if (LocalSymbolInfoMap.find(Entry) == LocalSymbolInfoMap.end())
    return nullptr;
  LDSymbol *Sym = LocalSymbolInfoMap.at(Entry);
  if (!Sym)
    return nullptr;
  return Sym->resolveInfo();
}
