//===- AArch64LDBackend.cpp------------------------------------------------===//
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

#include "AArch64LDBackend.h"
#include "AArch64.h"
#include "AArch64ELFDynamic.h"
#include "AArch64Errata843419Stub.h"
#include "AArch64FarcallStub.h"
#include "AArch64Info.h"
#include "AArch64InsnHelpers.h"
#include "AArch64LinuxInfo.h"
#include "AArch64Relocator.h"
#include "eld/BranchIsland/BranchIslandFactory.h"
#include "eld/BranchIsland/StubFactory.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Fragment/FillFragment.h"
#include "eld/Fragment/RegionFragment.h"
#include "eld/Fragment/Stub.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/Object/ObjectBuilder.h"
#include "eld/Support/MemoryArea.h"
#include "eld/Support/MemoryRegion.h"
#include "eld/Support/MsgHandling.h"
#include "eld/Support/RegisterTimer.h"
#include "eld/Support/TargetRegistry.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "eld/Target/ELFFileFormat.h"
#include "eld/Target/ELFSegment.h"
#include "eld/Target/ELFSegmentFactory.h"
#include "eld/Target/TargetInfo.h"
#include "llvm/ADT/Twine.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Object/ELFTypes.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Program.h"
#include "llvm/TargetParser/Triple.h"
#include <cstring>

using namespace eld;
using namespace llvm;

//===----------------------------------------------------------------------===//
// AArch64GNUInfoLDBackend
//===----------------------------------------------------------------------===//
AArch64GNUInfoLDBackend::AArch64GNUInfoLDBackend(eld::Module &pModule,
                                                 TargetInfo *pInfo)
    : GNULDBackend(pModule, pInfo), m_pErrata843419Factory(nullptr),
      m_pAArch64ErrataIslandFactory(nullptr), m_pRelocator(nullptr),
      m_pDynamic(nullptr), m_pIRelativeStart(nullptr), m_pIRelativeEnd(nullptr),
      m_ptdata(nullptr), m_ptbss(nullptr) {}

AArch64GNUInfoLDBackend::~AArch64GNUInfoLDBackend() {}

bool AArch64GNUInfoLDBackend::initBRIslandFactory() {
  if (nullptr == m_pBRIslandFactory) {
    m_pBRIslandFactory = make<BranchIslandFactory>(false, config());
  }
  if (nullptr == m_pAArch64ErrataIslandFactory)
    m_pAArch64ErrataIslandFactory = make<AArch64ErrataIslandFactory>();

  return true;
}

bool AArch64GNUInfoLDBackend::initStubFactory() {
  if (nullptr == m_pStubFactory)
    m_pStubFactory =
        make<StubFactory>(make<AArch64FarcallStub>(config().options().isPIE()));
  if (nullptr == m_pErrata843419Factory)
    m_pErrata843419Factory =
        make<AArch64ErrataFactory>(make<AArch64Errata843419Stub>());
  return true;
}

void AArch64GNUInfoLDBackend::initDynamicSections(ELFObjectFile &InputFile) {
  InputFile.setDynamicSections(
      *m_Module.createInternalSection(
          InputFile, LDFileFormat::Internal, ".got", llvm::ELF::SHT_PROGBITS,
          llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE, 8),
      *m_Module.createInternalSection(
          InputFile, LDFileFormat::Internal, ".got.plt",
          llvm::ELF::SHT_PROGBITS, llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE,
          8),
      *m_Module.createInternalSection(
          InputFile, LDFileFormat::Internal, ".plt", llvm::ELF::SHT_PROGBITS,
          llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR, 16),
      *m_Module.createInternalSection(
          InputFile, LDFileFormat::DynamicRelocation, ".rela.dyn",
          llvm::ELF::SHT_RELA, llvm::ELF::SHF_ALLOC, 8),
      *m_Module.createInternalSection(
          InputFile, LDFileFormat::DynamicRelocation, ".rela.plt",
          llvm::ELF::SHT_RELA, llvm::ELF::SHF_ALLOC, 8));
}

void AArch64GNUInfoLDBackend::initTargetSections(ObjectBuilder &pBuilder) {

  createGNUPropertySection(false);
}

void AArch64GNUInfoLDBackend::initTargetSymbols() {
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
}

