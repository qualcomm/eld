//===- ARMLDBackend.cpp----------------------------------------------------===//
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
#include "ARMLDBackend.h"
#include "ARM.h"
#include "ARMAttributeFragment.h"
#include "ARMEXIDXFragment.h"
#include "ARMInfo.h"
#include "ARMRelocator.h"
#include "ARMToARMStub.h"
#include "ARMToTHMStub.h"
#include "THMToARMStub.h"
#include "THMToTHMStub.h"
#include "eld/BranchIsland/BranchIslandFactory.h"
#include "eld/BranchIsland/StubFactory.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Fragment/FillFragment.h"
#include "eld/Fragment/RegionFragment.h"
#include "eld/Fragment/Stub.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/Object/ObjectBuilder.h"
#include "eld/Support/Memory.h"
#include "eld/Support/MemoryArea.h"
#include "eld/Support/MemoryRegion.h"
#include "eld/Support/MsgHandling.h"
#include "eld/Support/RegisterTimer.h"
#include "eld/Support/TargetRegistry.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "eld/Target/ELFDynamic.h"
#include "eld/Target/ELFFileFormat.h"
#include "eld/Target/ELFSegment.h"
#include "eld/Target/ELFSegmentFactory.h"
#include "eld/Target/GNULDBackend.h"
#include "eld/Target/TargetInfo.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/Twine.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Object/ELFTypes.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Program.h"
#include <algorithm>
#include <cstring>
#include <limits>

using namespace eld;
using namespace llvm;

//===----------------------------------------------------------------------===//
// ARMGNULDBackend
//===----------------------------------------------------------------------===//
ARMGNULDBackend::ARMGNULDBackend(eld::Module &pModule, TargetInfo *pInfo)
    : GNULDBackend(pModule, pInfo), m_pRelocator(nullptr),
      m_pEXIDXStart(nullptr), m_pEXIDXEnd(nullptr), m_pEXIDX(nullptr),
      m_pRWPIBase(nullptr), m_pSBRELSegment(nullptr),
      m_pARMAttributeSection(nullptr), AttributeFragment(nullptr) {}

ARMGNULDBackend::~ARMGNULDBackend() {}

bool ARMGNULDBackend::initBRIslandFactory() {
  if (nullptr == m_pBRIslandFactory) {
    m_pBRIslandFactory = make<BranchIslandFactory>(false, config());
  }
  return true;
}

bool ARMGNULDBackend::initStubFactory() {
  if (nullptr == m_pStubFactory)
    m_pStubFactory = make<StubFactory>();
  return true;
}

void ARMGNULDBackend::createAttributeSection(uint32_t Flag, uint32_t Align) {
  if (m_pARMAttributeSection)
    return;
  m_pARMAttributeSection = m_Module.createInternalSection(
      Module::InternalInputType::Attributes, LDFileFormat::Internal,
      ".ARM.attributes", llvm::ELF::SHT_ARM_ATTRIBUTES, Flag, Align);
}

void ARMGNULDBackend::initDynamicSections(ELFObjectFile &InputFile) {
  InputFile.setDynamicSections(
      *m_Module.createInternalSection(
          InputFile, LDFileFormat::Internal, ".got", llvm::ELF::SHT_PROGBITS,
          llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE, 4),
      *m_Module.createInternalSection(
          InputFile, LDFileFormat::Internal, ".got.plt",
          llvm::ELF::SHT_PROGBITS, llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE,
          4),
      *m_Module.createInternalSection(
          InputFile, LDFileFormat::Internal, ".plt", llvm::ELF::SHT_PROGBITS,
          llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR, 4),
      *m_Module.createInternalSection(
          InputFile, LDFileFormat::DynamicRelocation, ".rel.dyn",
          llvm::ELF::SHT_REL, llvm::ELF::SHF_ALLOC, 4),
      *m_Module.createInternalSection(
          InputFile, LDFileFormat::DynamicRelocation, ".rel.plt",
          llvm::ELF::SHT_REL, llvm::ELF::SHF_ALLOC, 4));
}

void ARMGNULDBackend::initTargetSections(ObjectBuilder &pBuilder) {
  // Create an .ARM.attribute section, if not already created
  createAttributeSection(0, 1);

  // FIXME: Currently we set exidx and extab to "Exception" and directly emit
  // them from input
  m_pEXIDX = m_Module.createInternalSection(
      Module::InternalInputType::Exception, LDFileFormat::Internal,
      ".ARM.exidx", llvm::ELF::SHT_ARM_EXIDX,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_LINK_ORDER, 4);

  // For final links (executables and shared libraries), create a separate
  // internal sentinel section that holds the 8-byte CANTUNWIND terminator.
  // Its section type and name match *(.ARM.exidx*) linker script rules so it
  // is always placed last.  Partial links (-r) do not get a sentinel.
  if (LinkerConfig::Object != config().codeGenType()) {
    m_pEXIDXSentinel = m_Module.createInternalSection(
        Module::InternalInputType::Exception, LDFileFormat::Internal,
        ".ARM.exidx", llvm::ELF::SHT_ARM_EXIDX,
        llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_LINK_ORDER, 4);
    m_pSentinelFrag = make<EXIDXSentinelFragment>(m_pEXIDXSentinel);
    m_pEXIDXSentinel->addFragment(m_pSentinelFrag);
    LayoutInfo *LI = getModule().getLayoutInfo();
    if (LI)
      LI->recordFragment(m_pEXIDXSentinel->getInputFile(), m_pEXIDXSentinel,
                         m_pSentinelFrag);
  }
}

