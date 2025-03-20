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
    : Fragment(Fragment::Stub), ThisSymInfo(nullptr), ThisSavedSymInfo(nullptr),
      ThisSize(0) {}

Stub::~Stub() { FixupList.clear(); }

void Stub::setSymInfo(ResolveInfo *CurSymInfo) { ThisSymInfo = CurSymInfo; }

void Stub::addFixup(DWord POffset, SWord PAddend, Type PType) {
  assert(POffset < size());
  FixupList.push_back(make<Fixup>(POffset, PAddend, PType));
}

void Stub::addFixup(const Fixup &PFixup) {
  assert(PFixup.offset() < size());
  FixupList.push_back(make<Fixup>(PFixup));
}

size_t Stub::size() const { return ThisSize; }

eld::Expected<void> Stub::emit(MemoryRegion &Mr, Module &M) {
  memcpy(Mr.begin() + getOffset(M.getConfig().getDiagEngine()), getContent(),
         size());
  return {};
}

std::string
Stub::getTargetSymbolContextForReloc(const Relocation &Reloc,
                                     uint32_t RelocAddend,
                                     bool UseOldStyleTrampolineName) const {
  const ResolveInfo *Info = Reloc.symInfo();
  if (UseOldStyleTrampolineName)
    return Info->name();
  if (!Info->isLocal())
    return Info->name();
  ObjectFile *Objfile =
      llvm::dyn_cast_or_null<ObjectFile>(Info->resolvedOrigin());
  if (!Objfile)
    return Info->name();
  ELFSection *OwningSection = Info->getOwningSection();
  if (!OwningSection)
    return Info->name();
  const ResolveInfo *TargetLocalSymbol =
      Objfile->getMatchingLocalSymbol(OwningSection->getIndex(), RelocAddend);
  if (!TargetLocalSymbol)
    return Info->name();
  return std::string(Info->name()) + "#(" + TargetLocalSymbol->name() + ")";
}

std::string Stub::getStubName(const Relocation &PReloc, bool IsClone,
                              bool IsSectionRelative, int64_t NumBranchIsland,
                              int64_t NumClone, uint32_t RelocAddend,
                              bool UseOldStyleTrampolineName) const {
  const ResolveInfo *Info = PReloc.symInfo();
  std::stringstream Ss;
  if (IsClone)
    Ss << "clone_" << NumClone << "_for_" << Info->name();
  else if (PReloc.targetRef() && PReloc.targetRef()->frag())
    Ss << "trampoline_for_"
       << getTargetSymbolContextForReloc(PReloc, RelocAddend,
                                         UseOldStyleTrampolineName)
       << "_from_"
       << PReloc.targetRef()->frag()->getOwningSection()->name().str() << "_"
       << PReloc.targetRef()
              ->frag()
              ->getOwningSection()
              ->getInputFile()
              ->getInput()
              ->getInputOrdinal();
  else
    Ss << "trampoline_" << NumBranchIsland << "_for_" << Info->name();

  if (IsSectionRelative)
    Ss << "#"
       << "(" << RelocAddend << ")";
  return Ss.str();
}
