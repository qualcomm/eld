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
#include "ARMELFDynamic.h"
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
#include "eld/Target/ARMEXIDXSection.h"
#include "eld/Target/ELFFileFormat.h"
#include "eld/Target/ELFSegment.h"
#include "eld/Target/ELFSegmentFactory.h"
#include "eld/Target/GNULDBackend.h"
#include "eld/Target/TargetInfo.h"
#include "llvm/ADT/Twine.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Object/ELFTypes.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Program.h"
#include <cstring>

using namespace eld;
using namespace llvm;

//===----------------------------------------------------------------------===//
// ARMGNULDBackend
//===----------------------------------------------------------------------===//
ARMGNULDBackend::ARMGNULDBackend(eld::Module &pModule, TargetInfo *pInfo)
    : GNULDBackend(pModule, pInfo), m_pRelocator(nullptr), m_pDynamic(nullptr),
      m_pEXIDXStart(nullptr), m_pEXIDXEnd(nullptr), m_pIRelativeStart(nullptr),
      m_pIRelativeEnd(nullptr), m_pEXIDX(nullptr),
      m_pRegionTableSection(nullptr), m_pRegionTableFragment(nullptr),
      m_pRWPIBase(nullptr), m_pSBRELSegment(nullptr),
      m_pARMAttributeSection(nullptr), AttributeFragment(nullptr),
      m_bEmitRegionTable(false) {}

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

  // Create a RegionTable section.
  m_pRegionTableSection = m_Module.createInternalSection(
      Module::InternalInputType::RegionTable, LDFileFormat::Internal,
      "__region_table__", llvm::ELF::SHT_PROGBITS, llvm::ELF::SHF_ALLOC, 4);
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

  SymbolName = "__exidx_start";
  m_pEXIDXStart =
      m_Module.getIRBuilder()
          ->addSymbol<IRBuilder::Force, IRBuilder::Unresolve>(
              m_Module.getInternalInput(Module::Script), SymbolName,
              ResolveInfo::NoType, ResolveInfo::Define, ResolveInfo::Global,
              0x0, // size
              0x0, // value
              FragmentRef::null(), ResolveInfo::Default);
  if (m_pEXIDXStart)
    m_pEXIDXStart->setShouldIgnore(false);
  if (m_Module.getConfig().options().isSymbolTracingRequested() &&
      m_Module.getConfig().options().traceSymbol(SymbolName))
    config().raise(Diag::target_specific_symbol) << SymbolName;

  SymbolName = "__exidx_end";
  m_pEXIDXEnd =
      m_Module.getIRBuilder()
          ->addSymbol<IRBuilder::Force, IRBuilder::Unresolve>(
              m_Module.getInternalInput(Module::Script), SymbolName,
              ResolveInfo::NoType, ResolveInfo::Define, ResolveInfo::Global,
              0x0, // size
              0x0, // value
              FragmentRef::null(), ResolveInfo::Default);

  if (m_pEXIDXEnd)
    m_pEXIDXEnd->setShouldIgnore(false);
  if (m_Module.getConfig().options().isSymbolTracingRequested() &&
      m_Module.getConfig().options().traceSymbol(SymbolName))
    config().raise(Diag::target_specific_symbol) << SymbolName;

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

  ELFSection *exidx =
      m_Module.getScript().sectionMap().find(llvm::ELF::SHT_ARM_EXIDX);
  if (exidx && exidx->size()) {
    OutputSectionEntry *O = exidx->getOutputSection();
    Fragment *Start = nullptr;
    Fragment *End = nullptr;
    ELFSection *Last = nullptr;
    for (auto &In : *O) {
      ELFSection *S = In->getSection();
      if (S->size() == 0)
        continue;
      Last = S;
      if (!Start) {
        Start = Last->getFragmentList().front();
      }
      if (Last)
        End = Last->getFragmentList().back();

      FragmentRef *exidx_start = make<FragmentRef>(*Start, 0);

      FragmentRef *exidx_end = make<FragmentRef>(*End, End->size());

      ResolveInfo oldStart, oldEnd;

      // FIXME: need real provide support. This will fail if trampoline inserted
      // inside the EXIDX section
      if (m_pEXIDXStart) {
        m_pEXIDXStart->setValue(exidx->addr() +
                                exidx_start->getOutputOffset(m_Module));
        oldStart.override(*m_pEXIDXStart->resolveInfo());
      }
      if (m_pEXIDXEnd) {
        m_pEXIDXEnd->setValue(exidx->addr() +
                              exidx_end->getOutputOffset(m_Module));
        oldEnd.override(*m_pEXIDXEnd->resolveInfo());
      }

      if (m_pEXIDXEnd) {
        m_pEXIDXEnd->setFragmentRef(exidx_end);
        m_pEXIDXEnd->resolveInfo()->setType(ResolveInfo::Object);
      }
      if (m_pEXIDXStart) {
        m_pEXIDXStart->setFragmentRef(exidx_start);
        m_pEXIDXStart->resolveInfo()->setType(ResolveInfo::Object);
      }
    }
  }

  // initialize .dynamic data
  if ((!config().isCodeStatic() || config().options().forceDynamic()) &&
      nullptr == m_pDynamic)
    m_pDynamic = make<ARMELFDynamic>(*this, config());

  // set .got size
  // when building shared object, the .got section is must
  if (LinkerConfig::Object != config().codeGenType()) {
    getRelaPLT()->setSize(getRelaPLT()->getRelocations().size() *
                          getRelEntrySize());
    getRelaDyn()->setSize(getRelaDyn()->getRelocations().size() *
                          getRelEntrySize());
    m_Module.addOutputSection(getRelaPLT());
    m_Module.addOutputSection(getRelaDyn());
  }

  // We need link ARM.EXIDX.xx to .xx
  Module::obj_iterator input, inEnd = m_Module.objEnd();
  for (input = m_Module.objBegin(); input != inEnd; ++input) {
    ELFObjectFile *ObjFile = llvm::dyn_cast<ELFObjectFile>(*input);
    if (!ObjFile)
      continue;
    for (auto &sect : ObjFile->getSections()) {
      if (sect->isBitcode())
        continue;
      ELFSection *section = llvm::dyn_cast<eld::ELFSection>(sect);
      if (section->isIgnore())
        continue;
      if (section->isDiscard())
        continue;
      if (!section->isEXIDX())
        continue;
      // get the output relocation ELFSection with name in accordance with
      // linker script rule for the section where relocations are patched
      llvm::StringRef outputName = section->name();

      ELFSection *output_sect = nullptr;
      OutputSectionEntry *outputSection = section->getOutputSection();
      if (outputSection)
        output_sect = outputSection->getSection();
      else
        output_sect = m_Module.getSection(outputName.str());
      if (nullptr == output_sect)
        continue;

      // set output relocation section link
      const ELFSection *input_link =
          llvm::dyn_cast_or_null<ELFSection>(section->getLink());
      assert(nullptr != input_link && "Illegal input ARM.exidx section.");

      // get the linked output section
      ELFSection *output_link = nullptr;
      outputSection = input_link->getOutputSection();
      if (outputSection)
        output_link = outputSection->getSection();
      else
        output_link = m_Module.getSection(input_link->name().str());

      assert(nullptr != output_link);

      output_sect->setLink(output_link);
    }
  }

  // check if entry is missing
  LDSymbol *entry_sym = m_Module.getNamePool().findSymbol(getEntry().str());
  Relocation *entry_reloc = nullptr;
  ELFSection *Last = nullptr;
  if (exidx && (entry_sym != nullptr) && (entry_sym->hasFragRef())) {
    for (auto &In : *exidx->getOutputSection()) {
      ELFSection *S = In->getSection();
      if (S->size())
        Last = S;
      for (auto &F : S->getFragmentList()) {
        for (auto &relocation : F->getOwningSection()->getRelocations()) {
          // bypass the reloc if the symbol is in the discarded input section
          ResolveInfo *info = relocation->symInfo();

          if (ResolveInfo::Section == info->type() &&
              ResolveInfo::Undefined == info->desc())
            continue;

          // bypass the reloc if the section where it sits will be discarded.
          if (relocation->targetRef()->frag()->getOwningSection()->isIgnore())
            continue;

          if (relocation->targetRef()->frag()->getOwningSection()->isDiscard())
            continue;

          uint64_t reloc_offset = relocation->targetRef()->offset();
          if (!reloc_offset) {
            Fragment *region_frag =
                relocation->symInfo()->outSymbol()->fragRef()->frag();
            if (region_frag == entry_sym->fragRef()->frag()) {
              entry_reloc = relocation;
              break;
            }
          }
        }
      }
    }

    LayoutPrinter *printer = getModule().getLayoutPrinter();
    if (!entry_reloc) {
      static const char raw_data[] = "\x00\x00\x00\x00\x01\x00\x00\x00";
      StringRef entry_data(raw_data, 8);
      size_t align = entry_sym->fragRef()->frag()->alignment();
      Fragment *frag =
          make<RegionFragment>(entry_data, Last, Fragment::Type::Region, align);
      Last->addFragmentAndUpdateSize(frag);
      if (printer)
        printer->recordFragment(Last->getInputFile(), Last, frag);
      // create the relocation against this entry
      entry_reloc = Relocation::Create(llvm::ELF::R_ARM_PREL31, 32,
                                       make<FragmentRef>(*frag, 0), 0);
      entry_reloc->setSymInfo(entry_sym->resolveInfo());
      Last->addRelocation(entry_reloc);
      m_InternalRelocs.push_back(entry_reloc);
    }
  }
}