void ARMGNULDBackend::initTargetSymbols() {
  // Define the symbol _GLOBAL_OFFSET_TABLE_ if there is a symbol with the
  // same name in input
  auto SymbolName = "_GLOBAL_OFFSET_TABLE_";
  if (LinkerConfig::Object != config().codeGenType()) {
    m_pGOTSymbol =
        m_Module.getIRBuilder()
            ->addSymbol<IRBuilder::AsReferred, IRBuilder::Resolve>(
                m_Module.getInternalInput(Module::Script), SymbolName,
                ResolveInfo::Object, ResolveInfo::Define, ResolveInfo::Local,
                0x0, // size
                0x0, // value
                FragmentRef::null(), ResolveInfo::Hidden);
    if (m_Module.getConfig().options().isSymbolTracingRequested() &&
        m_Module.getConfig().options().traceSymbol(SymbolName))
      config().raise(Diag::target_specific_symbol) << SymbolName;
    if (m_pGOTSymbol)
      m_pGOTSymbol->setShouldIgnore(false);
  }

  // If linker script, lets not add this symbol.
  if (m_Module.getScript().linkerScriptHasSectionsCommand())
    return;

  const NamePool& NP = m_Module.getNamePool();
  SymbolName = "__exidx_start";
  const ResolveInfo *EXIDXStartInfo = NP.findInfo(SymbolName);
  if (EXIDXStartInfo && EXIDXStartInfo->isUndef()) {
    m_pEXIDXStart =
        m_Module.getIRBuilder()
            ->addSymbol<IRBuilder::Force, IRBuilder::Unresolve>(
                m_Module.getInternalInput(Module::Script), SymbolName,
                ResolveInfo::Object, ResolveInfo::Define, ResolveInfo::Global,
                0x0, // size
                0x0, // value
                FragmentRef::null(), ResolveInfo::Visibility::Hidden);
    if (m_pEXIDXStart) {
      m_pEXIDXStart->setShouldIgnore(false);
      if (m_Module.getConfig().options().isSymbolTracingRequested() &&
          m_Module.getConfig().options().traceSymbol(SymbolName))
        config().raise(Diag::target_specific_symbol) << SymbolName;
    }
  }
  SymbolName = "__exidx_end";
  const ResolveInfo *EXIDXEndInfo = NP.findInfo(SymbolName);
  if (EXIDXEndInfo && EXIDXEndInfo->isUndef()) {
    m_pEXIDXEnd =
        m_Module.getIRBuilder()
            ->addSymbol<IRBuilder::Force, IRBuilder::Unresolve>(
                m_Module.getInternalInput(Module::Script), SymbolName,
                ResolveInfo::Object, ResolveInfo::Define, ResolveInfo::Global,
                0x0, // size
                0x0, // value
                FragmentRef::null(), ResolveInfo::Visibility::Hidden);

    if (m_pEXIDXEnd) {
      m_pEXIDXEnd->setShouldIgnore(false);
      if (m_Module.getConfig().options().isSymbolTracingRequested() &&
          m_Module.getConfig().options().traceSymbol(SymbolName))
        config().raise(Diag::target_specific_symbol) << SymbolName;
    }
  }
  SymbolName = "__RWPI_BASE__";
  m_pRWPIBase =
      m_Module.getIRBuilder()->addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
          m_Module.getInternalInput(Module::Script), SymbolName,
          ResolveInfo::NoType, ResolveInfo::Define, ResolveInfo::Absolute,
          0x0, // size
          0x0, // value
          FragmentRef::null());
  if (m_Module.getConfig().options().isSymbolTracingRequested() &&
      m_Module.getConfig().options().traceSymbol(SymbolName))
    config().raise(Diag::target_specific_symbol) << SymbolName;
  if (m_pRWPIBase)
    m_pRWPIBase->setShouldIgnore(false);
}

bool ARMGNULDBackend::initRelocator() {
  if (nullptr == m_pRelocator) {
    m_pRelocator = make<ARMRelocator>(*this, config(), m_Module);
  }
  return true;
}

Relocator *ARMGNULDBackend::getRelocator() const {
  assert(nullptr != m_pRelocator);
  return m_pRelocator;
}

void ARMGNULDBackend::doPreLayout() {
  if (isMicroController() &&
      ((config().codeGenType() == LinkerConfig::DynObj) ||
       (config().options().isPIE()))) {
    config().raise(Diag::not_supported) << "SharedLibrary/PIE"
                                        << "Cortex-M";
    m_Module.setFailure(true);
    return;
  }

  // set .got size
  // when building shared object, the .got section is must
  if (LinkerConfig::Object != config().codeGenType()) {
    getRelaPLT()->setSize(getRelaPLT()->getRelocationCount() *
                          getRelEntrySize());
    getRelaDyn()->setSize(getRelaDyn()->getRelocationCount() *
                          getRelEntrySize());
    m_Module.addOutputSection(getRelaPLT());
    m_Module.addOutputSection(getRelaDyn());
  }

  // For each EXIDX input section, ensure its output section's sh_link points
  // to the output .text section (not the raw input section).  This is needed
  // because ObjectBuilder stores the input-section link on the output section;
  // getSectLink must return an output section index.
  for (const auto &KV : m_EXIDXFragments) {
    EXIDXFragment *Frag = KV.second;
    ELFSection *InputExidx = Frag->getOwningSection();
    if (!InputExidx)
      continue;
    ELFSection *OutExidx = InputExidx->getOutputELFSection();
    if (!OutExidx || OutExidx->isIgnore() || OutExidx->isDiscard())
      continue;
    ELFSection *InputLink = InputExidx->getLink();
    if (!InputLink)
      continue;
    ELFSection *OutLink = InputLink->getOutputELFSection();
    if (OutLink)
      OutExidx->setLink(OutLink);
  }

  // Activate the sentinel fragment early so its 8-byte contribution to the
  // .ARM.exidx output section size is known during the address-assignment
  // layout pass.  Without this, the sentinel size would be 0 at layout time
  // and .dynamic (or whatever follows) would be placed at an address that
  // overlaps with the sentinel once it is activated in doPostLayout.
  //
  // Match GNU ld: only emit the sentinel when at least one live EXIDX entry
  // carries real unwind data (i.e. not a bare CANTUNWIND word 0x1).
  if (m_pSentinelFrag) {
    for (const auto &KV : m_EXIDXFragments) {
      EXIDXFragment *Frag = KV.second;
      ELFSection *OutSec = Frag->getOutputELFSection();
      if (OutSec && !OutSec->isIgnore() && !OutSec->isDiscard() &&
          Frag->hasRealUnwindData()) {
        m_pSentinelFrag->activate();
        break;
      }
    }
  }
}