bool AArch64GNUInfoLDBackend::initRelocator() {
  if (nullptr == m_pRelocator) {
    m_pRelocator = make<AArch64Relocator>(*this, config(), m_Module);
  }
  return true;
}

bool AArch64GNUInfoLDBackend::processInputFiles(
    std::vector<InputFile *> &Inputs) {
  if (!m_pGPF)
    return config().getDiagEngine()->diagnose();
  for (auto &I : Inputs) {
    ELFObjectFile *ObjFile = llvm::dyn_cast<ELFObjectFile>(I);
    if (!ObjFile)
      continue;
    if (!ObjFile->getSize())
      continue;
    processInputFile(I);
  }
  return config().getDiagEngine()->diagnose();
}

bool AArch64GNUInfoLDBackend::processInputFile(InputFile *In) {
  // Create features
  uint32_t features = 0;
  bool hasWarning = false;
  auto Iter = NoteGNUPropertyMap.find(In);
  if (Iter != NoteGNUPropertyMap.end())
    features = Iter->second;
  if (config().options().hasForceBTI() &&
      !(features & llvm::ELF::GNU_PROPERTY_AARCH64_FEATURE_1_BTI)) {
    features |= llvm::ELF::GNU_PROPERTY_AARCH64_FEATURE_1_BTI;
    m_pGPF->updateInfo(features);
    NoteGNUPropertyMap[In] = features;
    config().raise(Diag::no_feature_found_in_file)
        << "BTI features recorded" << In->getInput()->decoratedPath();
    hasWarning = true;
  }
  if (config().options().hasForcePACPLT() &&
      !(features & llvm::ELF::GNU_PROPERTY_AARCH64_FEATURE_1_PAC)) {
    features |= llvm::ELF::GNU_PROPERTY_AARCH64_FEATURE_1_PAC;
    m_pGPF->updateInfo(features);
    NoteGNUPropertyMap[In] = features;
    config().raise(Diag::no_feature_found_in_file)
        << "PAC features recorded" << In->getInput()->decoratedPath();
    hasWarning = true;
  }
  static bool hasBTIFlag = true;
  static bool hasPACFlag = true;
  // reset BTI feature if BTI flag is not seen
  if ((!(features & llvm::ELF::GNU_PROPERTY_AARCH64_FEATURE_1_BTI)) ||
      (!hasBTIFlag)) {
    hasBTIFlag = false;
    m_pGPF->resetFlag(llvm::ELF::GNU_PROPERTY_AARCH64_FEATURE_1_BTI);
  }
  // reset PAC feature if PAC flag is not seen
  if ((!(features & llvm::ELF::GNU_PROPERTY_AARCH64_FEATURE_1_PAC)) ||
      (!hasPACFlag)) {
    hasPACFlag = false;
    m_pGPF->resetFlag(llvm::ELF::GNU_PROPERTY_AARCH64_FEATURE_1_PAC);
  }
  return !hasWarning;
}

Relocator *AArch64GNUInfoLDBackend::getRelocator() const {
  assert(nullptr != m_pRelocator);
  return m_pRelocator;
}

Relocation::Type AArch64GNUInfoLDBackend::getCopyRelType() const {
  return llvm::ELF::R_AARCH64_COPY;
}

