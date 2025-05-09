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
#include "eld/LayoutMap/LayoutInfo.h"
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
BranchIslandFactory::BranchIslandFactory(bool UseAddends, LinkerConfig &Config)
    : NumBranchIsland(0), NumClone(0), UseAddends(UseAddends), Config(Config) {}

BranchIslandFactory::~BranchIslandFactory() {}

/// findBranchIsland - find an already existing island if exists
///  for the relocation, that defines the symbol.
BranchIsland *BranchIslandFactory::findBranchIsland(Module &PModule,
                                                    Relocation &PReloc, Stub *S,
                                                    int64_t Addend) {
  OutputSectionEntry *OutputSection = PReloc.targetRef()->getOutputSection();

  llvm::StringRef SymName = PReloc.symInfo()->name();

  if (!PModule.getConfig().options().noReuseOfTrampolinesFile().empty() &&
      PModule.findCanReuseTrampolinesForSymbol(SymName))
    return nullptr;

  std::vector<BranchIsland *> Islands =
      OutputSection->getBranchIslandsForSymbol(PReloc.symInfo());

  if (!Islands.size())
    return nullptr;

  int64_t Offset = 0;
  std::vector<std::pair<BranchIsland *, int64_t>> BranchIslandWithOffsetsVector;

  for (auto &I : Islands) {
    if ((S->isRelocInRange(&PReloc, I->branchIslandAddr(PModule), Offset,
                           PModule)) &&
        (I->canReuseBranchIsland(PReloc.symInfo(), Addend, UseAddends, S))) {
      BranchIslandWithOffsetsVector.push_back(std::make_pair(I, Offset));
    }
  }
  if (BranchIslandWithOffsetsVector.size()) {
    auto BranchIsland = *std::min_element(
        BranchIslandWithOffsetsVector.cbegin(),
        BranchIslandWithOffsetsVector.cend(),
        [](const auto &B1, const auto &B2) { return B1.second < B2.second; });
    return BranchIsland.first;
  }
  return nullptr;
}