void ARMGNULDBackend::sortEXIDX() {
  // ARM EHABI requires .ARM.exidx entries to be sorted by the address of
  // the function each entry describes.
  ELFSection *E =
      m_Module.getScript().sectionMap().find(llvm::ELF::SHT_ARM_EXIDX);

  if (!E)
    return;

  OutputSectionEntry *O = E->getOutputSection();
  if (!O)
    return;

  const uint64_t MaxSortKey = std::numeric_limits<uint64_t>::max();

  // Fast membership test: set of all EXIDXFragment pointers produced by
  // readSection(). Used to distinguish EXIDX fragments from other fragments
  // (e.g. non-EXIDX fragments that may share the same output section rule).
  llvm::DenseSet<Fragment *> EXIDXFragSet;
  EXIDXFragSet.reserve(m_EXIDXFragments.size());
  for (const auto &KV : m_EXIDXFragments)
    EXIDXFragSet.insert(KV.second);

  DiagnosticEngine *Diag = config().getDiagEngine();

  // Setup RuleKeys for linker script rule container sorting
  llvm::DenseMap<RuleContainer *, uint64_t> RuleKey;
  for (auto &In : *O)
    RuleKey[In] = MaxSortKey;

  for (auto &In : *O) {
    ELFSection *S = In->getSection();
    if (!S)
      continue;

    // Sort key for each fragment in this rule's section: the lowest function
    // address covered by that fragment. Non-EXIDX fragments are not inserted
    // here and retain their relative order via OriginalFragOffsets.
    llvm::DenseMap<Fragment *, uint64_t> FragSortKeys;

    // Pre-sort layout offsets of every fragment in this rule's section.
    // Used as a stable tiebreaker so fragments with equal keys (or non-EXIDX
    // fragments) keep their original relative order after stable_sort.
    llvm::DenseMap<Fragment *, uint32_t> OriginalFragOffsets;
    bool SawEXIDXFrag = false;
    for (Fragment *F : S->getFragmentList())
      OriginalFragOffsets[F] = F->getOffset(Diag);

    for (Fragment *F : S->getFragmentList()) {
      if (!EXIDXFragSet.contains(F))
        continue;
      SawEXIDXFrag = true;
      FragSortKeys[F] = MaxSortKey;

      EXIDXFragment *EXIDX = dyn_cast<EXIDXFragment>(F);
      if (!EXIDX)
        continue;
      ELFSection *Owning = EXIDX->getOwningSection();
      if (!Owning)
        continue;
      auto &Pieces = EXIDX->getPieces();
      if (Pieces.empty())
        continue;

      // Sort key for each 8-byte EXIDX piece (entry) within this fragment,
      // keyed by the piece's input section offset. Initialised to MaxSortKey
      // and then narrowed to the lowest function address found via relocations.
      llvm::DenseMap<uint32_t, uint64_t> SortKeys;

      // Original index of each piece (by input offset) before sorting.
      // Used as a tiebreaker so pieces with identical function addresses
      // retain their original order, preserving stable sort semantics.
      llvm::DenseMap<uint32_t, uint32_t> OriginalOrder;
      for (uint32_t I = 0, N = Pieces.size(); I != N; ++I) {
        const uint32_t InputOffset = Pieces[I].InputOffset;
        SortKeys[InputOffset] = MaxSortKey;
        OriginalOrder[InputOffset] = I;
      }

      // The first word of an EXIDX entry anchors the described function.
      for (Relocation *R : Owning->getRelocations()) {
        if (!R || !R->targetRef() || R->targetRef()->isNull())
          continue;
        if (R->targetRef()->frag() != EXIDX)
          continue;
        // R_ARM_NONE (type 0) is a personality-function hint with no address
        // contribution; skip it to avoid using the personality routine address
        // as a sort key.
        if (R->type() == llvm::ELF::R_ARM_NONE)
          continue;

        const uint32_t RelocOffset = R->targetRef()->offset();
        EXIDXPiece Piece = EXIDX->getPiece(RelocOffset);
        if (RelocOffset != Piece.InputOffset)
          continue;

        int64_t Candidate = static_cast<int64_t>(R->symValue(m_Module)) +
                            static_cast<int64_t>(R->addend());
        uint64_t &Key = SortKeys[Piece.InputOffset];
        Key = std::min(Key, static_cast<uint64_t>(Candidate));
      }

      const ELFSection *Linked = Owning->getLink();
      const uint64_t FallbackKey = Linked ? Linked->addr() : MaxSortKey;
      std::stable_sort(Pieces.begin(), Pieces.end(),
                       [&](const EXIDXPiece &A, const EXIDXPiece &B) {
                         uint64_t KeyA = SortKeys.lookup(A.InputOffset);
                         uint64_t KeyB = SortKeys.lookup(B.InputOffset);
                         if (KeyA == MaxSortKey)
                           KeyA = FallbackKey;
                         if (KeyB == MaxSortKey)
                           KeyB = FallbackKey;
                         if (KeyA != KeyB)
                           return KeyA < KeyB;
                         return OriginalOrder.lookup(A.InputOffset) <
                                OriginalOrder.lookup(B.InputOffset);
                       });

      // Relocations record their target position using input-section offsets.
      // After sorting, pieces are reordered in the output, so input offset 0
      // may no longer be output offset 0.
      for (Relocation *R : Owning->getRelocations()) {
        if (!R || !R->targetRef() || R->targetRef()->isNull())
          continue;
        if (R->targetRef()->frag() != EXIDX)
          continue;
        uint32_t NewOffset =
            EXIDX->translateInputOffset(R->targetRef()->offset());
        R->targetRef()->setOffset(NewOffset);
      }

      uint64_t FragKey = MaxSortKey;
      for (const EXIDXPiece &P : Pieces)
        FragKey = std::min(FragKey, SortKeys.lookup(P.InputOffset));
      if (FragKey == MaxSortKey) {
        const ELFSection *Linked = Owning->getLink();
        FragKey = Linked ? Linked->addr() : MaxSortKey;
      }
      FragSortKeys[F] = FragKey;
    }

    if (!SawEXIDXFrag)
      continue;

    std::stable_sort(S->getFragmentList().begin(), S->getFragmentList().end(),
                     [&](Fragment *A, Fragment *B) {
                       const bool AIsEXIDX = EXIDXFragSet.contains(A);
                       const bool BIsEXIDX = EXIDXFragSet.contains(B);
                       if (AIsEXIDX && BIsEXIDX) {
                         const uint64_t KeyA = FragSortKeys.lookup(A);
                         const uint64_t KeyB = FragSortKeys.lookup(B);
                         if (KeyA != KeyB)
                           return KeyA < KeyB;
                       }
                       return OriginalFragOffsets.lookup(A) <
                              OriginalFragOffsets.lookup(B);
                     });

    // Record the minimum key for this rule so we can sort rules below.
    uint64_t RuleMinKey = MaxSortKey;
    for (const auto &KV : FragSortKeys)
      RuleMinKey = std::min(RuleMinKey, KV.second);
    RuleKey[In] = RuleMinKey;
  }

  std::stable_sort(O->begin(), O->end(),
                   [&](RuleContainer *A, RuleContainer *B) {
                     const uint64_t KeyA = RuleKey.lookup(A);
                     const uint64_t KeyB = RuleKey.lookup(B);
                     if (KeyA != KeyB)
                       return KeyA < KeyB;
                     return false;
                   });
  evaluateAssignments(O);

  Fragment *FirstEXIDXFrag = nullptr;
  Fragment *LastEXIDXFrag = nullptr;
  for (auto &In : *O) {
    ELFSection *S = In->getSection();
    if (!S)
      continue;
    for (Fragment *F : S->getFragmentList()) {
      if (F->isNull())
        continue;
      if (!EXIDXFragSet.contains(F))
        continue;
      if (!F->size())
        continue;
      if (!FirstEXIDXFrag)
        FirstEXIDXFrag = F;
      LastEXIDXFrag = F;
    }
  }

  // The sentinel was activated in doPreLayout to ensure its 8-byte size was
  // included in the layout pass.  Here we set the PREL31 target address once
  // output addresses are available.
  if (m_pSentinelFrag && m_pSentinelFrag->size() && LastEXIDXFrag) {
    // The rule's MPSection has no sh_link; use the fragment's owning input
    // section (the actual .ARM.exidx.* section) to resolve the linked .text
    // output section whose addr() is set by layout.
    auto *LastEXIDX = dyn_cast<EXIDXFragment>(LastEXIDXFrag);
    ELFSection *OwningSection =
        LastEXIDX ? LastEXIDX->getOwningSection() : nullptr;
    ELFSection *InputLink = OwningSection ? OwningSection->getLink() : nullptr;
    ELFSection *LastLinkedSection =
        InputLink ? InputLink->getOutputELFSection() : nullptr;
    if (LastLinkedSection)
      m_pSentinelFrag->setTargetAddr(LastLinkedSection->addr() +
                                     LastLinkedSection->size());
  }

  if (m_pEXIDXStart && FirstEXIDXFrag)
    m_pEXIDXStart->setFragmentRef(make<FragmentRef>(*FirstEXIDXFrag, 0));
  // __exidx_end points past the sentinel (the true end of the EXIDX table).
  if (m_pEXIDXEnd && m_pSentinelFrag && m_pSentinelFrag->size()) {
    m_pEXIDXEnd->setFragmentRef(
        make<FragmentRef>(*m_pSentinelFrag, m_pSentinelFrag->size()));
  } else if (m_pEXIDXEnd && LastEXIDXFrag) {
    m_pEXIDXEnd->setFragmentRef(
        make<FragmentRef>(*LastEXIDXFrag, LastEXIDXFrag->size()));
  }
}