void AArch64GNUInfoLDBackend::defineGOTSymbol(Fragment &pFrag) {
  // define symbol _GLOBAL_OFFSET_TABLE_
  auto SymbolName = "_GLOBAL_OFFSET_TABLE_";
  if (m_pGOTSymbol != nullptr) {
    m_pGOTSymbol =
        m_Module.getIRBuilder()
            ->addSymbol<IRBuilder::Force, IRBuilder::Unresolve>(
                pFrag.getOwningSection()->getInputFile(), SymbolName,
                ResolveInfo::Object, ResolveInfo::Define, ResolveInfo::Local,
                0x0, // size
                0x0, // value
                make<FragmentRef>(pFrag, 0x0), ResolveInfo::Hidden);
  } else {
    m_pGOTSymbol =
        m_Module.getIRBuilder()
            ->addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
                m_Module.getInternalInput(Module::Script), SymbolName,
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

bool AArch64GNUInfoLDBackend::finalizeScanRelocations() {
  Fragment *frag = nullptr;
  if (auto *GOTPLT = getGOTPLT())
    if (GOTPLT->hasSectionData())
      frag = *GOTPLT->getFragmentList().begin();
  if (frag)
    defineGOTSymbol(*frag);
  return true;
}

void AArch64GNUInfoLDBackend::doPreLayout() {
  // initialize .dynamic data
  if ((!config().isCodeStatic() || config().options().forceDynamic()) &&
      nullptr == m_pDynamic)
    m_pDynamic = make<AArch64ELFDynamic>(*this, config());

  if (LinkerConfig::Object != config().codeGenType()) {
    getRelaPLT()->setSize(getRelaPLT()->getRelocations().size() *
                          getRelaEntrySize());
    getRelaDyn()->setSize(getRelaDyn()->getRelocations().size() *
                          getRelaEntrySize());
    m_Module.addOutputSection(getRelaPLT());
    m_Module.addOutputSection(getRelaDyn());
  }
  m_ptdata = m_Module.getScript().sectionMap().find(".tdata");
  m_ptbss = m_Module.getScript().sectionMap().find(".tbss");
}

void AArch64GNUInfoLDBackend::initSegmentFromLinkerScript(
    ELFSegment *pSegment) {
  auto sect = pSegment->begin(), sectEnd = pSegment->end();
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

      if (cur->isNoBits())
        continue;

      // Convert to PROBIT
      cur->setType(llvm::ELF::SHT_PROGBITS);
      cur->setKind(LDFileFormat::Regular);
      config().raise(Diag::warn_mix_bss_section)
          << lastMixedNonBSSSection->name() << cur->name();
    }
  }
}

AArch64ELFDynamic *AArch64GNUInfoLDBackend::dynamic() { return m_pDynamic; }

unsigned int AArch64GNUInfoLDBackend::getTargetSectionOrder(
    const ELFSection &pSectHdr) const {
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

  return SHO_UNDEFINED;
}

