//===- AArch64ErrataIslandFactory.cpp--------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "AArch64ErrataIslandFactory.h"
#include "AArch64InsnHelpers.h"
#include "eld/Core/Module.h"
#include "eld/Fragment/Fragment.h"
#include "eld/LayoutMap/LayoutPrinter.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Support/Memory.h"
#include "eld/Support/MsgHandling.h"
#include "eld/Target/Relocator.h"
#include <sstream>

using namespace eld;

//===----------------------------------------------------------------------===//
// AArch64ErrataIslandFactory
//===----------------------------------------------------------------------===//

/// ctor
AArch64ErrataIslandFactory::AArch64ErrataIslandFactory()
    : numAArch64ErrataIsland(0) {}

AArch64ErrataIslandFactory::~AArch64ErrataIslandFactory() {}

/// createAArch64ErrataIsland - create a errata island
BranchIsland *AArch64ErrataIslandFactory::createAArch64ErrataIsland(
    Fragment *frag, uint32_t pOffset, Stub *stub, eld::IRBuilder &pBuilder) {
  LayoutPrinter *layoutPrinter = pBuilder.getModule().getLayoutPrinter();
  DiagnosticEngine *DiagEngine =
      pBuilder.getModule().getConfig().getDiagEngine();
  ELFSection *outputELFSection = frag->getOutputELFSection();

  Stub *clone = stub->clone(nullptr, nullptr, nullptr, DiagEngine);

  BranchIsland *island = make<BranchIsland>(clone);

  numAArch64ErrataIsland++;

  std::stringstream ss;

  ss << "__errata_stub__#" << numAArch64ErrataIsland << "_for_"
     << outputELFSection->name().str();

  std::string name(ss.str());

  InputFile *trampolineInput = pBuilder.getModule().getInternalInput(
      Module::InternalInputType::Trampoline);

  // create LDSymbol for the stub
  LDSymbol *symbol = pBuilder.AddSymbol<IRBuilder::Force, IRBuilder::Resolve>(
      trampolineInput, name, ResolveInfo::Function, ResolveInfo::Define,
      ResolveInfo::Local,
      stub->size(),         // size
      stub->initSymValue(), // value
      make<FragmentRef>(*clone, clone->initSymValue()), ResolveInfo::Default,
      true /* isPostLTOPhase */);
  symbol->setShouldIgnore(false);
  pBuilder.getModule().addSymbol(symbol->resolveInfo());
  if (pBuilder.getModule().getConfig().options().isSymbolTracingRequested() &&
      pBuilder.getModule().getConfig().options().traceSymbol(name))
    DiagEngine->raise(diag::target_specific_symbol) << name;

  clone->setSymInfo(symbol->resolveInfo());

  ss.str("");
  ss << "__errata_return_" << numAArch64ErrataIsland << "_for_"
     << outputELFSection->name().str();

  std::string returnName(ss.str());
  LDSymbol *returnSymbol =
      pBuilder.AddSymbol<IRBuilder::Force, IRBuilder::Resolve>(
          frag->getOwningSection()->getInputFile(), returnName,
          ResolveInfo::Function, ResolveInfo::Define, ResolveInfo::Local,
          4, // size
          0, // value
          make<FragmentRef>(*frag, pOffset + 4), ResolveInfo::Default,
          true /* isPostLTOPhase */);
  returnSymbol->setShouldIgnore(false);
  pBuilder.getModule().addSymbol(returnSymbol->resolveInfo());
  if (pBuilder.getModule().getConfig().options().isSymbolTracingRequested() &&
      pBuilder.getModule().getConfig().options().traceSymbol(returnName))
    DiagEngine->raise(diag::target_specific_symbol) << returnName;
  // Set the branch target of this cloned stub.
  for (Stub::fixup_iterator it = clone->fixup_begin(), ie = clone->fixup_end();
       it != ie; ++it) {

    Relocation *reloc = Relocation::Create(
        (*it)->type(),
        pBuilder.getModule().getBackend()->getRelocator()->getSize(
            (*it)->type()),
        make<FragmentRef>(*clone, (*it)->offset()), 0);
    if ((*it)->type() == llvm::ELF::R_AARCH64_JUMP26) {
      reloc->setSymInfo(returnSymbol->resolveInfo());
      reloc->target() = AArch64InsnHelpers::buildBranchInsn();
    } else
      reloc->setSymInfo(returnSymbol->resolveInfo());
    island->addRelocation(*reloc);
  }
  Relocation *reloc = Relocation::Create(llvm::ELF::R_AARCH64_JUMP26, 32,
                                         make<FragmentRef>(*frag, pOffset), 0);
  reloc->setSymInfo(symbol->resolveInfo());
  reloc->target() = AArch64InsnHelpers::buildBranchInsn();
  island->addRelocation(*reloc);

  typedef llvm::SmallVectorImpl<Fragment *>::iterator Iter;

  // Current nodes.
  Fragment *curNode = frag;

  RuleContainer *MatchedRule =
      curNode->getOwningSection()->getMatchedLinkerScriptRule();
  ELFSection *MatchedSection = MatchedRule->getSection();

  Iter curNodeIter = curNode->getIterator();
  Iter endNodeIter = MatchedSection->getFragmentList().end();

  int64_t fragAddr = 0;
  bool isLastFragInSection = false;

  // Default stub offset.
  size_t stubOffset = curNode->getOffset(DiagEngine) + curNode->size();

  // If the fragment is the only fragment in the section, lets go to the end of
  // the list and try appending the stub.
  if ((curNodeIter + 1) == endNodeIter) {
    fragAddr = outputELFSection->addr() + stubOffset;
    isLastFragInSection = true;
  }

  // SEARCH FORWARD FOR A PLACE TO HOLD THE STUB, THATS AFTER ALL STUBS.j
  // | curNode | Align | Stub' | Align | Stub" | Align | Region |
  if (!isLastFragInSection) {
    while (++curNodeIter != endNodeIter) {
      curNode = *curNodeIter;
      // If we reached a region fragment, we found a place to insert the
      // stub.
      if (curNode->getKind() == Fragment::Region)
        break;
      if (curNode->getKind() == Fragment::Stub)
        continue;
      stubOffset = curNode->getOffset(DiagEngine);
    }
    // Fetch the address of the fragment and see if the reloc is able to reach
    // it.
    fragAddr = outputELFSection->addr() + stubOffset;
  }

  int64_t StubOffset = 0;
  // Lets use the map information to produce more diagnostics, adding
  // information to the data structures will increase memory!
  if (!stub->isRelocInRange(nullptr,
                            fragAddr - outputELFSection->addr() - pOffset,
                            StubOffset, pBuilder.getModule())) {
    pBuilder.getModule().setFailure(true);
    return nullptr;
  }

  // Trampoline fragment list.
  llvm::SmallVector<Fragment *, 0> toBeInsertedFrags;
  if (clone != nullptr)
    toBeInsertedFrags.push_back(clone);

  if (isLastFragInSection) {
    curNode = MatchedSection->getFragmentList().back();
    curNodeIter = MatchedSection->getFragmentList().end();
  }
  MatchedSection->splice(curNodeIter, toBeInsertedFrags);
  symbol->resolveInfo()->setResolvedOrigin(trampolineInput);

  // Set the owning section.
  ELFSection *TrampolineInputSection =
      pBuilder.getModule().getLinkerScript().sectionMap().createELFSection(
          ".text" + name, LDFileFormat::Regular, llvm::ELF::SHT_PROGBITS,
          MatchedSection->getFlags(), /*EntSize=*/0);
  TrampolineInputSection->setMatchedLinkerScriptRule(MatchedRule);
  TrampolineInputSection->setOutputSection(
      outputELFSection->getOutputSection());
  clone->setOwningSection(TrampolineInputSection);

  for (auto &F : toBeInsertedFrags)
    F->getOwningSection()->setMatchedLinkerScriptRule(MatchedRule);

  if (layoutPrinter) {
    layoutPrinter->recordFragment(trampolineInput, clone->getOutputELFSection(),
                                  clone);
    layoutPrinter->recordSymbol(clone, symbol);
    layoutPrinter->recordTrampolines();
  }

  // FIXME : insert all trampolines and do this only once per iteration.
  Fragment *dirtyFrag = clone->getPrevNode();

  if (!dirtyFrag)
    dirtyFrag = clone;

  bool resetSectionSize = true;

  uint64_t newOffset = 0;
  if (auto previousFrag = dirtyFrag->getPrevNode())
    newOffset = previousFrag->getOffset(DiagEngine) + previousFrag->size();
  // Flag thats used to check if the linker already processed the clone.
  bool seenClone = false;
  for (auto i = dirtyFrag->getIterator(),
            n = MatchedSection->getFragmentList().end();
       i != n; ++i) {
    auto dirtyFrag = *i;
    // There is no need to reset the section size, if any of the fragment after
    // the stub has the same offset as previous.
    if (seenClone && (newOffset == dirtyFrag->getOffset(DiagEngine))) {
      resetSectionSize = false;
      break;
    }
    dirtyFrag->setOffset(newOffset);
    // If we found a stub, we start checking for offsets after the stub.
    if (dirtyFrag == clone)
      seenClone = true;
    newOffset = dirtyFrag->getOffset(DiagEngine) + dirtyFrag->size();
  }

  if (resetSectionSize)
    outputELFSection->setSize(newOffset);

  // Add the branch island to the output section.
  outputELFSection->getOutputSection()->addBranchIsland(island);

  return island;
}