bool ARMGNULDBackend::readSection(InputFile &pInput, ELFSection *S) {
  // Keep one EXIDX fragment per section and track entry offsets as pieces.
  if (S->isEXIDX()) {
    llvm::StringRef Region = pInput.getSlice(S->offset(), S->size());
    EXIDXFragment *EXIDX = make<EXIDXFragment>(Region, S, S->getAddrAlign());
    m_EXIDXFragments[S] = EXIDX;
    LayoutInfo *layoutInfo = getModule().getLayoutInfo();
    S->addFragment(EXIDX);
    for (uint32_t i = 0; i < S->size(); i += 8) {
      const uint32_t PieceSize = (S->size() - i >= 8) ? 8 : (S->size() - i);
      EXIDX->addPiece({i, PieceSize});
    }
    if (layoutInfo)
      layoutInfo->recordFragment(&pInput, S, EXIDX);
    return true;
  }
  if (S->getType() == llvm::ELF::SHT_ARM_ATTRIBUTES) {
    llvm::StringRef Region = pInput.getSlice(S->offset(), S->size());
    if (!AttributeFragment) {
      createAttributeSection(S->getFlags(), S->getAddrAlign());
      AttributeFragment = make<ARMAttributeFragment>(m_pARMAttributeSection);
      m_pARMAttributeSection->addFragment(AttributeFragment);
      LayoutInfo *layoutInfo = getModule().getLayoutInfo();
      if (layoutInfo)
        layoutInfo->recordFragment(m_pARMAttributeSection->getInputFile(),
                                m_pARMAttributeSection, AttributeFragment);
    }
    AttributeFragment->updateAttributes(
        Region, m_Module, llvm::dyn_cast<ObjectFile>(&pInput), config());
    return m_pARMAttributeSection;
  }
  return GNULDBackend::readSection(pInput, S);
}

void ARMGNULDBackend::doPostLayout() {
  {
    eld::RegisterTimer T("Sort EXIDX Fragments if Present", "Do Post Layout",
                         m_Module.getConfig().options().printTimingStats());
    ELFSection *exidx =
        m_Module.getScript().sectionMap().find(llvm::ELF::SHT_ARM_EXIDX);
    if (exidx)
      sortEXIDX();
  }

  GNULDBackend::doPostLayout();
}

void ARMGNULDBackend::initSegmentFromLinkerScript(ELFSegment *pSegment) {
  ELFSegment::iterator sect = pSegment->begin(), sectEnd = pSegment->end();
  bool isPrevBSS = false;
  bool hasMixedBSS = false;
  ELFSection *lastMixedNonBSSSection = nullptr;

  for (sect = pSegment->begin(); sect != sectEnd; ++sect) {
    ELFSection *cur = (*sect)->getSection();
    if (isPrevBSS && !cur->isNoBits()) {
      hasMixedBSS = true;
      lastMixedNonBSSSection = cur;
    }
    isPrevBSS = cur->isNoBits();
  }

  if (lastMixedNonBSSSection)
    hasMixedBSS = true;

  if (hasMixedBSS) {
    for (sect = pSegment->begin(); sect != sectEnd; ++sect) {
      ELFSection *cur = (*sect)->getSection();

      if (cur == lastMixedNonBSSSection)
        break;

      if (!cur->isNoBits())
        continue;

      // Convert to PROBIT
      cur->setType(llvm::ELF::SHT_PROGBITS);
      cur->setKind(LDFileFormat::Regular);
      config().raise(Diag::warn_mix_bss_section)
          << lastMixedNonBSSSection->name() << cur->name();
    }
  }
}

void ARMGNULDBackend::reserveTargetDynamicEntries() {
  m_pDynamic->reserveOne(llvm::ELF::DT_RELCOUNT);
}