void ARMGNULDBackend::sortEXIDX() {
  // Mark all fragments.
  ELFSection *E =
      m_Module.getScript().sectionMap().find(llvm::ELF::SHT_ARM_EXIDX);

  if (!E)
    return;

  if (!E->size())
    return;

  OutputSectionEntry *O = E->getOutputSection();
  llvm::SmallVector<Fragment *, 0> Frags;
  ELFSection *exidx = nullptr;
  // Scan relocation to the fragment.
  for (auto &In : *O) {
    ELFSection *S = In->getSection();
    if (!exidx)
      exidx = In->getSection();
    for (auto &F : S->getFragmentList())
      Frags.push_back(F);
    S->getFragmentList().clear();
  }

  if (O->getLastRule()) {
    exidx = O->getLastRule()->getSection();
    exidx->setMatchedLinkerScriptRule(O->getLastRule());
  }
  for (auto &F : Frags)
    F->getOwningSection()->setMatchedLinkerScriptRule(
        exidx->getMatchedLinkerScriptRule());
  exidx->splice(exidx->getFragmentList().end(), Frags);

  for (auto &F : exidx->getFragmentList()) {
    for (auto &relocation : F->getOwningSection()->getRelocations()) {
      // bypass the reloc if the symbol is in the discarded input section
      ResolveInfo *info = relocation->symInfo();

      if (ResolveInfo::Section == info->type() &&
          ResolveInfo::Undefined == info->desc())
        continue;

      // bypass the reloc if the section where it sits will be discarded.
      if (relocation->targetRef()->frag()->getOwningSection()->isIgnore())
        continue;

      if (relocation->targetRef()->frag()->getOwningSection()->isDiscard())
        continue;

      if (0x0 == relocation->type())
        continue;

      uint64_t reloc_offset = relocation->targetRef()->offset();
      if (!reloc_offset) {
        // this is key
        Fragment *region_frag = relocation->targetRef()->frag();
        region_frag->setFragmentKind(Fragment::Region);

        Relocator::Address S = getRelocator()->getSymValue(relocation);
        Relocator::Address key = S + (relocation->target() & 0xFFFFFFFF);
        region_frag->setOffset(key); // key is used for sorting
      }
    } // for all relocations
  } // for all relocation section

  // Sort fragments by key value stored in offset
  std::sort(exidx->getFragmentList().begin(), exidx->getFragmentList().end(),
            [this](Fragment *i, Fragment *j) {
              Relocator::Address i_offset =
                  i->getOffset(config().getDiagEngine());
              Relocator::Address j_offset =
                  j->getOffset(config().getDiagEngine());
              return (i_offset < j_offset);
            });

  // Reset offset to real offset
  uint64_t offset = 0;
  for (auto &Frag : exidx->getFragmentList()) {
    if (Frag->isNull())
      continue;
    Frag->setOffset(offset);
    offset += 8;
  }

  O->setFirstNonEmptyRule(exidx->getMatchedLinkerScriptRule());

  // Reset EXIDX symbols.
  if (m_pEXIDXStart) {
    m_pEXIDXStart->fragRef()->setFragment(exidx->getFragmentList().front());
    m_pEXIDXStart->fragRef()->setOffset(0);
  }
  if (m_pEXIDXEnd) {
    m_pEXIDXEnd->fragRef()->setFragment(exidx->getFragmentList().back());
    m_pEXIDXEnd->fragRef()->setOffset(8);
  }
}

