//===- ICF.cpp-------------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//
// This file implements an initial conservative version of identical code
// folding (ICF). It discovers and merges equivalent relocation-free executable
// input sections. The pass retains the first section as the canonical
// representative, moves all symbol references to that representative, and
// discards the duplicate before section merging and layout.
// The pass runs after GC and before input sections are merged into their output
// sections.
//
// FIXME: Add support for relocation-aware equivalence checking.
//===----------------------------------------------------------------------===//

#include "eld/Core/ICF.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Core/Module.h"
#include "eld/Fragment/Fragment.h"
#include "eld/Fragment/FragmentRef.h"
#include "eld/Input/ObjectFile.h"
#include "eld/Readers/ELFSection.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Casting.h"
#include <algorithm>

using namespace eld;

namespace {

bool isEligibleForICF(const ELFSection &Section) {
  // This implementation is deliberately narrow - only simple,
  // relocation-free executable input sections can be folded.
  if (!Section.isAlloc() || !Section.isCode() || !Section.isProgBits() ||
      Section.isWritable() || Section.isTLS() || Section.isMergeKind() ||
      Section.isLinkOrder() || Section.isInGroup() || Section.isFixedAddr() ||
      Section.hasRelocData() || Section.isFolded())
    return false;

  // A single fragment preserves every symbol's section-relative offset.
  return Section.fragmentCount() == 1 && Section.size() != 0;
}

bool haveSameIdentity(const ELFSection &A, const ELFSection &B) {
  return A.getOutputSection() == B.getOutputSection() &&
         A.getFlags() == B.getFlags() && A.getContents() == B.getContents();
}

using SectionReferences =
    llvm::DenseMap<ELFSection *, llvm::SmallVector<FragmentRef *, 0>>;

void collectSymbolReference(SectionReferences &References, LDSymbol *Symbol) {
  if (!Symbol || !Symbol->hasFragRef())
    return;
  FragmentRef *Ref = Symbol->fragRef();
  if (!Ref)
    return;
  auto *Section = llvm::dyn_cast<ELFSection>(Ref->frag()->getOwningSection());
  if (Section)
    References[Section].push_back(Ref);
}

// Index symbol references once so folding a section visits only its references.
SectionReferences collectSectionReferences(Module &Module) {
  SectionReferences References;
  for (InputFile *Input : Module.getObjectList()) {
    auto *Object = llvm::dyn_cast<ObjectFile>(Input);
    if (!Object)
      continue;
    for (LDSymbol *Symbol : Object->getSymbols())
      collectSymbolReference(References, Symbol);
    for (LDSymbol *Symbol : Object->getLocalSymbols())
      collectSymbolReference(References, Symbol);
  }
  return References;
}

// Bind every symbol referring to the Folded section to the Canonical section.
void retargetSymbols(SectionReferences &References, ELFSection &Folded,
                     ELFSection &Canonical) {
  auto It = References.find(&Folded);
  if (It == References.end())
    return;
  Fragment *To = Canonical.getFrontFragment();
  for (FragmentRef *Ref : It->second)
    Ref->setFragment(To);
}

} // namespace

// Discover and fold each equivalence class in input order.
void ICF::run() {
  if (!ThisConfig.options().icfEnabled())
    return;

  SectionReferences References = collectSectionReferences(ThisModule);
  llvm::SmallVector<ELFSection *, 0> Candidates;
  for (InputFile *Input : ThisModule.getObjectList()) {
    auto *Object = llvm::dyn_cast<ObjectFile>(Input);
    if (!Object)
      continue;
    for (Section *Section : Object->getSections()) {
      auto *ELF = llvm::dyn_cast<ELFSection>(Section);
      if (isEligibleForICF(*ELF))
        Candidates.push_back(ELF);
    }
  }

  for (size_t I = 0; I < Candidates.size(); ++I) {
    ELFSection *Canonical = Candidates[I];
    if (Canonical->isFolded())
      continue;
    for (size_t J = I + 1; J < Candidates.size(); ++J) {
      ELFSection *Folded = Candidates[J];
      if (Folded->isFolded() || !haveSameIdentity(*Canonical, *Folded))
        continue;
      retargetSymbols(References, *Folded, *Canonical);
      Canonical->setAddrAlign(
          std::max(Canonical->getAddrAlign(), Folded->getAddrAlign()));
      Folded->setFolded();
      Folded->setKind(LinkerSectionKind::Discard);
      if (ThisConfig.options().printICFSections())
        ThisConfig.raise(Diag::trace_icf_section)
            << Folded->originalInput()->getInput()->decoratedPath()
            << Folded->getDecoratedName(ThisConfig.options())
            << Canonical->originalInput()->getInput()->decoratedPath()
            << Canonical->getDecoratedName(ThisConfig.options());
    }
  }
}