void ARMGNULDBackend::applyTargetDynamicEntries() {
  uint32_t relaCount = 0;
  for (auto &R : getRelaDyn()->getRelocations())
    if (R->type() == llvm::ELF::R_ARM_RELATIVE)
      relaCount++;
  m_pDynamic->applyOne(llvm::ELF::DT_RELCOUNT, relaCount);
}

void ARMGNULDBackend::defineGOTSymbol(Fragment &pFrag) {
  // define symbol _GLOBAL_OFFSET_TABLE_
  auto SymbolName = "_GLOBAL_OFFSET_TABLE_";
  if (m_pGOTSymbol != nullptr) {
    m_pGOTSymbol =
        m_Module.getIRBuilder()
            ->addSymbol<IRBuilder::Force, IRBuilder::Unresolve>(
                m_Module.getInternalInput(Module::Script), SymbolName,
                ResolveInfo::Object, ResolveInfo::Define, ResolveInfo::Local,
                0x0, // size
                0x0, // value
                make<FragmentRef>(pFrag, 0x0), ResolveInfo::Hidden);
  } else {
    m_pGOTSymbol =
        m_Module.getIRBuilder()
            ->addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
                pFrag.getOwningSection()->getInputFile(), SymbolName,
                ResolveInfo::Object, ResolveInfo::Define, ResolveInfo::Local,
                0x0, // size
                0x0, // value
                make<FragmentRef>(pFrag, 0x0), ResolveInfo::Hidden);
  }
  if (m_Module.getConfig().options().isSymbolTracingRequested() &&
      m_Module.getConfig().options().traceSymbol(SymbolName))
    config().raise(Diag::target_specific_symbol) << SymbolName;
  m_pGOTSymbol->setShouldIgnore(false);
}

bool ARMGNULDBackend::finalizeScanRelocations() {
  Fragment *frag = nullptr;
  if (auto *GOTPLT = getGOTPLT())
    if (GOTPLT->hasSectionData())
      frag = *GOTPLT->getFragmentList().begin();
  if (frag)
    defineGOTSymbol(*frag);

  if (!config().isCodeStatic())
    return true;

  // For an IFunc symbol that is both referenced directly (its address is
  // taken via an absolute / PC-relative reloc, which resolves to PLT[ifunc])
  // and referenced through the GOT, we materialize a dedicated GOT slot that
  // holds the address of PLT[ifunc]. GOT references then resolve to this slot
  // rather than the GOTPLT slot, so that the GOT-loaded pointer equals the
  // direct-reference pointer (pointer equality).
  for (auto &[symInfo, plt] : m_PLTMap) {
    if (!symInfo->isIFunc() || !symInfo->hasIFuncDirectRef() ||
        !symInfo->hasIFuncNeedsGOT())
      continue;
    ELFObjectFile *PLTSlotObjFile =
        llvm::cast<ELFObjectFile>(plt->getOwningSection()->getInputFile());
    ARMGOT *G = createGOT(GOT::Regular, PLTSlotObjFile, symInfo);
    FragmentRef *PLTFragRef = make<FragmentRef>(*plt, 0);
    Relocation *r = Relocation::Create(llvm::ELF::R_ARM_ABS32, 32,
                                       make<FragmentRef>(*G, 0), 0);
    PLTSlotObjFile->getGOT()->addRelocation(r);
    r->modifyRelocationFragmentRef(PLTFragRef);
    // createGOT(GOT::Regular, ...) already calls recordGOT for symInfo.
    symInfo->setReserved(symInfo->reserved() | Relocator::ReserveGOT);
  }
  return true;
}

eld::Expected<uint64_t>
ARMGNULDBackend::emitSection(ELFSection *pSection,
                             MemoryRegion &pRegion) const {
  return GNULDBackend::emitSection(pSection, pRegion);
}

/// finalizeSymbol - finalize the symbol value
bool ARMGNULDBackend::finalizeTargetSymbols() { return true; }

void ARMGNULDBackend::finalizeBeforeWrite() {
  // Update __RWPI_BASE__
  if (m_pRWPIBase && m_pSBRELSegment)
    m_pRWPIBase->setValue(m_pSBRELSegment->vaddr());
  GNULDBackend::finalizeBeforeWrite();
}

bool ARMGNULDBackend::DoesOverrideMerge(ELFSection *pSection) const {
  if (pSection->getKind() == LDFileFormat::Internal)
    return false;
  if (pSection->getType() == llvm::ELF::SHT_ARM_ATTRIBUTES)
    return true;
  if (m_Module.getScript().linkerScriptHasSectionsCommand())
    return false;
  if (LinkerConfig::Object == config().codeGenType())
    return false;
  switch (pSection->getType()) {
  case llvm::ELF::SHT_ARM_ATTRIBUTES:
  case llvm::ELF::SHT_ARM_EXIDX:
    return true;
  }
  return false;
}

ELFSection *ARMGNULDBackend::mergeSection(ELFSection *pSection) {
  switch (pSection->getType()) {
  case llvm::ELF::SHT_ARM_ATTRIBUTES:
    return m_pARMAttributeSection;
  case llvm::ELF::SHT_ARM_EXIDX: {
    if (!pSection->getLink() && pSection->getInputFile())
      config().raise(Diag::warn_armexidx_no_link)
          << pSection->getInputFile()->getInput()->getName()
          << pSection->name();
    else if (pSection->getLink()->isIgnore()) {
      // if the target section of the .ARM.exidx is Ignore, then it should be
      // ignored as well
      pSection->setKind(LDFileFormat::Ignore);
      return nullptr;
    }
    ObjectBuilder builder(config(), m_Module);
    if (builder.moveSection(pSection, m_pEXIDX)) {
      pSection->setMatchedLinkerScriptRule(
          m_pEXIDX->getMatchedLinkerScriptRule());
      pSection->setOutputSection(m_pEXIDX->getOutputSection());
      builder.updateSectionFlags(m_pEXIDX, pSection);
    }
    return m_pEXIDX;
  }
  }
  return nullptr;
}