bool ARMGNULDBackend::readSection(InputFile &pInput, ELFSection *S) {
  // We need break them down to individual entry
  if (auto *EXIDX = llvm::dyn_cast<ARMEXIDXSection>(S)) {
    uint32_t Offset = 0;
    LayoutPrinter *printer = getModule().getLayoutPrinter();
    for (uint32_t i = 0; i < S->size(); i += 8) {
      llvm::StringRef region = pInput.getSlice(S->offset() + i, 8);
      Fragment *frag = make<RegionFragment>(region, S, Fragment::Type::Region,
                                            S->getAddrAlign());
      if (printer)
        printer->recordFragment(&pInput, S, frag);
      EXIDX->addFragment(frag);
      EXIDX->addEntry({Offset, frag});
      Offset += 8;
    }
    return true;
  }
  if (S->getType() == llvm::ELF::SHT_ARM_ATTRIBUTES) {
    llvm::StringRef Region = pInput.getSlice(S->offset(), S->size());
    if (!AttributeFragment) {
      createAttributeSection(S->getFlags(), S->getAddrAlign());
      AttributeFragment = make<ARMAttributeFragment>(m_pARMAttributeSection);
      m_pARMAttributeSection->getFragmentList().push_back(AttributeFragment);
      LayoutPrinter *printer = getModule().getLayoutPrinter();
      if (printer)
        printer->recordFragment(m_pARMAttributeSection->getInputFile(),
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
    if (exidx && exidx->size())
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

/// dynamic - the dynamic section of the target machine.
/// Use co-variant return type to return its own dynamic section.
ARMELFDynamic *ARMGNULDBackend::dynamic() { return m_pDynamic; }

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
  return true;
}

eld::Expected<uint64_t>
ARMGNULDBackend::emitSection(ELFSection *pSection,
                             MemoryRegion &pRegion) const {
  return GNULDBackend::emitSection(pSection, pRegion);
}

void ARMGNULDBackend::defineIRelativeRange(ResolveInfo &pSym) {
  // It is up to linker script to define those symbols.
  if (m_Module.getScript().linkerScriptHasSectionsCommand())
    return;

  // Define the copy symbol in the bss section and resolve it
  if (!m_pIRelativeStart && !m_pIRelativeEnd) {
    auto SymbolName = "__rel_iplt_start";
    m_pIRelativeStart =
        m_Module.getIRBuilder()
            ->addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
                m_Module.getInternalInput(Module::Script), SymbolName,
                ResolveInfo::Object, ResolveInfo::Define,
                (ResolveInfo::Binding)pSym.binding(),
                0,   // size
                0x0, // value
                FragmentRef::null(), (ResolveInfo::Visibility)pSym.other());

    m_pIRelativeStart->setShouldIgnore(false);
    if (m_Module.getConfig().options().isSymbolTracingRequested() &&
        m_Module.getConfig().options().traceSymbol(SymbolName))
      config().raise(Diag::target_specific_symbol) << SymbolName;
    SymbolName = "__rel_iplt_end";
    m_pIRelativeEnd =
        m_Module.getIRBuilder()
            ->addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
                m_Module.getInternalInput(Module::Script), SymbolName,
                ResolveInfo::Object, ResolveInfo::Define,
                (ResolveInfo::Binding)pSym.binding(),
                pSym.size(), // size
                0x0,         // value
                FragmentRef::null(), (ResolveInfo::Visibility)pSym.other());

    m_pIRelativeEnd->setShouldIgnore(false);
    if (m_Module.getConfig().options().isSymbolTracingRequested() &&
        m_Module.getConfig().options().traceSymbol(SymbolName))
      config().raise(Diag::target_specific_symbol) << SymbolName;
  }
}

/// finalizeSymbol - finalize the symbol value
bool ARMGNULDBackend::finalizeTargetSymbols() {
  if (m_pIRelativeStart && m_pIRelativeEnd) {
    m_pIRelativeStart->setValue(
        getRelaPLT()->getOutputSection()->getSection()->addr());
    m_pIRelativeEnd->setValue(
        getRelaPLT()->getOutputSection()->getSection()->addr() +
        getRelaPLT()->getOutputSection()->getSection()->size());
  }
  return true;
}

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
    config().raise(Diag::create_got_entry) << R->name();
  // If we are creating a GOT, always create a .got.plt.
  if (!getGOTPLT()->getFragmentList().size()) {
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
    if (GOT)
      recordGOT(R, G);
    else
      recordGOTPLT(R, G);
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
  // If there is no entries GOTPLT and PLT, we dont have a PLT0.
  if (!getPLT()->getFragmentList().size()) {
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

void ARMGNULDBackend::finishAssignOutputSections() {
  OutputSectionEntry *O = m_pRegionTableSection->getOutputSection();

  // No region table for partial linking.
  if (config().codeGenType() == LinkerConfig::Object)
    return;

  // No region table for PIE or Dynamic libraries.
  if ((config().codeGenType() == LinkerConfig::DynObj) ||
      (config().options().isPIE()))
    return;

  if (O && ((O->name() != ".unrecognized") && !(O->isDiscard())))
    m_bEmitRegionTable = true;

  // Dont create a fragment if nothing matched.
  if (!m_bEmitRegionTable)
    return;

  // Create a RegionTable Fragment.
  m_pRegionTableFragment =
      make<RegionTableFragment<llvm::object::ELF32LE>>(m_pRegionTableSection);
  m_pRegionTableSection->addFragmentAndUpdateSize(m_pRegionTableFragment);
  LayoutPrinter *printer = getModule().getLayoutPrinter();
  if (printer)
    printer->recordFragment(m_pRegionTableSection->getInputFile(),
                            m_pRegionTableSection, m_pRegionTableFragment);
}

// Update the RegionTable with updated information from the Backend.
bool ARMGNULDBackend::updateTargetSections() {
  if (!m_pRegionTableFragment)
    return false;
  return m_pRegionTableFragment->updateInfo(this);
}

bool ARMGNULDBackend::handleBSS(const ELFSection *prev,
                                const ELFSection *cur) const {
  return ((GNULDBackend::handleBSS(prev, cur)) && !m_bEmitRegionTable);
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
                                       Relocation::Address Addend,
                                       bool LastVisit) {
  if (auto *EXIDX = llvm::dyn_cast<ARMEXIDXSection>(Section)) {
    EXIDXEntry Entry = EXIDX->getEntry(Offset);
    Relocation *R = eld::IRBuilder::addRelocation(
        getRelocator(), *Entry.Fragment, Type, Sym, Offset - Entry.InputOffset);
    EXIDX->addRelocation(R);
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
/// createARMLDBackend - the help funtion to create corresponding ARMLDBackend
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
  eld::TargetRegistry::RegisterGNULDBackend(TheThumbTarget, createARMLDBackend);
}
