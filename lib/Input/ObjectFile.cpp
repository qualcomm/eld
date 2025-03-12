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

void ObjectFile::addSection(Section *S) { m_SectionTable.push_back(S); }

LDSymbol *ObjectFile::getLocalSymbol(unsigned int pIdx) const {
  if (pIdx >= m_LocalSymTab.size())
    return nullptr;
  return m_LocalSymTab[pIdx];
}

LDSymbol *ObjectFile::getSymbol(unsigned int pIdx) const {
  if (pIdx >= m_SymTab.size())
    return nullptr;
  return m_SymTab[pIdx];
}

LDSymbol *ObjectFile::getSymbol(std::string pName) const {
  for (auto sym : m_SymTab) {
    if (sym->name() == pName)
      return sym;
  }
  return nullptr;
}

LDSymbol *ObjectFile::getLocalSymbol(llvm::StringRef pName) const {
  for (auto lsym : m_LocalSymTab) {
    if (lsym->name() == pName)
      return lsym;
  }
  return nullptr;
}

void ObjectFile::addSymbol(LDSymbol *S) {
  m_SymTab.push_back(S);
  const ResolveInfo *info = S->resolveInfo();
  if (!info)
    return;
  if (!info->isLocal() || info->isSection())
    return;
  m_LocalSymbolInfoMap[std::make_pair(S->sectionIndex(), S->value())] = S;
}

bool ObjectFile::classof(const InputFile *E) {
  return E->isObjectFile() || E->isBitcode() || E->isDynamicLibrary() ||
         E->isInternal() || E->isExecutableELFFile() || E->isBinaryFile();
}

std::string ObjectFile::getFeaturesStr() const {
  return llvm::join(m_Features.begin(), m_Features.end(), ",");
}

const ResolveInfo *ObjectFile::getMatchingLocalSymbol(uint64_t sectionIndex,
                                                      uint64_t value) const {
  std::pair<uint64_t, uint64_t> entry = std::make_pair(sectionIndex, value);
  if (m_LocalSymbolInfoMap.find(entry) == m_LocalSymbolInfoMap.end())
    return nullptr;
  LDSymbol *sym = m_LocalSymbolInfoMap.at(entry);
  if (!sym)
    return nullptr;
  return sym->resolveInfo();
}