void ARMGNULDBackend::setUpReachedSectionsForGC(
    GarbageCollection::SectionReachedListMap &pSectReachedListMap) const {
  // traverse all the input relocations to find the relocation sections applying
  // .ARM.exidx sections
  Module::const_obj_iterator input, inEnd = m_Module.objEnd();
  for (input = m_Module.objBegin(); input != inEnd; ++input) {
    ELFObjectFile *ObjFile = llvm::dyn_cast<ELFObjectFile>(*input);
    if (!ObjFile)
      continue;
    for (auto &reloc_sect : ObjFile->getRelocationSections()) {
      // bypass the discarded relocation section
      // 1. its section kind is changed to Ignore. (The target section is a
      // discarded group section.)
      // 2. it has no reloc data. (All symbols in the input relocs are in the
      // discarded group sections)
      ELFSection *apply_sect =
          llvm::dyn_cast_or_null<ELFSection>(reloc_sect->getLink());
      if (reloc_sect->isIgnore())
        continue;
      if (!apply_sect)
        continue;

      if (apply_sect->getOutputSection() &&
          apply_sect->getOutputSection()->isDiscard())
        continue;

      if (apply_sect->isEXIDX()) {
        // 1. set up the reference according to relocations
        bool add_first = false;
        GarbageCollection::SectionListTy *reached_sects = nullptr;
        for (auto &reloc : reloc_sect->getLink()->getRelocations()) {
          ResolveInfo *sym = reloc->symInfo();
          // only the target symbols defined in the input fragments can make the
          // reference
          if (nullptr == sym)
            continue;
          if (!sym->isDefine() || !sym->outSymbol()->hasFragRef())
            continue;

          // only the target symbols defined in the concerned sections can make
          // the reference
          ELFSection *target_sect = sym->getOwningSection();
          if (target_sect->getKind() != LDFileFormat::Regular &&
              target_sect->isNoBits())
            continue;

          // setup the reached list, if we first add the element to reached list
          // of this section, create an entry in ReachedSections map
          if (!add_first) {
            reached_sects = &pSectReachedListMap.getReachedList(*apply_sect);
            add_first = true;
          }
          reached_sects->insert(target_sect);
        }
        reached_sects = nullptr;
        add_first = false;
        // 2. set up the reference from XXX to .ARM.exidx.XXX
        assert(apply_sect->getLink() != nullptr);
        pSectReachedListMap.addReference(*apply_sect->getLink(), *apply_sect);
      }
    }
  }
}

unsigned int
ARMGNULDBackend::getTargetSectionOrder(const ELFSection &pSectHdr) const {
  if (pSectHdr.name() == ".got") {
    if (config().options().hasNow())
      return SHO_RELRO;
    return SHO_NON_RELRO_FIRST;
  }

  if (pSectHdr.name() == ".got.plt") {
    if (config().options().hasNow())
      return SHO_RELRO;
    return SHO_NON_RELRO_FIRST;
  }

  if (pSectHdr.name() == ".plt")
    return SHO_PLT;

  if (pSectHdr.isEXIDX() || pSectHdr.name() == ".ARM.extab")
    // put ARM.exidx and ARM.extab in the same order of .eh_frame
    return SHO_EXCEPTION;

  return SHO_UNDEFINED;
}

Stub *ARMGNULDBackend::getBranchIslandStub(Relocation *pReloc,
                                           int64_t pTargetValue) const {

  assert(getStubFactory() != nullptr);
  if (pReloc->shouldUsePLTAddr())
    pTargetValue = getPLTAddr(pReloc->symInfo());
  for (auto &i : getStubFactory()->getAllStubs()) {
    if (i->isNeeded(pReloc, pTargetValue, m_Module))
      return i;
  }
  return nullptr;
}

void ARMGNULDBackend::mayBeRelax(int, bool &pFinished) {
  if (config().options().noTrampolines()) {
    pFinished = true;
    return;
  }
  assert(nullptr != getStubFactory() && nullptr != getBRIslandFactory());
  ELFFileFormat *file_format = getOutputFormat();
  pFinished = true;

  // check branch relocs and create the related stubs if needed
  Module::obj_iterator input, inEnd = m_Module.objEnd();
  for (input = m_Module.objBegin(); input != inEnd; ++input) {
    ELFObjectFile *ObjFile = llvm::dyn_cast<ELFObjectFile>(*input);
    if (!ObjFile)
      continue;
    for (auto &rs : ObjFile->getRelocationSections()) {
      if (rs->isIgnore())
        continue;
      for (auto &reloc : rs->getLink()->getRelocations()) {
        // Undef weak call is converted to NOP, no need for any stubs
        if (reloc->symInfo()->isWeak() && reloc->symInfo()->isUndef() &&
            !reloc->symInfo()->isDyn() &&
            !(reloc->symInfo()->reserved() & Relocator::ReservePLT))
          continue;

        switch (reloc->type()) {
        case llvm::ELF::R_ARM_PC24:
        case llvm::ELF::R_ARM_CALL:
        case llvm::ELF::R_ARM_JUMP24:
        case llvm::ELF::R_ARM_PLT32:
        case llvm::ELF::R_ARM_THM_CALL:
        case llvm::ELF::R_ARM_THM_JUMP24:
        case llvm::ELF::R_ARM_THM_XPC22:
        case llvm::ELF::R_ARM_THM_JUMP19: {
          Relocation *relocation = llvm::cast<Relocation>(reloc);
          if (relocation->symInfo()->isUndef() &&
              (relocation->symInfo()->reserved() & Relocator::ReservePLT) == 0)
            continue;
          std::pair<BranchIsland *, bool> branchIsland =
              getStubFactory()->create(*relocation, // relocation
                                       *m_Module.getIRBuilder(),
                                       *getBRIslandFactory(), *this);
          if (branchIsland.first && !branchIsland.second) {
            switch (config().options().getStripSymbolMode()) {
            case GeneralOptions::StripAllSymbols:
            case GeneralOptions::StripLocals:
              break;
            default: {
              // a stub symbol should be local
              ELFSection &symtab = *file_format->getSymTab();
              ELFSection &strtab = *file_format->getStrTab();

              // increase the size of .symtab and .strtab if needed
              symtab.setSize(symtab.size() + sizeof(llvm::ELF::Elf32_Sym));
              symtab.setInfo(symtab.getInfo() + 1);
              strtab.setSize(strtab.size() +
                             branchIsland.first->symInfo()->nameSize() + 1);
            }
            } // end of switch
            pFinished = false;
          }
        } break;

        default:
          break;
        }
      }
    }
  }
}

