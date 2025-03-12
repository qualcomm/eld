//===- BranchIslandFactory.cpp---------------------------------------------===//
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

#include "eld/BranchIsland/BranchIslandFactory.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Core/Module.h"
#include "eld/Fragment/Fragment.h"
#include "eld/LayoutMap/LayoutPrinter.h"
#include "eld/Object/ObjectLinker.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Support/Memory.h"
#include "eld/Support/MsgHandling.h"
#include "eld/Support/RegisterTimer.h"
#include "eld/Target/Relocator.h"
#include "llvm/Support/Timer.h"
#include <sstream>
#include <string>

using namespace eld;

//===----------------------------------------------------------------------===//
// BranchIslandFactory
//===----------------------------------------------------------------------===//

/// ctor
BranchIslandFactory::BranchIslandFactory(bool useAddends, LinkerConfig &config)
    : numBranchIsland(0), numClone(0), _useAddends(useAddends),
      m_Config(config) {}

BranchIslandFactory::~BranchIslandFactory() {}

/// findBranchIsland - find an already existing island if exists
///  for the relocation, that defines the symbol.
BranchIsland *BranchIslandFactory::findBranchIsland(Module &pModule,
                                                    Relocation &pReloc,
                                                    Stub *stub,
                                                    int64_t addend) {
  OutputSectionEntry *outputSection = pReloc.targetRef()->getOutputSection();

  llvm::StringRef symName = pReloc.symInfo()->name();

  if (!pModule.getConfig().options().noReuseOfTrampolinesFile().empty() &&
      pModule.findCanReuseTrampolinesForSymbol(symName))
    return nullptr;

  std::vector<BranchIsland *> islands =
      outputSection->getBranchIslandsForSymbol(pReloc.symInfo());

  if (!islands.size())
    return nullptr;

  int64_t Offset = 0;
  std::vector<std::pair<BranchIsland *, int64_t>> BranchIslandWithOffsetsVector;

  for (auto &I : islands) {
    if ((stub->isRelocInRange(&pReloc, I->branchIslandAddr(pModule), Offset,
                              pModule)) &&
        (I->canReuseBranchIsland(pReloc.symInfo(), addend, _useAddends,
                                 stub))) {
      BranchIslandWithOffsetsVector.push_back(std::make_pair(I, Offset));
    }
  }
  if (BranchIslandWithOffsetsVector.size()) {
    auto branchIsland = *std::min_element(
        BranchIslandWithOffsetsVector.cbegin(),
        BranchIslandWithOffsetsVector.cend(),
        [](const auto &B1, const auto &B2) { return B1.second < B2.second; });
    return branchIsland.first;
  }
  return nullptr;
}