/// createBranchIsland - create a branch island for the relocation.
std::pair<BranchIsland *, bool>
BranchIslandFactory::createBranchIsland(Relocation &PReloc, Stub *S,
                                        eld::IRBuilder &PBuilder,
                                        const Relocator *PRelocator) {
  Module &PModule = PBuilder.getModule();
  LayoutInfo *LayoutInfo = PBuilder.getModule().getLayoutInfo();

  bool CloneFrag = false;

  Stub *Clone = nullptr;

  // May be the old trampoline generated was not reachable.
  // Lets not generate a trampoline for a trampoline :-)
  Fragment *RelocFrag = nullptr;

  if (!PReloc.symInfo()->isDyn() && !PReloc.symInfo()->isAbsolute())
    RelocFrag = PReloc.symInfo()->outSymbol()->fragRef()->frag();

  Fragment *TargetFrag = PReloc.targetRef()->frag();

  if (RelocFrag != nullptr && RelocFrag->getKind() == Fragment::Stub)
    PReloc.setSymInfo(((Stub *)RelocFrag)->savedSymInfo());

  ResolveInfo *RInfo = PReloc.symInfo();

  llvm::StringRef Str = RInfo->name();

  if (PBuilder.getModule().findInCopyFarCallSet(Str))
    CloneFrag = true;

  bool IsSectionRelative = (RInfo->type() == ResolveInfo::Section);

  if (!RInfo->outSymbol()->isDyn() && !PReloc.symInfo()->isAbsolute())
    RelocFrag = RInfo->outSymbol()->fragRef()->frag();

  ELFSection *OutputElfSection = PReloc.targetRef()->getOutputELFSection();
  ELFSection *TrampolineInputSection = nullptr;

  if (CloneFrag && RelocFrag && RelocFrag->getOwningSection()->hasRelocData()) {
    std::lock_guard<std::mutex> Guard(Mutex);
    Config.raise(Diag::unable_to_clone_fragment)
        << Str << PReloc.getSourcePath(Config.options())
        << OutputElfSection->name() << "\n";
    PBuilder.getModule().removeFromCopyFarCallSet(Str);
    CloneFrag = false;
  }

  uint32_t RealAddend = 0;
  uint32_t RelocAddend = 0;
  {
    if (IsSectionRelative) {
      // Offset from start of packet
      RealAddend = S->getRealAddend(PReloc, Config.getDiagEngine());
      // Offset to the real symbol
      RelocAddend = PReloc.addend() - RealAddend;
      PReloc.setAddend(RealAddend);
    } else {
      assert(PReloc.addend() ==
             S->getRealAddend(PReloc, Config.getDiagEngine()));
    }
  }

  BranchIsland *Island = nullptr;
  {
    Island = findBranchIsland(PBuilder.getModule(), PReloc, S, RelocAddend);
    if (Island) {
      Island->addReuse(&PReloc);
      return std::make_pair(Island, true);
    }
  }
  bool IsLastFragInSection = false;
  Fragment *CurNode = nullptr;
  ELFSection *MatchedSection = nullptr;
  RuleContainer *MatchedRule = nullptr;
  typedef llvm::SmallVectorImpl<Fragment *>::iterator Iter;
  Iter CurNodeIter;
  LDSymbol *Symbol = nullptr;
  InputFile *TrampolineInput = nullptr;
  int64_t FragAddr = 0;

  {
    // insert stub fragment
    TrampolineInput = PBuilder.getModule().getInternalInput(
        Module::InternalInputType::Trampoline);

    if (CloneFrag)
      Clone = S->clone(TrampolineInput, &PReloc, RelocFrag, &PBuilder,
                       Config.getDiagEngine());
    else
      Clone =
          S->clone(TrampolineInput, &PReloc, &PBuilder, Config.getDiagEngine());

    if (!Clone) {
      std::lock_guard<std::mutex> Guard(Mutex);
      PBuilder.getModule().setFailure(true);
      return std::make_pair(nullptr, false);
    }

    std::string SymbolName;
    {
      std::lock_guard<std::mutex> Guard(Mutex);
      Island = make<BranchIsland>(Clone);
      if (!CloneFrag)
        NumBranchIsland++;
      else
        NumClone++;

      // build a name for stub symbol
      SymbolName = S->getStubName(PReloc, CloneFrag, IsSectionRelative,
                                  NumBranchIsland, NumClone, RelocAddend,
                                  Config.useOldStyleTrampolineName());
      if (!CloneFrag) {
        uint64_t TrampolineCountToSymbol =
            OutputElfSection->getOutputSection()->getTrampolineCount(
                SymbolName);
        if (TrampolineCountToSymbol)
          SymbolName += "_" + std::to_string(TrampolineCountToSymbol);
      }

      // create LDSymbol for the stub
      Symbol = createSymbol(PModule, TrampolineInput, SymbolName,
                            S->size(),         // size
                            S->initSymValue(), // value
                            make<FragmentRef>(*Clone, Clone->initSymValue()));

      Symbol->setShouldIgnore(false);

      PBuilder.getModule().addSymbol(Symbol->resolveInfo());
    }

    Clone->setSymInfo(Symbol->resolveInfo());

    Clone->setSavedSymInfo(PReloc.symInfo());

    // Set the branch target of this cloned stub.
    {
      std::lock_guard<std::mutex> Guard(Mutex);
      for (Stub::fixup_iterator It = Clone->fixupBegin(),
                                Ie = Clone->fixupEnd();
           It != Ie; ++It) {
        Relocation *Reloc = Relocation::Create(
            (*It)->type(), PRelocator->getSize((*It)->type()),
            make<FragmentRef>(*Clone, (*It)->offset()),
            (*It)->addend() + RelocAddend);
        Reloc->setSymInfo(PReloc.symInfo());
        Island->addRelocation(*Reloc);
      }
    }

    // Current nodes.
    CurNode = PReloc.targetRef()->frag();

    MatchedRule = CurNode->getOwningSection()->getMatchedLinkerScriptRule();
    MatchedSection = MatchedRule->getSection();

    CurNodeIter = CurNode->getIterator();
    Iter EndNodeIter = MatchedSection->getFragmentList().end();

    // Set the owning section.
    {
      std::lock_guard<std::mutex> Guard(Mutex);
      TrampolineInputSection =
          PModule.getLinkerScript().sectionMap().createELFSection(
              ".text." + SymbolName, LDFileFormat::Internal,
              llvm::ELF::SHT_PROGBITS, MatchedSection->getFlags(),
              /*EntSize=*/0);
      TrampolineInputSection->setMatchedLinkerScriptRule(MatchedRule);
      TrampolineInputSection->setOutputSection(
          OutputElfSection->getOutputSection());
      TrampolineInputSection->setSize(Clone->size());
      TrampolineInputSection->setInputFile(
          PModule.getInternalInput(Module::Trampoline));
      PModule.getLinker()->getObjectLinker()->addInputSection(
          TrampolineInputSection);
    }

    Clone->setOwningSection(TrampolineInputSection);

    // Default stub offset.
    size_t StubOffset =
        CurNode->getOffset(Config.getDiagEngine()) + CurNode->size();

    // If the fragment is the only fragment in the section, lets go to the end
    // of
    // the list and try appending the stub.
    if ((CurNodeIter + 1) == EndNodeIter) {
      FragAddr = OutputElfSection->addr() + StubOffset;
      IsLastFragInSection = true;
    }

    // SEARCH FORWARD FOR A PLACE TO HOLD THE STUB, THATS AFTER ALL STUBS.j
    // | curNode | Align | Stub' | Align | Stub" | Align | Region |
    if (!IsLastFragInSection) {
      bool FoundRegionFragment = false;
      Fragment *PrevNode = nullptr;
      while (++CurNodeIter != EndNodeIter) {
        CurNode = *CurNodeIter;
        // If we reached a region fragment, we found a place to insert the
        // stub.
        if (CurNode->getKind() == Fragment::Region) {
          FoundRegionFragment = true;
          if (PrevNode)
            StubOffset =
                PrevNode->getOffset(Config.getDiagEngine()) + PrevNode->size();
          break;
        }
        PrevNode = CurNode;
      }
      if (!FoundRegionFragment)
        StubOffset =
            CurNode->getOffset(Config.getDiagEngine()) + CurNode->size();
      // Fetch the address of the fragment and see if the reloc is able to reach
      // it.
      FragAddr = OutputElfSection->addr() + StubOffset;
    }

    FragAddr +=
        llvm::offsetToAlignment(FragAddr, llvm::Align(Clone->alignment()));

    int64_t Offset = 0;

    // Lets use the map information to produce more diagnostics, adding
    // information to the data structures will increase memory!
    if (!S->isRelocInRange(&PReloc, FragAddr, Offset, PModule)) {
      std::lock_guard<std::mutex> Guard(Mutex);
      Config.raise(Diag::unable_to_insert_trampoline)
          << Str
          << Relocation::getFragmentPath(nullptr, TargetFrag, Config.options())
          << Relocation::getFragmentPath(nullptr, RelocFrag, Config.options())
          << std::string(
                 PModule.getBackend()->getRelocator()->getName(PReloc.type()));
      PBuilder.getModule().setFailure(true);
      return std::make_pair(nullptr, false);
    }
    Clone->setOffset(FragAddr - OutputElfSection->addr());
  }
  // Trampoline fragment list.
  {
    llvm::SmallVector<Fragment *, 0> ToBeInsertedFrags;
    if (Clone != nullptr)
      ToBeInsertedFrags.push_back(Clone);

    if (IsLastFragInSection) {
      CurNode = MatchedSection->getFragmentList().back();
      CurNodeIter = MatchedSection->getFragmentList().end();
    }

    for (auto &F : ToBeInsertedFrags)
      F->getOwningSection()->setMatchedLinkerScriptRule(MatchedRule);

    MatchedSection->splice(CurNodeIter, ToBeInsertedFrags);
    Symbol->resolveInfo()->setResolvedOrigin(TrampolineInput);

    if (LayoutInfo && !LayoutInfo->showOnlyLayout()) {
      std::lock_guard<std::mutex> Guard(Mutex);
      LayoutInfo->recordFragment(TrampolineInput, TrampolineInputSection,
                                    Clone);
      LayoutInfo->recordSymbol(Clone, Symbol);
      LayoutInfo->recordTrampolines();
    }

    OutputElfSection->getOutputSection()->addBranchIsland(PReloc.symInfo(),
                                                          Island);
    Island->saveTrampolineInfo(PReloc, RelocAddend);

    // Add the branch island to the output section.
    ASSERT((Island->branchIslandAddr(PModule) & ~0x1) == FragAddr,
           "Stub address not the same as island!");
  }

  return std::make_pair(Island, false);
}

LDSymbol *BranchIslandFactory::createSymbol(Module &PModule, InputFile *PInput,
                                            const std::string &PName,
                                            ResolveInfo::SizeType PSize,
                                            LDSymbol::ValueType PValue,
                                            FragmentRef *PFragmentRef) {
  ResolveInfo *InputSymInfo = nullptr;
  InputSymInfo = PModule.getNamePool().createSymbol(
      PInput, PName, false, ResolveInfo::Function, ResolveInfo::Define,
      ResolveInfo::Local, PSize, ResolveInfo::Default, false);
  LDSymbol *OutputSym = make<LDSymbol>(InputSymInfo, false);
  InputSymInfo->setOutSymbol(OutputSym);
  OutputSym->setFragmentRef(PFragmentRef);
  OutputSym->setValue(PValue, false);
  return OutputSym;
}