/// initTargetStubs
bool ARMGNULDBackend::initTargetStubs() {
  StubFactory *factory = getStubFactory();
  if (nullptr != factory) {
    uint32_t type = VENEER_ABS;
    if (config().isCodeIndep())
      type = VENEER_PIC;
    else if (config().options().getUseMovVeneer())
      type = VENEER_MOV;

    factory->registerStub(make<ARMToARMStub>(type, this));
    factory->registerStub(make<ARMToTHMStub>(type, this));

    if (!isMicroController())
      factory->registerStub(make<THMToTHMStub>(type, this));
    else {
      if (canUseMovTMovW())
        factory->registerStub(make<THMToTHMStub>(VENEER_MOV, this));
      else
        factory->registerStub(make<THMToTHMStub>(VENEER_THUMB1, this));
    }

    factory->registerStub(make<THMToARMStub>(type, this));
    return true;
  }
  return false;
}

/// doCreateProgramHdrs - backend can implement this function to create the
/// target-dependent segments
void ARMGNULDBackend::doCreateProgramHdrs() {
  ELFSection *exidx =
      m_Module.getScript().sectionMap().find(llvm::ELF::SHT_ARM_EXIDX);
  if (nullptr != exidx && 0x0 != exidx->size()) {
    // make PT_ARM_EXIDX
    ELFSegment *exidx_seg =
        make<ELFSegment>(llvm::ELF::PT_ARM_EXIDX, llvm::ELF::PF_R);
    elfSegmentTable().addSegment(exidx_seg);
    exidx_seg->setAlign(exidx->getAddrAlign());
    exidx_seg->append(exidx->getOutputSection());
  }
}

int ARMGNULDBackend::numReservedSegments() const {
  ELFSegment *exidx_segment = elfSegmentTable().find(llvm::ELF::PT_ARM_EXIDX);
  if (exidx_segment)
    return GNULDBackend::numReservedSegments();
  int numReservedSegments = 0;
  ELFSection *exidx =
      m_Module.getScript().sectionMap().find(llvm::ELF::SHT_ARM_EXIDX);
  if (nullptr != exidx && 0x0 != exidx->size())
    ++numReservedSegments;
  return numReservedSegments + GNULDBackend::numReservedSegments();
}

void ARMGNULDBackend::addTargetSpecificSegments() {
  ELFSegment *exidx_segment = elfSegmentTable().find(llvm::ELF::PT_ARM_EXIDX);
  if (exidx_segment)
    return;
  doCreateProgramHdrs();
}

bool ARMGNULDBackend::ltoNeedAssembler() {
  if (!config().options().getSaveTemps())
    return false;
  return true;
}

uint64_t ARMGNULDBackend::getSectLink(const ELFSection *S) const {
  if (S->isEXIDX() && S->getLink())
    return S->getLink()->getIndex();
  return GNULDBackend::getSectLink(S);
}

Relocation::Type ARMGNULDBackend::getCopyRelType() const {
  return llvm::ELF::R_ARM_COPY;
}

bool ARMGNULDBackend::ltoCallExternalAssembler(const std::string &Input,
                                               std::string RelocModel,
                                               const std::string &Output) {
  bool traceLTO = config().options().traceLTO();

  // Invoke assembler.
  std::string assembler = "clang";
  std::vector<StringRef> assemblerArgs;

  llvm::ErrorOr<std::string> assemblerPath =
      llvm::sys::findProgramByName(assembler);
  if (!assemblerPath) {
    // Look for the assembler within the folder where the linker is
    std::string apath = config().options().linkerPath();
    apath += "/" + assembler;
    if (!llvm::sys::fs::exists(apath))
      return false;
    else
      assemblerPath = apath;
  }
  std::string cpu = "-mcpu=" + config().targets().getTargetCPU();
  assemblerArgs.push_back(assemblerPath->c_str());
  assemblerArgs.push_back("-cc1as");
  assemblerArgs.push_back("-triple");
  assemblerArgs.push_back("armv4t--linux-gnueabi");
  assemblerArgs.push_back("-filetype");
  assemblerArgs.push_back("obj");
  assemblerArgs.push_back("-mrelax-all");
  if (!RelocModel.empty()) {
    assemblerArgs.push_back("-mrelocation-model");
    assemblerArgs.push_back(RelocModel.c_str());
  }
  // Do target feature
  std::vector<std::string> featureStrings;
  if (config().options().codegenOpts()) {
    for (auto ai : config().options().codeGenOpts()) {
      if (ai.compare(0, 7, "-mattr=") != 0)
        continue;

      llvm::StringRef feature = Saver.save(ai.substr(7));
      featureStrings.push_back(feature.str());
      assemblerArgs.push_back("-target-feature");
      assemblerArgs.push_back(feature.data());
    }
  }

  assemblerArgs.push_back(Input.c_str());
  assemblerArgs.push_back("-o");
  assemblerArgs.push_back(Output.c_str());

  if (traceLTO) {
    std::stringstream ss;
    for (auto s : assemblerArgs) {
      if (s.data())
        ss << s.data() << " ";
    }
    config().raise(Diag::process_launch) << ss.str();
  }

  return !(llvm::sys::ExecuteAndWait(assemblerPath->c_str(), assemblerArgs));
}

// Create GOT entry.
ARMGOT *ARMGNULDBackend::createGOT(GOT::GOTType T, ELFObjectFile *Obj,
                                   ResolveInfo *R, bool SkipPLTRef) {
  if (R != nullptr && ((config().options().isSymbolTracingRequested() &&
                        config().options().traceSymbol(*R)) ||
                       m_Module.getPrinter()->traceDynamicLinking()))
    config().raise(Diag::create_got_entry)
        << GOT::getGOTTypeAsStr(T) << R->name();
  // If we are creating a GOT, always create a .got.plt.
  if (!getGOTPLT()->hasFragments()) {
    // TODO: This should be GOT0, not GOTPLT0.
    LDSymbol *Dynamic = m_Module.getNamePool().findSymbol("_DYNAMIC");
    ARMGOTPLT0::Create(getGOTPLT(), Dynamic ? Dynamic->resolveInfo() : nullptr);
  }

  ARMGOT *G = nullptr;
  bool GOT = true;
  switch (T) {
  case GOT::Regular:
    G = ARMGOT::Create(Obj->getGOT(), R);
    break;
  case GOT::GOTPLT0:
    G = llvm::dyn_cast<ARMGOT>(*getGOTPLT()->getFragmentList().begin());
    GOT = false;
    break;
  case GOT::GOTPLTN: {
    // Fill GOT PLT slots with address of PLT0.
    // If the symbol is IRELATIVE, the PLT slot contains the relative symbol
    // value. No need to fill the GOT slot with PLT0.
    // No PLT0 for immediate binding.
    Fragment *F = SkipPLTRef ? nullptr : *getPLT()->getFragmentList().begin();
    G = ARMGOTPLTN::Create(Obj->getGOTPLT(), R, F);
    GOT = false;
    break;
  }
  case GOT::TLS_GD:
    G = ARMGDGOT::Create(Obj->getGOT(), R);
    break;
  case GOT::TLS_LD:
    // TODO: use a synthetic input file, separate from GOT header.
    G = ARMLDGOT::Create(getGOT(), R);
    break;
  case GOT::TLS_IE:
    G = ARMIEGOT::Create(Obj->getGOT(), R);
    break;
  default:
    assert(0);
    break;
  }
  if (R) {
    if (GOT) {
      reportErrorIfGOTIsDiscarded(R);
      recordGOT(R, G);
    } else {
      reportErrorIfGOTPLTIsDiscarded(R);
      recordGOTPLT(R, G);
    }
  }
  return G;
}