void AArch64GNUInfoLDBackend::mayBeRelax(int pass, bool &pFinished) {
  if (config().options().noTrampolines()) {
    pFinished = true;
    return;
  }

  assert(nullptr != getStubFactory() && nullptr != getBRIslandFactory());
  ELFFileFormat *file_format = getOutputFormat();
  pFinished = true;

  if (config().options().fixCortexA53Erratum843419() && pass == 0) {
    if (scanErrata843419())
      pFinished = false;
  }

  // check branch relocs and create the related stubs if needed
  Module::obj_iterator input, inEnd = m_Module.objEnd();
  for (input = m_Module.objBegin(); input != inEnd; ++input) {
    ELFObjectFile *ObjFile = llvm::dyn_cast<ELFObjectFile>(*input);
    if (!ObjFile)
      continue;
    for (auto &rs : ObjFile->getRelocationSections()) {
      if (rs->isIgnore())
        continue;
      if (rs->isDiscard())
        continue;
      for (auto &reloc : rs->getLink()->getRelocations()) {
        switch (reloc->type()) {
        case llvm::ELF::R_AARCH64_CALL26:
        case llvm::ELF::R_AARCH64_JUMP26: {
          Relocation *relocation = llvm::cast<Relocation>(reloc);
          if (!relocation->symInfo())
            break;
          if (relocation->symInfo()->isUndef())
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
              symtab.setSize(symtab.size() + sizeof(llvm::ELF::Elf64_Sym));
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

// Return whether this is a 3-insn erratum sequence.
bool AArch64GNUInfoLDBackend::isErratum843419Sequence(uint32_t insn1,
                                                      uint32_t insn2,
                                                      uint32_t insn3) {
  unsigned rt1, rt2;
  bool load, pair;

  // The 2nd insn is a single register load or store; or register pair
  // store.
  if (AArch64InsnHelpers::mem_op_p(insn2, &rt1, &rt2, &pair, &load) &&
      (!pair || (pair && !load))) {
    // The 3rd insn is a load or store instruction from the "Load/store
    // register (unsigned immediate)" encoding class, using Rn as the
    // base address register.
    if (AArch64InsnHelpers::ldst_uimm(insn3) &&
        (AArch64InsnHelpers::rn(insn3) == AArch64InsnHelpers::rd(insn1)))
      return true;
  }
  return false;
}

bool AArch64GNUInfoLDBackend::scanErrata843419() {
  LinkerScript &script = m_Module.getScript();
  SectionMap::iterator out, outBegin = script.sectionMap().begin(),
                            outEnd = script.sectionMap().end();
  out = outBegin;
  bool updated = false;
  for (; out != outEnd; ++out) {
    OutputSectionEntry::iterator in, inBegin, inEnd;
    inBegin = (*out)->begin();
    inEnd = (*out)->end();
    for (in = inBegin; in != inEnd; ++in) {
      ELFSection *section = (*in)->getSection();
      if (section->size()) {
        llvm::SmallVectorImpl<Fragment *>::iterator
            it = section->getFragmentList().begin(),
            ie = section->getFragmentList().end();
        while (it != ie) {
          RegionFragment *frag = llvm::dyn_cast<RegionFragment>(*it);
          if (!frag) {
            ++it;
            continue;
          }
          if (!frag->getOwningSection()->isCode()) {
            ++it;
            continue;
          }
          if ((frag->getOffset(config().getDiagEngine()) +
               AArch64InsnHelpers::InsnSize * 3) > frag->size()) {
            ++it;
            continue;
          }
          uint64_t vma = frag->getOutputELFSection()->addr() +
                         frag->getOffset(config().getDiagEngine());
          uint32_t offset = 0;
          while ((offset + 3 * AArch64InsnHelpers::InsnSize) <= frag->size()) {
            unsigned page_offset = ((vma + offset) & 0xFFF);
            if ((page_offset != 0xFF8) && (page_offset != 0xFFC)) {
              offset += AArch64InsnHelpers::InsnSize;
              continue;
            }
            uint32_t insns[4];
            const char *data = frag->getRegion().data() + offset;
            std::memcpy(insns, data, 3 * AArch64InsnHelpers::InsnSize);
            AArch64InsnHelpers::InsnType insn1 = insns[0];
            unsigned insn_offset = 0;
            if (AArch64InsnHelpers::is_adrp(insns[0])) {
              AArch64InsnHelpers::InsnType insn2 = insns[1];
              AArch64InsnHelpers::InsnType insn3 = insns[2];
              bool do_report = false;
              if (isErratum843419Sequence(insn1, insn2, insn3)) {
                do_report = true;
                insn_offset = offset + (2 * AArch64InsnHelpers::InsnSize);
              } else if ((offset + 4 * AArch64InsnHelpers::InsnSize) <=
                         frag->size()) {
                // Optionally we can have an insn between ins2 and ins3
                // And insn_opt must not be a branch.
                if (!AArch64InsnHelpers::b(insns[2]) &&
                    !AArch64InsnHelpers::bl(insns[2]) &&
                    !AArch64InsnHelpers::blr(insns[2]) &&
                    !AArch64InsnHelpers::br(insns[2])) {
                  // And insn_opt must not write to dest reg in insn1. However
                  // we do a conservative scan, which means we may fix/report
                  // more than necessary, but it doesn't hurt.

                  std::memcpy(insns, data, 4 * AArch64InsnHelpers::InsnSize);
                  AArch64InsnHelpers::InsnType insn4 = insns[3];
                  if (isErratum843419Sequence(insn1, insn2, insn4)) {
                    do_report = true;
                    insn_offset = offset + (3 * AArch64InsnHelpers::InsnSize);
                  }
                }
              }
              if (do_report) {
                createErratum843419Stub(frag, insn_offset);
                // Now since a new fragment has been inserted, the fragment
                // needs to be reset.
                ie = section->getFragmentList().end();
                it = frag->getIterator();
                updated = true;
              }
            }
            offset += AArch64InsnHelpers::InsnSize;
          } // for each fragment.
          ++it;
        } // for each section
      }
    } // for each output section
  }

  return updated;
}

void AArch64GNUInfoLDBackend::createErratum843419Stub(Fragment *frag,
                                                      uint32_t offset) {
  BranchIsland *branchIsland = m_pErrata843419Factory->create(
      frag, offset, *m_Module.getIRBuilder(), *m_pAArch64ErrataIslandFactory);
  switch (config().options().getStripSymbolMode()) {
  case GeneralOptions::StripAllSymbols:
  case GeneralOptions::StripLocals:
    break;
  default: {
    ELFSection &symtab = *(getOutputFormat()->getSymTab());
    ELFSection &strtab = *(getOutputFormat()->getStrTab());
    symtab.setSize(symtab.size() + (sizeof(llvm::ELF::Elf64_Sym) * 2));
    symtab.setInfo(symtab.getInfo() + 2);
    strtab.setSize(strtab.size() + (branchIsland->symInfo()->nameSize() * 2) +
                   2);
  } break;
  }

  // Adjust any relocation pointing to that location to the copied location
  InputFile *input = frag->getOwningSection()->getInputFile();
  ELFObjectFile *ObjFile = llvm::dyn_cast<ELFObjectFile>(input);
  if (!ObjFile)
    return;
  for (auto &rs : ObjFile->getRelocationSections()) {
    if (rs->isIgnore())
      continue;
    if (rs->isDiscard())
      continue;

    if (rs->getLink() != frag->getOwningSection())
      continue;

    for (auto &relocation : rs->getLink()->getRelocations()) {
      // bypass the reloc if the symbol is in the discarded input section
      ResolveInfo *info = relocation->symInfo();

      if (!info->outSymbol()->hasFragRef() &&
          ResolveInfo::Section == info->type() &&
          ResolveInfo::Undefined == info->desc())
        continue;

      ELFSection *target_sect =
          relocation->targetRef()->frag()->getOwningSection();
      // bypass the reloc if the section where it sits will be discarded.
      if (target_sect->isDiscard() || target_sect->isIgnore())
        continue;

      // one location only has one relocation
      if (relocation->targetRef()->frag() == frag &&
          relocation->targetRef()->offset() == offset) {
        relocation->targetRef()->setFragment(branchIsland->stub());
        relocation->targetRef()->setOffset(0);
        break;
      }

    } // for all relocations
  } // for all relocation section
}

void AArch64GNUInfoLDBackend::defineIRelativeRange(ResolveInfo &pSym) {
  // It is up to linker script to define those symbols.
  if (m_Module.getScript().linkerScriptHasSectionsCommand())
    return;

  // Define the copy symbol in the bss section and resolve it
  auto SymbolName = "__rela_iplt_start";
  if (!m_pIRelativeStart && !m_pIRelativeEnd) {
    m_pIRelativeStart =
        m_Module.getIRBuilder()
            ->addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
                m_Module.getInternalInput(Module::Script), SymbolName,
                ResolveInfo::Object, ResolveInfo::Define,
                (ResolveInfo::Binding)pSym.binding(),
                0,   // size
                0x0, // value
                FragmentRef::null(), (ResolveInfo::Visibility)pSym.other());
    if (m_Module.getConfig().options().isSymbolTracingRequested() &&
        m_Module.getConfig().options().traceSymbol(SymbolName)) {
      config().raise(Diag::target_specific_symbol) << SymbolName;
    }
    m_pIRelativeStart->setShouldIgnore(false);
    SymbolName = "__rela_iplt_end";
    m_pIRelativeEnd =
        m_Module.getIRBuilder()
            ->addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
                m_Module.getInternalInput(Module::Script), SymbolName,
                ResolveInfo::Object, ResolveInfo::Define,
                (ResolveInfo::Binding)pSym.binding(),
                pSym.size(), // size
                0x0,         // value
                FragmentRef::null(), (ResolveInfo::Visibility)pSym.other());
    if (m_Module.getConfig().options().isSymbolTracingRequested() &&
        m_Module.getConfig().options().traceSymbol(SymbolName)) {
      config().raise(Diag::target_specific_symbol) << SymbolName;
    }
    m_pIRelativeEnd->setShouldIgnore(false);
  }
}

bool AArch64GNUInfoLDBackend::finalizeTargetSymbols() {
  if (m_pIRelativeStart && m_pIRelativeEnd) {
    m_pIRelativeStart->setValue(
        getRelaPLT()->getOutputSection()->getSection()->addr());
    m_pIRelativeEnd->setValue(
        getRelaPLT()->getOutputSection()->getSection()->addr() +
        getRelaPLT()->getOutputSection()->getSection()->size());
  }
  return true;
}

void AArch64GNUInfoLDBackend::setOptions() {
  bool linkerScriptHasSectionsCommand =
      m_Module.getScript().linkerScriptHasSectionsCommand();
  // If we are not using linker scripts, lets start by setting that
  // we need ehframe header.
  if (!linkerScriptHasSectionsCommand) {
    config().options().setEhFrameHdr(true);
    return;
  }
  // If size of headers is specified in the linker script, and since we load the
  // program headers lets set ehframehdr may be needed.
  if (m_Module.getScript().hasSizeOfHeader())
    config().options().setEhFrameHdr(true);
  return;
}

bool AArch64GNUInfoLDBackend::ltoNeedAssembler() {
  if (!config().options().getSaveTemps())
    return false;
  return true;
}

bool AArch64GNUInfoLDBackend::ltoCallExternalAssembler(
    const std::string &Input, std::string RelocModel,
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
  assemblerArgs.push_back("aarch64--linux-gnu");
  assemblerArgs.push_back("-filetype");
  assemblerArgs.push_back("obj");
  assemblerArgs.push_back("-target-cpu");
  assemblerArgs.push_back("generic");
  assemblerArgs.push_back("-mrelax-all");

  if (!RelocModel.empty()) {
    assemblerArgs.push_back("-mrelocation-model");
    assemblerArgs.push_back(RelocModel.c_str());
  }

  // Do target feature
  std::vector<llvm::StringRef> featureStrings;
  if (config().options().codegenOpts()) {
    for (auto ai : config().options().codeGenOpts()) {
      if (ai.compare(0, 7, "-mattr=") != 0)
        continue;

      llvm::StringRef feature = Saver.save(ai.substr(7));
      featureStrings.push_back(feature);
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

  return !llvm::sys::ExecuteAndWait(assemblerPath->c_str(), assemblerArgs);
}

// Create GOT entry.
AArch64GOT *AArch64GNUInfoLDBackend::createGOT(GOT::GOTType T,
                                               ELFObjectFile *Obj,
                                               ResolveInfo *R,
                                               bool SkipPLTRef) {

  if (R != nullptr && ((config().options().isSymbolTracingRequested() &&
                        config().options().traceSymbol(*R)) ||
                       m_Module.getPrinter()->traceDynamicLinking()))
    config().raise(Diag::create_got_entry) << R->name();
  // If we are creating a GOT, always create a .got.plt.
  if (!getGOTPLT()->getFragmentList().size()) {
    // TODO: This should be GOT0, not GOTPLT0.
    LDSymbol *Dynamic = m_Module.getNamePool().findSymbol("_DYNAMIC");
    AArch64GOTPLT0::Create(getGOTPLT(),
                           Dynamic ? Dynamic->resolveInfo() : nullptr);
  }

  AArch64GOT *G = nullptr;
  bool GOT = true;
  switch (T) {
  case GOT::Regular:
    G = AArch64GOT::Create(Obj->getGOT(), R);
    break;
  case GOT::GOTPLT0:
    G = llvm::dyn_cast<AArch64GOT>(*getGOTPLT()->getFragmentList().begin());
    GOT = false;
    break;
  case GOT::GOTPLTN: {
    // If the symbol is IRELATIVE, the PLT slot contains the relative symbol
    // value. No need to fill the GOT slot with PLT0.
    // TODO: PLT0 seems to get created even with -znow.
    Fragment *PLT0 =
        SkipPLTRef ? nullptr : *getPLT()->getFragmentList().begin();
    G = AArch64GOTPLTN::Create(Obj->getGOTPLT(), R, PLT0);
    GOT = false;
    break;
  }
  // It seems, there are no global TLS GOT slots on aarch64.
  case GOT::TLS_DESC:
    G = AArch64TLSDESCGOT::Create(Obj->getGOTPLT(), R);
    break;
  case GOT::TLS_IE:
    G = AArch64IEGOT::Create(Obj->getGOT(), R);
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
void AArch64GNUInfoLDBackend::recordGOT(ResolveInfo *I, AArch64GOT *G) {
  m_GOTMap[I] = G;
}

// Record GOTPLT entry.
void AArch64GNUInfoLDBackend::recordGOTPLT(ResolveInfo *I, AArch64GOT *G) {
  m_GOTPLTMap[I] = G;
}

// Find an entry in the GOT.
AArch64GOT *AArch64GNUInfoLDBackend::findEntryInGOT(ResolveInfo *I) const {
  auto Entry = m_GOTMap.find(I);
  if (Entry == m_GOTMap.end())
    return nullptr;
  return Entry->second;
}

// Create PLT entry.
AArch64PLT *AArch64GNUInfoLDBackend::createPLT(ELFObjectFile *Obj,
                                               ResolveInfo *R,
                                               bool isIRelative) {
  // If there is no entries GOTPLT and PLT, we dont have a PLT0.
  if (R != nullptr && ((config().options().isSymbolTracingRequested() &&
                        config().options().traceSymbol(*R)) ||
                       m_Module.getPrinter()->traceDynamicLinking()))
    config().raise(Diag::create_plt_entry) << R->name();
  if (!getPLT()->getFragmentList().size()) {
    AArch64PLT0::Create(*m_Module.getIRBuilder(),
                        createGOT(GOT::GOTPLT0, nullptr, nullptr), getPLT(),
                        nullptr);
  }
  AArch64PLT *P = AArch64PLTN::Create(
      *m_Module.getIRBuilder(), createGOT(GOT::GOTPLTN, Obj, R, isIRelative),
      Obj->getPLT(), R);
  // init the corresponding rel entry in .rela.plt
  Relocation &rela_entry = *Obj->getRelaPLT()->createOneReloc();
  rela_entry.setType(isIRelative ? llvm::ELF::R_AARCH64_IRELATIVE
                                 : llvm::ELF::R_AARCH64_JUMP_SLOT);
  rela_entry.setTargetRef(make<FragmentRef>(*P->getGOT(), 0));
  if (isIRelative)
    P->getGOT()->setValueType(GOT::SymbolValue);
  rela_entry.setSymInfo(R);
  if (R)
    recordPLT(R, P);
  return P;
}

// Record GOT entry.
void AArch64GNUInfoLDBackend::recordPLT(ResolveInfo *I, AArch64PLT *P) {
  m_PLTMap[I] = P;
}

Stub *AArch64GNUInfoLDBackend::getBranchIslandStub(Relocation *pReloc,
                                                   int64_t targetValue) const {
  (void)pReloc;
  (void)targetValue;
  return *getStubFactory()->getAllStubs().cbegin();
}

// Find an entry in the GOT.
AArch64PLT *AArch64GNUInfoLDBackend::findEntryInPLT(ResolveInfo *I) const {
  auto Entry = m_PLTMap.find(I);
  if (Entry == m_PLTMap.end())
    return nullptr;
  return Entry->second;
}

void AArch64GNUInfoLDBackend::createGNUPropertySection(bool force) {
  if (!config().options().hasForceBTI() &&
      !config().options().hasForcePACPLT() && !force)
    return;
  if (m_pNoteGNUProperty)
    return;
  m_pNoteGNUProperty = m_Module.createInternalSection(
      Module::InternalInputType::Sections, LDFileFormat::Internal,
      ".note.gnu.property", llvm::ELF::SHT_NOTE, llvm::ELF::SHF_ALLOC, 1);
  m_pGPF = eld::make<AArch64NoteGNUPropertyFragment>(m_pNoteGNUProperty);
  m_pNoteGNUProperty->addFragmentAndUpdateSize(m_pGPF);
  m_pNoteGNUProperty->setWanted(true);
}

bool AArch64GNUInfoLDBackend::readSection(InputFile &pInput, ELFSection *S) {
  // We need break them down to individual entry
  if (S->getKind() == LDFileFormat::GNUProperty) {
    // Force create GNU property section
    createGNUPropertySection(true);
    S->setWanted(true);
    uint32_t featureSet = 0;
    if (!readGNUProperty<llvm::object::ELF64LE>(pInput, S, featureSet))
      return false;
    NoteGNUPropertyMap[&pInput] = featureSet;
    if (!m_pGPF->updateInfo(featureSet))
      return false;
    return true;
  }
  return GNULDBackend::readSection(pInput, S);
}

bool AArch64GNUInfoLDBackend::DoesOverrideMerge(ELFSection *pSection) const {
  if (pSection->getKind() == LDFileFormat::Internal)
    return false;
  if (pSection->name() == ".note.gnu.property")
    return true;
  return false;
}

ELFSection *AArch64GNUInfoLDBackend::mergeSection(ELFSection *S) {
  if (S->name() == ".note.gnu.property")
    return m_pNoteGNUProperty;
  return nullptr;
}

// Read .note.gnu.property and extract features for pointer authentication.
template <class ELFT>
bool AArch64GNUInfoLDBackend::readGNUProperty(InputFile &pInput, ELFSection *S,
                                              uint32_t &featureSet) {
  using Elf_Nhdr = typename ELFT::Nhdr;
  using Elf_Note = typename ELFT::Note;

  llvm::StringRef Contents = pInput.getSlice(S->offset(), S->size());
  llvm::ArrayRef<uint8_t> data =
      llvm::ArrayRef((const uint8_t *)Contents.data(), Contents.size());
  auto reportFatal = [&](const char *msg) {
    config().raise(Diag::gnu_property_read_error)
        << pInput.getInput()->decoratedPath() << msg;
  };
  while (!data.empty()) {
    // Read one NOTE record.
    auto *nhdr = reinterpret_cast<const Elf_Nhdr *>(data.data());
    if (data.size() < sizeof(Elf_Nhdr) ||
        data.size() < nhdr->getSize(S->getAddrAlign())) {
      reportFatal("data is too short");
      return false;
    }

    Elf_Note note(*nhdr);
    if (nhdr->n_type != llvm::ELF::NT_GNU_PROPERTY_TYPE_0 ||
        note.getName() != "GNU") {
      data = data.slice(nhdr->getSize(S->getAddrAlign()));
      continue;
    }

    uint32_t featureAndType = llvm::ELF::GNU_PROPERTY_AARCH64_FEATURE_1_AND;

    // Read a body of a NOTE record, which consists of type-length-value fields.
    ArrayRef<uint8_t> desc = note.getDesc(S->getAddrAlign());
    while (!desc.empty()) {
      if (desc.size() < 8) {
        reportFatal("program property is too short");
        return false;
      }
      uint32_t type =
          llvm::support::endian::read32<ELFT::Endianness>(desc.data());
      uint32_t size =
          llvm::support::endian::read32<ELFT::Endianness>(desc.data() + 4);
      desc = desc.slice(8);
      if (desc.size() < size) {
        reportFatal("program property is too short");
        return false;
      }

      if (type == featureAndType) {
        // We found a FEATURE_1_AND field. There may be more than one of these
        // in a .note.gnu.property section, for a relocatable object we
        // accumulate the bits set.
        if (size < 4) {
          reportFatal("FEATURE_1_AND entry is too short");
          return false;
        }
        featureSet |=
            llvm::support::endian::read32<ELFT::Endianness>(desc.data());
      }

      // Padding is present in the note descriptor, if necessary.
      desc = desc.slice(alignTo<(ELFT::Is64Bits ? 8 : 4)>(size));
    }
    // Go to next NOTE record to look for more FEATURE_1_AND descriptions.
    data = data.slice(nhdr->getSize(S->getAddrAlign()));
  }
  return true;
}

void AArch64GNUInfoLDBackend::initializeAttributes() {
  getInfo().initializeAttributes(m_Module.getIRBuilder()->getInputBuilder());
}

namespace eld {

//===----------------------------------------------------------------------===//
//  createAArch64LDBackend - the help function to create corresponding
//  AArch64LDBackend
//===----------------------------------------------------------------------===//
GNULDBackend *createAArch64LDBackend(Module &pModule) {
  if (pModule.getConfig().targets().triple().isOSLinux())
    return make<AArch64GNUInfoLDBackend>(
        pModule, make<AArch64LinuxInfo>(pModule.getConfig()));
  return make<AArch64GNUInfoLDBackend>(pModule,
                                       make<AArch64Info>(pModule.getConfig()));
}

} // namespace eld

//===----------------------------------------------------------------------===//
// Force static initialization.
//===----------------------------------------------------------------------===//
extern "C" void ELDInitializeAArch64LDBackend() {
  // Register the linker backend
  eld::TargetRegistry::RegisterGNULDBackend(TheAArch64Target,
                                            createAArch64LDBackend);
}

namespace eld {
template bool AArch64GNUInfoLDBackend::readGNUProperty<llvm::object::ELF64LE>(
    InputFile &pInput, ELFSection *S, uint32_t &featureSet);
}
