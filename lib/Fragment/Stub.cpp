//===- Stub.cpp------------------------------------------------------------===//
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

#include "eld/Fragment/Stub.h"
#include "eld/Core/Module.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Readers/Relocation.h"
#include "eld/Support/Memory.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "llvm/Support/Casting.h"
#include <sstream>
#include <string>

using namespace eld;

Stub::Stub()
    : Fragment(Fragment::Stub), m_pSymInfo(nullptr), m_pSavedSymInfo(nullptr),
      m_Size(0) {}

Stub::~Stub() { m_FixupList.clear(); }

void Stub::setSymInfo(ResolveInfo *pSymInfo) { m_pSymInfo = pSymInfo; }

void Stub::addFixup(DWord pOffset, SWord pAddend, Type pType) {
  assert(pOffset < size());
  m_FixupList.push_back(make<Fixup>(pOffset, pAddend, pType));
}

void Stub::addFixup(const Fixup &pFixup) {
  assert(pFixup.offset() < size());
  m_FixupList.push_back(make<Fixup>(pFixup));
}

size_t Stub::size() const { return m_Size; }

eld::Expected<void> Stub::emit(MemoryRegion &mr, Module &M) {
  memcpy(mr.begin() + getOffset(M.getConfig().getDiagEngine()), getContent(),
         size());
  return {};
}

std::string
Stub::getTargetSymbolContextForReloc(const Relocation &reloc,
                                     uint32_t relocAddend,
                                     bool useOldStyleTrampolineName) const {
  const ResolveInfo *info = reloc.symInfo();
  if (useOldStyleTrampolineName)
    return info->name();
  if (!info->isLocal())
    return info->name();
  ObjectFile *objfile =
      llvm::dyn_cast_or_null<ObjectFile>(info->resolvedOrigin());
  if (!objfile)
    return info->name();
  ELFSection *owningSection = info->getOwningSection();
  if (!owningSection)
    return info->name();
  const ResolveInfo *targetLocalSymbol =
      objfile->getMatchingLocalSymbol(owningSection->getIndex(), relocAddend);
  if (!targetLocalSymbol)
    return info->name();
  return std::string(info->name()) + "#(" + targetLocalSymbol->name() + ")";
}

std::string Stub::getStubName(const Relocation &pReloc, bool isClone,
                              bool isSectionRelative, int64_t numBranchIsland,
                              int64_t numClone, uint32_t relocAddend,
                              bool useOldStyleTrampolineName) const {
  const ResolveInfo *info = pReloc.symInfo();
  std::stringstream ss;
  if (isClone)
    ss << "clone_" << numClone << "_for_" << info->name();
  else if (pReloc.targetRef() && pReloc.targetRef()->frag())
    ss << "trampoline_for_"
       << getTargetSymbolContextForReloc(pReloc, relocAddend,
                                         useOldStyleTrampolineName)
       << "_from_"
       << pReloc.targetRef()->frag()->getOwningSection()->name().str() << "_"
       << pReloc.targetRef()
              ->frag()
              ->getOwningSection()
              ->getInputFile()
              ->getInput()
              ->getInputOrdinal();
  else
    ss << "trampoline_" << numBranchIsland << "_for_" << info->name();

  if (isSectionRelative)
    ss << "#"
       << "(" << relocAddend << ")";
  return ss.str();
}