// Record GOT entry.
void ARMGNULDBackend::recordGOT(ResolveInfo *I, ARMGOT *G) { m_GOTMap[I] = G; }

// Record GOTPLT entry.
void ARMGNULDBackend::recordGOTPLT(ResolveInfo *I, ARMGOT *G) {
  m_GOTPLTMap[I] = G;
}

// Find an entry in the GOT.
ARMGOT *ARMGNULDBackend::findEntryInGOT(ResolveInfo *I) const {
  auto Entry = m_GOTMap.find(I);
  if (Entry == m_GOTMap.end())
    return nullptr;
  return Entry->second;
}

int64_t ARMGNULDBackend::getPLTAddr(ResolveInfo *pInfo) const {
  auto slot = findEntryInPLT(pInfo);
  assert(slot != nullptr && "Requested PLT for unreserved slot");
  return slot->getAddr(config().getDiagEngine());
}

// Create PLT entry.
ARMPLT *ARMGNULDBackend::createPLT(ELFObjectFile *Obj, ResolveInfo *R,
                                   bool isIRelative) {
  bool hasNow = config().options().hasNow();
  if (R != nullptr && ((config().options().isSymbolTracingRequested() &&
                        config().options().traceSymbol(*R)) ||
                       m_Module.getPrinter()->traceDynamicLinking()))
    config().raise(Diag::create_plt_entry) << R->name();

  reportErrorIfPLTIsDiscarded(R);

  // If there is no entries GOTPLT and PLT, we dont have a PLT0.
  if (!getPLT()->hasFragments()) {
    ARMPLT0::Create(*m_Module.getIRBuilder(),
                    createGOT(GOT::GOTPLT0, nullptr, nullptr), getPLT(),
                    nullptr);
  }
  ARMPLT *P = ARMPLTN::Create(
      *m_Module.getIRBuilder(),
      createGOT(GOT::GOTPLTN, Obj, R, hasNow || isIRelative), Obj->getPLT(), R);
  // init the corresponding rel entry in .rel.plt
  Relocation *rel_entry = Obj->getRelaPLT()->createOneReloc();
  rel_entry->setType(isIRelative ? llvm::ELF::R_ARM_IRELATIVE
                                 : llvm::ELF::R_ARM_JUMP_SLOT);
  rel_entry->setTargetRef(make<FragmentRef>(*P->getGOT(), 0));
  if (isIRelative)
    P->getGOT()->setValueType(GOT::SymbolValue);
  rel_entry->setSymInfo(R);
  if (R)
    recordPLT(R, P);
  return P;
}

// Record GOT entry.
void ARMGNULDBackend::recordPLT(ResolveInfo *I, ARMPLT *P) { m_PLTMap[I] = P; }

// Find an entry in the GOT.
ARMPLT *ARMGNULDBackend::findEntryInPLT(ResolveInfo *I) const {
  auto Entry = m_PLTMap.find(I);
  if (Entry == m_PLTMap.end())
    return nullptr;
  return Entry->second;
}

bool ARMGNULDBackend::canRewriteToBLX() const {
  // We always rewrite the instruction to BLX except for cases if
  // microcontroller
  return AttributeFragment && !AttributeFragment->isCPUProfileMicroController();
}

bool ARMGNULDBackend::isMicroController() const {
  llvm::StringRef CPUName = config().targets().getTargetCPU();
  return CPUName.equals_insensitive("cortex-m0") ||
         (AttributeFragment &&
          AttributeFragment->isCPUProfileMicroController());
}

bool ARMGNULDBackend::isJ1J2BranchEncoding() const {
  return AttributeFragment && AttributeFragment->hasJ1J2Encoding();
}

bool ARMGNULDBackend::canUseMovTMovW() const {
  return AttributeFragment && AttributeFragment->hasMovtMovW();
}

void ARMGNULDBackend::initializeAttributes() {
  getInfo().initializeAttributes(m_Module.getIRBuilder()->getInputBuilder());
}

bool ARMGNULDBackend::handleRelocation(ELFSection *Section,
                                       Relocation::Type Type, LDSymbol &Sym,
                                       uint32_t Offset,
                                       Relocation::Address Addend) {
  auto It = m_EXIDXFragments.find(Section);
  if (It != m_EXIDXFragments.end()) {
    (void)It->second->getPiece(Offset);
    Relocation *R = eld::IRBuilder::addRelocation(getRelocator(), *It->second,
                                                  Type, Sym, Offset);
    Section->addRelocation(R);
    return true;
  }
  return false;
}

void ARMGNULDBackend::setDefaultConfigs() {
  GNULDBackend::setDefaultConfigs();
  if (config().options().threadsEnabled() &&
      !config().isGlobalThreadingEnabled()) {
    config().disableThreadOptions(
        LinkerConfig::EnableThreadsOpt::ScanRelocations |
        LinkerConfig::EnableThreadsOpt::ApplyRelocations |
        LinkerConfig::EnableThreadsOpt::LinkerRelaxation);
  }
}

namespace eld {

//===----------------------------------------------------------------------===//
/// createARMLDBackend - the help function to create corresponding ARMLDBackend
///
GNULDBackend *createARMLDBackend(Module &pModule) {
  return make<ARMGNULDBackend>(pModule, make<ARMInfo>(pModule.getConfig()));
}

} // namespace eld

//===----------------------------------------------------------------------===//
// Force static initialization.
//===----------------------------------------------------------------------===//
extern "C" void ELDInitializeARMLDBackend() {
  // Register the linker backend
  eld::TargetRegistry::RegisterGNULDBackend(TheARMTarget, createARMLDBackend);
}