/// createBranchIsland - create a branch island for the relocation.
std::pair<BranchIsland *, bool>
BranchIslandFactory::createBranchIsland(Relocation &pReloc, Stub *stub,
                                        eld::IRBuilder &pBuilder,
                                        const Relocator *pRelocator) {
  Module &pModule = pBuilder.getModule();
  LayoutPrinter *layoutPrinter = pBuilder.getModule().getLayoutPrinter();

  bool cloneFrag = false;

  Stub *clone = nullptr;

  // May be the old trampoline generated was not reachable.
  // Lets not generate a trampoline for a trampoline :-)
  Fragment *relocFrag = nullptr;

  if (!pReloc.symInfo()->isDyn() && !pReloc.symInfo()->isAbsolute())
    relocFrag = pReloc.symInfo()->outSymbol()->fragRef()->frag();

  Fragment *targetFrag = pReloc.targetRef()->frag();

  if (relocFrag != nullptr && relocFrag->getKind() == Fragment::Stub)
    pReloc.setSymInfo(((Stub *)relocFrag)->savedSymInfo());

  ResolveInfo *rInfo = pReloc.symInfo();

  llvm::StringRef str = rInfo->name();

  if (pBuilder.getModule().findInCopyFarCallSet(str))
    cloneFrag = true;

  bool isSectionRelative = (rInfo->type() == ResolveInfo::Section);

  if (!rInfo->outSymbol()->isDyn() && !pReloc.symInfo()->isAbsolute())
    relocFrag = rInfo->outSymbol()->fragRef()->frag();

  ELFSection *outputELFSection = pReloc.targetRef()->getOutputELFSection();
  ELFSection *TrampolineInputSection = nullptr;

  if (cloneFrag && relocFrag && relocFrag->getOwningSection()->hasRelocData()) {
    std::lock_guard<std::mutex> Guard(Mutex);
    m_Config.raise(diag::unable_to_clone_fragment)
        << str << pReloc.getSourcePath(m_Config.options())
        << outputELFSection->name() << "\n";
    pBuilder.getModule().removeFromCopyFarCallSet(str);
    cloneFrag = false;
  }

  uint32_t realAddend = 0;
  uint32_t relocAddend = 0;
  {
    if (isSectionRelative) {
      // Offset from start of packet
      realAddend = stub->getRealAddend(pReloc, m_Config.getDiagEngine());
      // Offset to the real symbol
      relocAddend = pReloc.addend() - realAddend;
      pReloc.setAddend(realAddend);
    } else {
      assert(pReloc.addend() ==
             stub->getRealAddend(pReloc, m_Config.getDiagEngine()));
    }
  }

  BranchIsland *island = nullptr;
  {
    island = findBranchIsland(pBuilder.getModule(), pReloc, stub, relocAddend);
    if (island) {
      island->addReuse(&pReloc);
      return std::make_pair(island, true);
    }
  }
  bool isLastFragInSection = false;
  Fragment *curNode = nullptr;
  ELFSection *MatchedSection = nullptr;
  RuleContainer *MatchedRule = nullptr;
  typedef llvm::SmallVectorImpl<Fragment *>::iterator Iter;
  Iter curNodeIter;
  LDSymbol *symbol = nullptr;
  InputFile *trampolineInput = nullptr;
  int64_t fragAddr = 0;

  {
    // insert stub fragment
    trampolineInput = pBuilder.getModule().getInternalInput(
        Module::InternalInputType::Trampoline);

    if (cloneFrag)
      clone = stub->clone(trampolineInput, &pReloc, relocFrag, &pBuilder,
                          m_Config.getDiagEngine());
    else
      clone = stub->clone(trampolineInput, &pReloc, &pBuilder,
                          m_Config.getDiagEngine());

    if (!clone) {
      std::lock_guard<std::mutex> Guard(Mutex);
      pBuilder.getModule().setFailure(true);
      return std::make_pair(nullptr, false);
    }

    std::string SymbolName;
    {
      std::lock_guard<std::mutex> Guard(Mutex);
      island = make<BranchIsland>(clone);
      if (!cloneFrag)
        numBranchIsland++;
      else
        numClone++;

      // build a name for stub symbol
      SymbolName = stub->getStubName(pReloc, cloneFrag, isSectionRelative,
                                     numBranchIsland, numClone, relocAddend,
                                     m_Config.useOldStyleTrampolineName());
      if (!cloneFrag) {
        uint64_t trampolineCountToSymbol =
            outputELFSection->getOutputSection()->getTrampolineCount(
                SymbolName);
        if (trampolineCountToSymbol)
          SymbolName += "_" + std::to_string(trampolineCountToSymbol);
      }

      // create LDSymbol for the stub
      symbol = createSymbol(pModule, trampolineInput, SymbolName,
                            stub->size(),         // size
                            stub->initSymValue(), // value
                            make<FragmentRef>(*clone, clone->initSymValue()));

      symbol->setShouldIgnore(false);

      pBuilder.getModule().addSymbol(symbol->resolveInfo());
    }

    clone->setSymInfo(symbol->resolveInfo());

    clone->setSavedSymInfo(pReloc.symInfo());

    // Set the branch target of this cloned stub.
    {
      std::lock_guard<std::mutex> Guard(Mutex);
      for (Stub::fixup_iterator it = clone->fixup_begin(),
                                ie = clone->fixup_end();
           it != ie; ++it) {

        Relocation *reloc = Relocation::Create(
            (*it)->type(), pRelocator->getSize((*it)->type()),
            make<FragmentRef>(*clone, (*it)->offset()),
            (*it)->addend() + relocAddend);
        reloc->setSymInfo(pReloc.symInfo());
        island->addRelocation(*reloc);
      }
    }

    // Current nodes.
    curNode = pReloc.targetRef()->frag();

    MatchedRule = curNode->getOwningSection()->getMatchedLinkerScriptRule();
    MatchedSection = MatchedRule->getSection();

    curNodeIter = curNode->getIterator();
    Iter endNodeIter = MatchedSection->getFragmentList().end();

    // Set the owning section.
    {
      std::lock_guard<std::mutex> Guard(Mutex);
      TrampolineInputSection =
          pModule.getLinkerScript().sectionMap().createELFSection(
              ".text." + SymbolName, LDFileFormat::Internal,
              llvm::ELF::SHT_PROGBITS, MatchedSection->getFlags(),
              /*EntSize=*/0);
      TrampolineInputSection->setMatchedLinkerScriptRule(MatchedRule);
      TrampolineInputSection->setOutputSection(
          outputELFSection->getOutputSection());
      TrampolineInputSection->setSize(clone->size());
      TrampolineInputSection->setInputFile(
          pModule.getInternalInput(Module::Trampoline));
      pModule.getLinker()->getObjectLinker()->addInputSection(
          TrampolineInputSection);
    }

    clone->setOwningSection(TrampolineInputSection);

    // Default stub offset.
    size_t stubOffset =
        curNode->getOffset(m_Config.getDiagEngine()) + curNode->size();

    // If the fragment is the only fragment in the section, lets go to the end
    // of
    // the list and try appending the stub.
    if ((curNodeIter + 1) == endNodeIter) {
      fragAddr = outputELFSection->addr() + stubOffset;
      isLastFragInSection = true;
    }

    // SEARCH FORWARD FOR A PLACE TO HOLD THE STUB, THATS AFTER ALL STUBS.j
    // | curNode | Align | Stub' | Align | Stub" | Align | Region |
    if (!isLastFragInSection) {
      bool foundRegionFragment = false;
      Fragment *prevNode = nullptr;
      while (++curNodeIter != endNodeIter) {
        curNode = *curNodeIter;
        // If we reached a region fragment, we found a place to insert the
        // stub.
        if (curNode->getKind() == Fragment::Region) {
          foundRegionFragment = true;
          if (prevNode)
            stubOffset = prevNode->getOffset(m_Config.getDiagEngine()) +
                         prevNode->size();
          break;
        }
        prevNode = curNode;
      }
      if (!foundRegionFragment)
        stubOffset =
            curNode->getOffset(m_Config.getDiagEngine()) + curNode->size();
      // Fetch the address of the fragment and see if the reloc is able to reach
      // it.
      fragAddr = outputELFSection->addr() + stubOffset;
    }

    fragAddr +=
        llvm::offsetToAlignment(fragAddr, llvm::Align(clone->alignment()));

    int64_t Offset = 0;

    // Lets use the map information to produce more diagnostics, adding
    // information to the data structures will increase memory!
    if (!stub->isRelocInRange(&pReloc, fragAddr, Offset, pModule)) {
      std::lock_guard<std::mutex> Guard(Mutex);
      m_Config.raise(diag::unable_to_insert_trampoline)
          << str
          << Relocation::getFragmentPath(nullptr, targetFrag,
                                         m_Config.options())
          << Relocation::getFragmentPath(nullptr, relocFrag, m_Config.options())
          << std::string(
                 pModule.getBackend()->getRelocator()->getName(pReloc.type()));
      pBuilder.getModule().setFailure(true);
      return std::make_pair(nullptr, false);
    }
    clone->setOffset(fragAddr - outputELFSection->addr());
  }
  // Trampoline fragment list.
  {
    llvm::SmallVector<Fragment *, 0> toBeInsertedFrags;
    if (clone != nullptr)
      toBeInsertedFrags.push_back(clone);

    if (isLastFragInSection) {
      curNode = MatchedSection->getFragmentList().back();
      curNodeIter = MatchedSection->getFragmentList().end();
    }

    for (auto &F : toBeInsertedFrags)
      F->getOwningSection()->setMatchedLinkerScriptRule(MatchedRule);

    MatchedSection->splice(curNodeIter, toBeInsertedFrags);
    symbol->resolveInfo()->setResolvedOrigin(trampolineInput);

    if (layoutPrinter && !layoutPrinter->showOnlyLayout()) {
      std::lock_guard<std::mutex> Guard(Mutex);
      layoutPrinter->recordFragment(trampolineInput, TrampolineInputSection,
                                    clone);
      layoutPrinter->recordSymbol(clone, symbol);
      layoutPrinter->recordTrampolines();
    }

    outputELFSection->getOutputSection()->addBranchIsland(pReloc.symInfo(),
                                                          island);
    island->saveTrampolineInfo(pReloc, relocAddend);

    // Add the branch island to the output section.
    ASSERT((island->branchIslandAddr(pModule) & ~0x1) == fragAddr,
           "Stub address not the same as island!");
  }

  return std::make_pair(island, false);
}

LDSymbol *BranchIslandFactory::createSymbol(Module &pModule, InputFile *pInput,
                                            const std::string &pName,
                                            ResolveInfo::SizeType pSize,
                                            LDSymbol::ValueType pValue,
                                            FragmentRef *pFragmentRef) {
  ResolveInfo *input_sym_info = nullptr;
  input_sym_info = pModule.getNamePool().createSymbol(
      pInput, pName, false, ResolveInfo::Function, ResolveInfo::Define,
      ResolveInfo::Local, pSize, ResolveInfo::Default, false);
  LDSymbol *output_sym = make<LDSymbol>(input_sym_info, false);
  input_sym_info->setOutSymbol(output_sym);
  output_sym->setFragmentRef(pFragmentRef);
  output_sym->setValue(pValue, false);
  return output_sym;
}
