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
  // Garbage collection marks dead sections as Ignore. Explicitly discarded
  // sections must not be candidates either.
  if (Section.isIgnore() || Section.isDiscard())
    return false;

  // Only allocated sections contribute code to the output image.
  if (!Section.isAlloc())
    return false;

  // This initial pass folds only read-only executable PROGBITS sections.
  if (!Section.isCode() || !Section.isProgBits() || Section.isWritable() ||
      Section.isTLS() || Section.isMergeKind())
    return false;

  // Link-order and group sections have relationships that this pass does not
  // model.
  if (Section.isLinkOrder() || Section.isInGroup())
    return false;

  // Fixed-address sections must retain their original placement.
  if (Section.isFixedAddr())
    return false;

  // Relocation-aware equivalence is not implemented yet.
  if (Section.hasRelocData() || Section.isFolded())
    return false;

  // .init and .fini are concatenated instruction streams, not independent
  // functions, so their contents must not be folded.
  if (Section.name() == ".init" || Section.name() == ".fini")
    return false;

  // A single fragment preserves every symbol's section-relative offset.
  return Section.fragmentCount() == 1 && Section.size() != 0;
}

bool haveSameIdentity(const ELFSection &A, const ELFSection &B) {
  return A.getOutputSection() == B.getOutputSection() &&
         A.getFlags() == B.getFlags() && A.getContents() == B.getContents();
}

using SectionSymbolDefinitions =
    llvm::DenseMap<ELFSection *, llvm::SmallVector<FragmentRef *, 0>>;

void collectSymbolDefinition(SectionSymbolDefinitions &Definitions,
                             LDSymbol *Symbol) {
  if (!Symbol || !Symbol->hasFragRef())
    return;
  FragmentRef *Ref = Symbol->fragRef();
  if (!Ref)
    return;
  auto *Section = llvm::dyn_cast<ELFSection>(Ref->frag()->getOwningSection());
  if (Section)
    Definitions[Section].push_back(Ref);
}

// Index symbol definitions once so folding a section visits only its symbols.
SectionSymbolDefinitions collectSectionSymbolDefinitions(Module &Module) {
  SectionSymbolDefinitions Definitions;
  for (InputFile *Input : Module.getObjectList()) {
    auto *Object = llvm::dyn_cast<ObjectFile>(Input);
    if (!Object)
      continue;
    for (LDSymbol *Symbol : Object->getSymbols())
      collectSymbolDefinition(Definitions, Symbol);
    for (LDSymbol *Symbol : Object->getLocalSymbols())
      collectSymbolDefinition(Definitions, Symbol);
  }
  return Definitions;
}

// Bind every symbol defined in the Folded section to the Canonical section.
void retargetSymbols(SectionSymbolDefinitions &Definitions, ELFSection &Folded,
                     ELFSection &Canonical) {
  auto It = Definitions.find(&Folded);
  if (It == Definitions.end())
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

  // Plugins can replace symbol contents after ICF and folding could make a
  // plugin update affect multiple functions through the shared fragment.
  // TODO: Return only for plugins that may replace symbol content instead of
  // doing it for all plugins.
  if (!ThisModule.getScript().getPlugins().empty()) {
    ThisConfig.raise(Diag::warn_icf_disabled_plugins);
    return;
  }

  SectionSymbolDefinitions Definitions =
      collectSectionSymbolDefinitions(ThisModule);
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

  // TODO: Sort section hashes and compare groups within the same hash similar
  // to what LLD does.
  for (size_t I = 0; I < Candidates.size(); ++I) {
    ELFSection *Canonical = Candidates[I];
    if (Canonical->isFolded())
      continue;
    for (size_t J = I + 1; J < Candidates.size(); ++J) {
      ELFSection *Folded = Candidates[J];
      if (Folded->isFolded() || !haveSameIdentity(*Canonical, *Folded))
        continue;
      retargetSymbols(Definitions, *Folded, *Canonical);
      Fragment *CanonicalFragment = Canonical->getFrontFragment();
      Fragment *FoldedFragment = Folded->getFrontFragment();
      uint32_t Alignment = std::max(
          std::max(Canonical->getAddrAlign(), Folded->getAddrAlign()),
          static_cast<uint32_t>(std::max(CanonicalFragment->alignment(),
                                         FoldedFragment->alignment())));
      // Layout uses the fragment alignment, so preserve the folded section's
      // stronger requirement on the canonical fragment for retargeted symbols.
      Canonical->setAddrAlign(Alignment);
      CanonicalFragment->setAlignment(Alignment);
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
