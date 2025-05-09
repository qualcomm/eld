//===- HexagonLDBackend.cpp------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "HexagonLDBackend.h"
#include "Hexagon.h"
#include "HexagonAbsoluteStub.h"
#include "HexagonELFDynamic.h"
#include "HexagonLinuxInfo.h"
#include "HexagonRelocator.h"
#include "HexagonStandaloneInfo.h"
#include "HexagonTLSStub.h"
#include "eld/BranchIsland/BranchIslandFactory.h"
#include "eld/BranchIsland/StubFactory.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Fragment/FillFragment.h"
#include "eld/Fragment/FragUtils.h"
#include "eld/Fragment/RegionFragment.h"
#include "eld/Fragment/RegionFragmentEx.h"
#include "eld/Fragment/Stub.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/LayoutMap/LayoutInfo.h"
#include "eld/Object/ObjectBuilder.h"
#include "eld/Object/ObjectLinker.h"
#include "eld/Readers/CommonELFSection.h"
#include "eld/Support/DynamicLibrary.h"
#include "eld/Support/HashUtils.h"
#include "eld/Support/Memory.h"
#include "eld/Support/MemoryArea.h"
#include "eld/Support/MsgHandling.h"
#include "eld/Support/RegisterTimer.h"
#include "eld/Support/TargetRegistry.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/Target/ELFFileFormat.h"
#include "eld/Target/ELFSegment.h"
#include "eld/Target/ELFSegmentFactory.h"
#include "llvm/ADT/Hashing.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/Memory.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Parallel.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/ThreadPool.h"
#include "llvm/TargetParser/Triple.h"
#include <cstring>
#include <string.h>
#include <string>

using namespace eld;
using namespace llvm;

namespace {
// It should only be used for emitting diagnostics in
// HexagonLDBackend::allocateMemoryBlock function.
static DiagnosticEngine *sDiagEngine = nullptr;
class PrepareStaticDiagEngine {
public:
  PrepareStaticDiagEngine(DiagnosticEngine *diagEngine) {
    m_Mutex.lock();
    sDiagEngine = diagEngine;
  }
  ~PrepareStaticDiagEngine() {
    sDiagEngine = nullptr;
    m_Mutex.unlock();
  }

private:
  std::mutex m_Mutex;
};

} // namespace

//===----------------------------------------------------------------------===//
// HexagonLDBackend
//===----------------------------------------------------------------------===//
HexagonLDBackend::HexagonLDBackend(eld::Module &pModule, HexagonInfo *pInfo)
    : GNULDBackend(pModule, pInfo), m_pRelocator(nullptr), m_pDynamic(nullptr),
      m_psdata(nullptr), m_pscommon_1(nullptr), m_pscommon_2(nullptr),
      m_pscommon_4(nullptr), m_pscommon_8(nullptr), m_pstart(nullptr),
      m_pguard(nullptr), m_psdabase(nullptr), AttributeSection(nullptr),
      AttributeFragment(nullptr), m_pTLSBASE(nullptr), m_pTDATAEND(nullptr),
      m_pTLSEND(nullptr), m_scommon_1_hash(0), m_scommon_2_hash(0),
      m_scommon_4_hash(0), m_scommon_8_hash(0), m_common_hash(0),
      m_pMSGBase(nullptr), m_pEndOfImage(nullptr) {
  m_scommon_1_hash = llvm::hash_combine(std::string(".scommon.1"));
  m_scommon_2_hash = llvm::hash_combine(std::string(".scommon.2"));
  m_scommon_4_hash = llvm::hash_combine(std::string(".scommon.4"));
  m_scommon_8_hash = llvm::hash_combine(std::string(".scommon.8"));
  m_common_hash = llvm::hash_combine(std::string("COMMON"));

  // Validate Arch options.
  config().options().setValidateArchOptions();
}

bool HexagonLDBackend::initRelocator() {
  if (nullptr == m_pRelocator) {
    m_pRelocator = make<HexagonRelocator>(*this, config(), m_Module);
  }
  return true;
}

Relocator *HexagonLDBackend::getRelocator() const {
  assert(nullptr != m_pRelocator);
  return m_pRelocator;
}

Relocation::Type HexagonLDBackend::getCopyRelType() const {
  return llvm::ELF::R_HEX_COPY;
}

bool HexagonLDBackend::finalizeScanRelocations() {
  Fragment *frag = nullptr;
  if (auto *GOTPLT = getGOTPLT())
    if (GOTPLT->hasSectionData())
      frag = *GOTPLT->getFragmentList().begin();
  if (frag)
    defineGOTSymbol(*frag);
  return true;
}

void HexagonLDBackend::doPreLayout() {
  if (config().isCodeStatic() && !config().options().forceDynamic())
    return;

  if (getRelaPLT()) {
    getRelaPLT()->setSize(getRelaPLT()->getRelocations().size() *
                          getRelaEntrySize());
    m_Module.addOutputSection(getRelaPLT());
  }
  if (getRelaDyn()) {
    getRelaDyn()->setSize(getRelaDyn()->getRelocations().size() *
                          getRelaEntrySize());
    m_Module.addOutputSection(getRelaDyn());
  }
}

void HexagonLDBackend::doPostLayout() {
  m_psdata = m_Module.getScript().sectionMap().find(".sdata");
  GNULDBackend::doPostLayout();
}

eld::Expected<void>
HexagonLDBackend::postProcessing(llvm::FileOutputBuffer &pOutput) {
  eld::Expected<void> expBasePostProcess =
      GNULDBackend::postProcessing(pOutput);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expBasePostProcess);
  return {};
}

/// dynamic - the dynamic section of the target machine.
/// Use co-variant return type to return its own dynamic section.
HexagonELFDynamic *HexagonLDBackend::dynamic() { return m_pDynamic; }

void HexagonLDBackend::defineGOTSymbol(Fragment &pFrag) {
  // define symbol _GLOBAL_OFFSET_TABLE_
  if (m_pGOTSymbol != nullptr) {
    m_pGOTSymbol = m_Module.getIRBuilder()
                       ->addSymbol<IRBuilder::Force, IRBuilder::Unresolve>(
                           pFrag.getOwningSection()->getInputFile(),
                           "_GLOBAL_OFFSET_TABLE_", ResolveInfo::Object,
                           ResolveInfo::Define, ResolveInfo::Local,
                           0x0, // size
                           0x0, // value
                           make<FragmentRef>(pFrag, 0x0), ResolveInfo::Hidden);
  } else {
    m_pGOTSymbol = m_Module.getIRBuilder()
                       ->addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
                           pFrag.getOwningSection()->getInputFile(),
                           "_GLOBAL_OFFSET_TABLE_", ResolveInfo::Object,
                           ResolveInfo::Define, ResolveInfo::Local,
                           0x0, // size
                           0x0, // value
                           make<FragmentRef>(pFrag, 0x0), ResolveInfo::Hidden);
  }
  m_pGOTSymbol->setShouldIgnore(false);
}
std::optional<bool>
HexagonLDBackend::shouldProcessSectionForGC(const ELFSection &S) const {
  if (S.getType() == llvm::ELF::SHT_HEXAGON_ATTRIBUTES)
    return false;
  return {};
}

unsigned int
HexagonLDBackend::getTargetSectionOrder(const ELFSection &pSectHdr) const {
  if (m_Module.getScript().linkerScriptHasSectionsCommand())
    return SHO_UNDEFINED;

  if (LinkerConfig::Object != config().codeGenType()) {
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
  }

  if (pSectHdr.name() == ".sdata")
    return SHO_SMALL_DATA;

  return SHO_UNDEFINED;
}

void HexagonLDBackend::initDynamicSections(ELFObjectFile &InputFile) {
  InputFile.setDynamicSections(
      *m_Module.createInternalSection(
          InputFile, LDFileFormat::Internal, ".got", llvm::ELF::SHT_PROGBITS,
          llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE, 4),
      *m_Module.createInternalSection(
          InputFile, LDFileFormat::Internal, ".got.plt",
          llvm::ELF::SHT_PROGBITS, llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE,
          8),
      *m_Module.createInternalSection(
          InputFile, LDFileFormat::Internal, ".plt", llvm::ELF::SHT_PROGBITS,
          llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR, 16),
      *m_Module.createInternalSection(
          InputFile, LDFileFormat::DynamicRelocation, ".rela.dyn",
          llvm::ELF::SHT_RELA, llvm::ELF::SHF_ALLOC, 4),
      *m_Module.createInternalSection(
          InputFile, LDFileFormat::DynamicRelocation, ".rela.plt",
          llvm::ELF::SHT_RELA, llvm::ELF::SHF_ALLOC, 4));
}

void HexagonLDBackend::createAttributeSection() {
  if (AttributeSection)
    return;
  AttributeSection = m_Module.createInternalSection(
      Module::InternalInputType::Attributes, LDFileFormat::Target,
      ".hexagon.attributes", llvm::ELF::SHT_HEXAGON_ATTRIBUTES, 0, 1);
  AttributeFragment = make<HexagonAttributeFragment>(AttributeSection);
  AttributeSection->addFragment(AttributeFragment);
  if (auto *layoutInfo = getModule().getLayoutInfo())
    layoutInfo->recordFragment(AttributeSection->getInputFile(), AttributeSection,
                            AttributeFragment);
}

void HexagonLDBackend::initTargetSections(ObjectBuilder &pBuilder) {
  m_pguard = m_Module.createInternalSection(
      Module::InternalInputType::Guard, LDFileFormat::Internal, ".text.guard",
      llvm::ELF::SHT_PROGBITS, llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR,
      4);

  bool linkerScriptHasSectionsCommand =
      (m_Module.getScript().linkerScriptHasSectionsCommand());

  if ((!config().isCodeStatic()) || (config().options().forceDynamic())) {
    if (nullptr == m_pDynamic)
      m_pDynamic = make<HexagonELFDynamic>(*this, config());
  }

  for (int i = HexagonTLSStub::GD; i <= HexagonTLSStub::LDtoLE; ++i) {
    std::string stubName =
        HexagonTLSStub::stubName((HexagonTLSStub::StubType)i).str();
    m_TLSStubs[stubName] = m_Module.createInternalSection(
        Module::InternalInputType::TLSStub, LDFileFormat::Internal,
        ".text." + stubName, llvm::ELF::SHT_PROGBITS,
        llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR, 4);
  }

  if (linkerScriptHasSectionsCommand)
    return;

  m_psdata = m_Module.createInternalSection(
      Module::InternalInputType::SmallData, LDFileFormat::Internal, ".sdata",
      llvm::ELF::SHT_PROGBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE | llvm::ELF::SHF_HEX_GPREL,
      ((LinkerConfig::Object == config().codeGenType()) ? 0 : 4 * 1024));

  m_pscommon_1 = m_Module.createInternalSection(
      Module::InternalInputType::SmallData, LDFileFormat::Internal,
      ".scommon.1", llvm::ELF::SHT_NOBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE, 1);

  m_pscommon_2 = m_Module.createInternalSection(
      Module::InternalInputType::SmallData, LDFileFormat::Internal,
      ".scommon.2", llvm::ELF::SHT_NOBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE, 2);

  m_pscommon_4 = m_Module.createInternalSection(
      Module::InternalInputType::SmallData, LDFileFormat::Internal,
      ".scommon.4", llvm::ELF::SHT_NOBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE, 4);

  m_pscommon_8 = m_Module.createInternalSection(
      Module::InternalInputType::SmallData, LDFileFormat::Internal,
      ".scommon.8", llvm::ELF::SHT_NOBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE, 8);

  m_pstart = m_Module.createInternalSection(
      Module::InternalInputType::Guard, LDFileFormat::Internal, ".start",
      llvm::ELF::SHT_PROGBITS, llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE, 8);
}

void HexagonLDBackend::initTargetSymbols() {
  if (config().codeGenType() == LinkerConfig::Object)
    return;
  auto SymbolName = "_GLOBAL_OFFSET_TABLE_";
  // Define the symbol _GLOBAL_OFFSET_TABLE_ if there is a symbol with the
  // same name in input
  m_pGOTSymbol =
      m_Module.getIRBuilder()
          ->addSymbol<IRBuilder::AsReferred, IRBuilder::Resolve>(
              m_Module.getInternalInput(Module::Script), SymbolName,
              ResolveInfo::Object, ResolveInfo::Define, ResolveInfo::Local,
              0x0, // size
              0x0, // value
              FragmentRef::null(), ResolveInfo::Hidden);
  if (m_pGOTSymbol)
    m_pGOTSymbol->setShouldIgnore(false);
  if (m_Module.getConfig().options().isSymbolTracingRequested() &&
      m_Module.getConfig().options().traceSymbol(SymbolName))
    config().raise(Diag::target_specific_symbol) << SymbolName;
  SymbolName = "__end";
  m_pEndOfImage = m_Module.getNamePool().findSymbol(SymbolName);
  if (!m_pEndOfImage)
    m_pEndOfImage =
        m_Module.getIRBuilder()
            ->addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
                m_Module.getInternalInput(Module::Script), SymbolName,
                ResolveInfo::NoType, ResolveInfo::Define, ResolveInfo::Absolute,
                0x0, // size
                0x0, // value
                FragmentRef::null());
  if (m_pEndOfImage)
    m_pEndOfImage->setShouldIgnore(false);
  if (m_Module.getConfig().options().isSymbolTracingRequested() &&
      m_Module.getConfig().options().traceSymbol(SymbolName))
    config().raise(Diag::target_specific_symbol) << SymbolName;
  // If linker script, lets not add this symbol.
  if (m_Module.getScript().linkerScriptHasSectionsCommand()) {
    m_pMSGBase = m_Module.getNamePool().findSymbol("_MSG_BASE_");
    m_psdabase = m_Module.getNamePool().findSymbol("_SDA_BASE_");
    return;
  }
  SymbolName = "_SDA_BASE_";
  m_psdabase =
      m_Module.getIRBuilder()
          ->addSymbol<IRBuilder::AsReferred, IRBuilder::Resolve>(
              m_Module.getInternalInput(Module::Script), SymbolName,
              ResolveInfo::Object, ResolveInfo::Define, ResolveInfo::Absolute,
              0x0, // size
              0x0, // value
              FragmentRef::null(), ResolveInfo::Hidden);
  if (m_psdabase)
    m_psdabase->setShouldIgnore(false);
  if (m_Module.getConfig().options().isSymbolTracingRequested() &&
      m_Module.getConfig().options().traceSymbol(SymbolName))
    config().raise(Diag::target_specific_symbol) << SymbolName;

  SymbolName = "__sbss_start";
  LDSymbol *sbssStart =
      m_Module.getIRBuilder()
          ->addSymbol<IRBuilder::AsReferred, IRBuilder::Resolve>(
              m_Module.getInternalInput(Module::Script), SymbolName,
              ResolveInfo::Object, ResolveInfo::Define, ResolveInfo::Absolute,
              0x0, // size
              0x0, // value
              FragmentRef::null(), ResolveInfo::Hidden);
  if (sbssStart)
    sbssStart->setShouldIgnore(false);
  if (m_Module.getConfig().options().isSymbolTracingRequested() &&
      m_Module.getConfig().options().traceSymbol(SymbolName))
    config().raise(Diag::target_specific_symbol) << SymbolName;
  SymbolName = "__sbss_end";
  LDSymbol *sbssEnd =
      m_Module.getIRBuilder()
          ->addSymbol<IRBuilder::AsReferred, IRBuilder::Resolve>(
              m_Module.getInternalInput(Module::Script), "__sbss_end",
              ResolveInfo::Object, ResolveInfo::Define, ResolveInfo::Absolute,
              0x0, // size
              0x0, // value
              FragmentRef::null(), ResolveInfo::Hidden);
  if (sbssEnd)
    sbssEnd->setShouldIgnore(false);
  if (m_Module.getConfig().options().isSymbolTracingRequested() &&
      m_Module.getConfig().options().traceSymbol(SymbolName))
    config().raise(Diag::target_specific_symbol) << SymbolName;

  // OSABI for linux and standalone is Sys V - UNIX. Need to see triple
  // for standalone verification
  if (!config().targets().triple().isOSLinux()) {
    SymbolName = "_TLS_START_";
    m_pTLSBASE =
        m_Module.getIRBuilder()
            ->addSymbol<IRBuilder::AsReferred, IRBuilder::Resolve>(
                m_Module.getInternalInput(Module::Script), SymbolName,
                ResolveInfo::Object, ResolveInfo::Define, ResolveInfo::Absolute,
                0x0, // size
                0x0, // value
                FragmentRef::null(), ResolveInfo::Hidden);
    if (m_pTLSBASE)
      m_pTLSBASE->setShouldIgnore(false);
    if (m_Module.getConfig().options().isSymbolTracingRequested() &&
        m_Module.getConfig().options().traceSymbol(SymbolName))
      config().raise(Diag::target_specific_symbol) << SymbolName;
    SymbolName = "_TLS_DATA_END_";
    m_pTDATAEND =
        m_Module.getIRBuilder()
            ->addSymbol<IRBuilder::AsReferred, IRBuilder::Resolve>(
                m_Module.getInternalInput(Module::Script), SymbolName,
                ResolveInfo::Object, ResolveInfo::Define, ResolveInfo::Absolute,
                0x0, // size
                0x0, // value
                FragmentRef::null(), ResolveInfo::Hidden);
    if (m_pTDATAEND)
      m_pTDATAEND->setShouldIgnore(false);
    if (m_Module.getConfig().options().isSymbolTracingRequested() &&
        m_Module.getConfig().options().traceSymbol(SymbolName))
      config().raise(Diag::target_specific_symbol) << SymbolName;

    SymbolName = "_TLS_END_";
    m_pTLSEND =
        m_Module.getIRBuilder()
            ->addSymbol<IRBuilder::AsReferred, IRBuilder::Resolve>(
                m_Module.getInternalInput(Module::Script), SymbolName,
                ResolveInfo::Object, ResolveInfo::Define, ResolveInfo::Absolute,
                0x0, // size
                0x0, // value
                FragmentRef::null(), ResolveInfo::Hidden);
    if (m_pTLSEND)
      m_pTLSEND->setShouldIgnore(false);
    if (m_Module.getConfig().options().isSymbolTracingRequested() &&
        m_Module.getConfig().options().traceSymbol(SymbolName))
      config().raise(Diag::target_specific_symbol) << SymbolName;
  }

  if (LinkerConfig::DynObj == config().codeGenType())
    return;
  SymbolName = "_MSG_BASE_";
  m_pMSGBase =
      m_Module.getIRBuilder()->addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
          m_Module.getInternalInput(Module::Script), SymbolName,
          ResolveInfo::NoType, ResolveInfo::Define, ResolveInfo::Absolute,
          0x0, // size
          0x0, // value
          FragmentRef::null());
  if (m_pMSGBase)
    m_pMSGBase->setShouldIgnore(false);
  if (m_Module.getConfig().options().isSymbolTracingRequested() &&
      m_Module.getConfig().options().traceSymbol(SymbolName))
    config().raise(Diag::target_specific_symbol) << SymbolName;
}

bool HexagonLDBackend::initTargetStubs() { return true; }

bool HexagonLDBackend::initBRIslandFactory() {
  if (nullptr == m_pBRIslandFactory) {
    m_pBRIslandFactory = make<BranchIslandFactory>(true, config());
  }
  return true;
}

bool HexagonLDBackend::initStubFactory() {
  if (nullptr == m_pStubFactory)
    m_pStubFactory = make<StubFactory>(make<HexagonAbsoluteStub>(
        config().codeGenType() == LinkerConfig::DynObj));
  return true;
}

bool HexagonLDBackend::haslinkerRelaxed(
    const std::vector<RegionFragmentEx *> &FragsForRelaxation) {
  bool isFinished = true;
  for (auto &F : FragsForRelaxation) {
    for (auto &reloc : F->getOwningSection()->getRelocations()) {
      // Addend needs to be 0.
      if (reloc->addend() != 0)
        continue;
      // If the fragment cannot be relaxed, dont relax
      if (!canFragmentBeRelaxed(F))
        continue;
      // If the relocation points to a fragment whose alignment is more than 4
      // we may need to skip relaxation
      LDSymbol *sym = reloc->symInfo()->outSymbol();
      if (!sym || !sym->hasFragRefSection())
        continue;
      if (sym->fragRef()->frag()->getOwningSection()->getAddrAlign() > 4)
        continue;
      switch (reloc->type()) {
      case llvm::ELF::R_HEX_B22_PCREL:
        // If the last instruction in the section jumps to the
        // next instruction, then we dont need this instruction.
        if (reloc->getOffset() == F->size() - 4 &&
            getRelocator()->getSymValue(reloc) == reloc->place(m_Module) + 4) {
          reloc->setType(llvm::ELF::R_HEX_NONE);
          m_RelaxedRelocs.insert(reloc);
          F->deleteInstruction(reloc->getOffset(), 4);
          // We are not done.
          isFinished = false;
          if (m_Module.getPrinter()->isVerbose())
            config().raise(Diag::deleting_instructions)
                << "B22_PCREL" << 4 << reloc->symInfo()->name()
                << F->getOwningSection()->name()
                << llvm::utohexstr(reloc->getOffset(), true)
                << F->getOwningSection()
                       ->getInputFile()
                       ->getInput()
                       ->decoratedPath();
        }
      }
    }
  }
  return !isFinished;
}

void HexagonLDBackend::mayBeRelax(int, bool &pFinished) {
  if (config().options().noTrampolines()) {
    pFinished = true;
    return;
  }
  assert(nullptr != getStubFactory() && nullptr != getBRIslandFactory());
  ELFFileFormat *file_format = getOutputFormat();
  pFinished = true;
  std::vector<OutputSectionEntry *> OutSections;
  std::vector<RegionFragmentEx *> FragsForRelaxation;
  bool isRelaxationEnabled = config().options().isLinkerRelaxationEnabled();

  for (auto &O : OutputSectionToFrags) {
    if (O.first && !O.first->getSection()->isCode())
      continue;
    for (auto &F : O.second) {
      if (!F->getOwningSection()->isCode())
        continue;
      if (isRelaxationEnabled) {
        if (auto RelaxFrag = llvm::dyn_cast<eld::RegionFragmentEx>(F))
          FragsForRelaxation.push_back(RelaxFrag);
      }
    }
    OutSections.push_back(O.first);
  }

  // Insert trampolines
  auto InsertTrampolinesForOutputSection = [&](size_t n) {
    OutputSectionEntry *Out = OutSections.at(n);
    std::vector<Fragment *> Frags;
    for (auto &F : OutputSectionToFrags[Out]) {
      if (F->getOwningSection()->isCode()) {
        Frags.push_back(F);
      }
    }
    eld::RegisterTimer T(Out->name(), "Trampoline Time",
                         m_Module.getConfig().options().printTimingStats());
    for (auto &F : Frags) {
      for (auto &reloc : F->getOwningSection()->getRelocations()) {
        switch (reloc->type()) {
        case llvm::ELF::R_HEX_B22_PCREL:
        case llvm::ELF::R_HEX_PLT_B22_PCREL:
        case llvm::ELF::R_HEX_GD_PLT_B22_PCREL:
        case llvm::ELF::R_HEX_LD_PLT_B22_PCREL:
        case llvm::ELF::R_HEX_B15_PCREL:
        case llvm::ELF::R_HEX_B13_PCREL:
        case llvm::ELF::R_HEX_B9_PCREL: {
          Relocation *relocation = llvm::cast<Relocation>(reloc);
          if (!relocation->symInfo())
            break;
          // Dont process undefined symbols that dont have a PLT entry.
          if (reloc->symInfo()->isUndef() && !reloc->symInfo()->isWeakUndef() &&
              !(reloc->symInfo()->reserved() & Relocator::ReservePLT))
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
              std::lock_guard<std::mutex> Guard(Mutex);
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
          if (!config().getDiagEngine()->diagnose()) {
            if (m_Module.getPrinter()->isVerbose())
              config().raise(Diag::function_has_error) << __PRETTY_FUNCTION__;
            pFinished = true;
          }
        } break;

        default:
          break;
        }
      }
    }
  };

  if (config().options().numThreads() <= 1 ||
      !config().isLinkerRelaxationMultiThreaded()) {
    for (size_t I = 0; I < OutSections.size(); ++I)
      InsertTrampolinesForOutputSection(I);
  } else {
    llvm::parallelFor((size_t)0, OutSections.size(),
                      InsertTrampolinesForOutputSection);
  }
  if (pFinished) {
    // TODO : Multithread this code.
    if (isRelaxationEnabled && haslinkerRelaxed(FragsForRelaxation))
      pFinished = false;
  }
}

/// finalizeSymbol - finalize the symbol value
bool HexagonLDBackend::finalizeTargetSymbols() {
  if (config().codeGenType() == LinkerConfig::Object)
    return true;

  // Get the pointer to the real end of the image.
  if (m_pEndOfImage && !m_pEndOfImage->scriptDefined()) {
    uint64_t imageEnd = 0;
    for (auto &seg : elfSegmentTable()) {
      if (seg->type() != llvm::ELF::PT_LOAD)
        continue;
      uint64_t segSz = seg->paddr() + seg->memsz();
      if (imageEnd < segSz)
        imageEnd = segSz;
    }
    alignAddress(imageEnd, 8);
    m_pEndOfImage->setValue(imageEnd + 1);
  }

  if (m_Module.getScript().linkerScriptHasSectionsCommand())
    return true;

  if (m_psdabase) {
    if (m_psdata)
      m_psdabase->setValue(m_psdata->addr());
  }
  if (m_pMSGBase) {
    ELFSection *rodata = m_Module.getSection(".rodata");
    if (rodata) {
      Fragment *F = rodata->getFirstFragmentInRule();
      if (F) {
        m_pMSGBase->setFragmentRef(make<FragmentRef>(*F, 0x0));
        m_pMSGBase->setValue(rodata->addr());
      }
    }
  }
  ELFSegment *pt_tls =
      elfSegmentTable().find(llvm::ELF::PT_TLS, llvm::ELF::PF_R, 0);
  if (pt_tls) {
    uint64_t memsz = pt_tls->memsz();
    alignAddress(memsz, 8ull);
    pt_tls->setMemsz(memsz);
    uint64_t ptTLSAlign = pt_tls->align();
    alignAddress(ptTLSAlign, 8ull);
    pt_tls->setAlign(ptTLSAlign);
    if (m_pTLSBASE)
      m_pTLSBASE->setValue(pt_tls->vaddr());
    if (m_pTDATAEND)
      m_pTDATAEND->setValue(pt_tls->filesz() + pt_tls->vaddr());
    if (m_pTLSEND)
      m_pTLSEND->setValue(pt_tls->memsz() + pt_tls->vaddr());
    // Update the TLS template size as well.
    setTLSTemplateSize(pt_tls->memsz());
  }
  return true;
}

bool HexagonLDBackend::finalizeLayout() {
  if (config().codeGenType() == LinkerConfig::Object)
    return true;
  LDSymbol *LWChecksum =
      m_Module.getNamePool().findSymbol("__lw_image_layout_checksum");
  if (LWChecksum) {
    uint32_t Checksum = m_Module.getImageLayoutChecksum() & 0xFFFFFFFF;
    RegionFragment *R =
        llvm::dyn_cast<RegionFragment>(LWChecksum->fragRef()->frag());
    if (R)
      R->setContent<uint32_t>(Checksum);
  }
  return true;
}

uint64_t
HexagonLDBackend::getValueForDiscardedRelocations(const Relocation *R) const {
  if (!m_pEndOfImage)
    return GNULDBackend::getValueForDiscardedRelocations(R);
  return m_pEndOfImage->value();
}

bool HexagonLDBackend::DoesOverrideMerge(ELFSection *pInputSection) const {
  // FIXME: Should we also add pInputSection->kind() == LDFileFormat::Common,
  // here?
  if (pInputSection->getKind() == LDFileFormat::Internal)
    return false;
  return (!m_Module.getScript().linkerScriptHasSectionsCommand() &&
          LinkerConfig::Object != config().codeGenType() &&
          ((pInputSection->getFlags() & llvm::ELF::SHF_HEX_GPREL) ||
           (pInputSection->getKind() == LDFileFormat::LinkOnce)));
}

/// merge Input Sections
ELFSection *HexagonLDBackend::mergeSection(ELFSection *pInputSection) {
  MoveSectionAndSort(pInputSection, m_psdata);
  return m_psdata;
}

uint32_t HexagonLDBackend::getGP() {
  static bool warned = false;
  if (m_psdabase)
    return m_psdabase->value();
  if (!warned && m_Module.getScript().linkerScriptHasSectionsCommand()) {
    config().raise(Diag::sda_base_not_found);
    warned = true;
  }
  if (m_psdata == nullptr) {
    config().raise(Diag::small_data_not_found);
    m_Module.setFailure(true);
    // prevent warning
    return 1;
  } else {
    return m_psdata->addr();
  }
}

uint32_t HexagonLDBackend::getMsgBase() const {
  bool linkerScriptHasSectionsCommand =
      m_Module.getScript().linkerScriptHasSectionsCommand();
  if (linkerScriptHasSectionsCommand) {
    if (!m_pMSGBase) {
      config().raise(Diag::msg_base_not_found_linker_script);
      m_Module.setFailure(true);
      return 0;
    }
  } else if (m_pMSGBase && !m_pMSGBase->hasFragRef()) {
    config().raise(Diag::msg_base_not_found_no_linker_script);
    m_Module.setFailure(true);
    return 0;
  }
  return m_pMSGBase->value();
}

/// allocateCommonSymbols - allocate common symbols in the corresponding
/// sections. This is called at pre-layout stage.
bool HexagonLDBackend::allocateCommonSymbols() {
  if (!m_Module.getCommonSymbols().size())
    return true;

  eld::RegisterTimer T("Allocate Common Symbols",
                       "Hexagon Allocate Common Symbols",
                       m_Module.getConfig().options().printTimingStats());

  for (auto &common_sym : m_Module.getCommonSymbols()) {
    LDSymbol *com_sym = common_sym->outSymbol();
    std::string internalSectionName = computeInternalCommonSectionName(com_sym);
    // For common symbols, alignment = symbol value.
    ELFSection *S = m_Module.createCommonELFSection(
        internalSectionName, com_sym->value(), common_sym->resolvedOrigin());

    Fragment *frag = make<FillFragment>(getModule(), 0x0, (com_sym)->size(), S,
                                        /*alignment=*/com_sym->value());
    S->addFragmentAndUpdateSize(frag);
    (com_sym)->setFragmentRef(make<FragmentRef>(*frag, 0));
  }
  return true;
}

bool HexagonLDBackend::MoveSectionAndSort(ELFSection *pFrom, ELFSection *pTo) {
  ObjectBuilder builder(config(), m_Module);
  if (pFrom->getFragmentList().size() == 0)
    return true;

  if (builder.moveSection(pFrom, pTo)) {
    std::sort(pTo->getFragmentList().begin(), pTo->getFragmentList().end(),
              [](Fragment *A, Fragment *B) {
                return (A->alignment() < B->alignment());
              });
    pFrom->setMatchedLinkerScriptRule(pTo->getMatchedLinkerScriptRule());
    pFrom->setOutputSection(pTo->getOutputSection());
    builder.mayChangeSectionTypeOrKind(pTo->getOutputELFSection(), pFrom);
    builder.updateSectionFlags(pTo, pFrom);
  }
  // Clear.
  return true;
}

void HexagonLDBackend::mayWarnSection(ELFSection *sect) const {
  StringRef sectionName = sect->name();
  // If the section has some sort of flag, no need to worry.
  if (sect->getFlags() != 0)
    return;
  bool raiseWarn = llvm::StringSwitch<bool>(sectionName)
                       .StartsWith(".sdata", true)
                       .StartsWith(".gnu.linkonce", true)
                       .StartsWith(".tcm", true)
                       .Default(false);
  if (raiseWarn) {
    config().raise(Diag::section_does_not_have_proper_permissions)
        << sectionName << sect->getInputFile()->getInput()->decoratedPath();
    return;
  }
  GNULDBackend::mayWarnSection(sect);
}

int32_t HexagonLDBackend::getPacketOffset(const Relocation &pReloc) const {
  bool isPCREL = false;
  switch (pReloc.type()) {
  case llvm::ELF::R_HEX_32_PCREL:
  case llvm::ELF::R_HEX_6_PCREL_X:
  case llvm::ELF::R_HEX_B13_PCREL:
  case llvm::ELF::R_HEX_B13_PCREL_X:
  case llvm::ELF::R_HEX_B15_PCREL:
  case llvm::ELF::R_HEX_B15_PCREL_X:
  case llvm::ELF::R_HEX_B22_PCREL:
  case llvm::ELF::R_HEX_B22_PCREL_X:
  case llvm::ELF::R_HEX_B32_PCREL_X:
  case llvm::ELF::R_HEX_B7_PCREL:
  case llvm::ELF::R_HEX_B7_PCREL_X:
  case llvm::ELF::R_HEX_B9_PCREL:
  case llvm::ELF::R_HEX_B9_PCREL_X:
  case llvm::ELF::R_HEX_PLT_B22_PCREL:
    isPCREL = true;
    break;
  default:
    break;
  }

  if (!isPCREL)
    return 0;

  const Fragment *frag = pReloc.targetRef()->frag();
  llvm::StringRef RegionStr = getRegionFromFragment(frag);
  uint32_t offset = pReloc.targetRef()->offset();
  assert(offset < frag->size() && "Offset is greater than fragment size!");
  const char *start = RegionStr.data();
  const char *regionAtOffset = start + offset;
  uint32_t maxInstructionsToCheck = 0;

  while ((regionAtOffset != start) && (maxInstructionsToCheck < 4)) {
    uint32_t wordAtRegion;

    memcpy(
        &wordAtRegion,
        reinterpret_cast<const uint32_t *>(regionAtOffset - sizeof(uint32_t)),
        sizeof(wordAtRegion));
    if (((wordAtRegion & MASK_END_PACKET) == END_OF_PACKET) ||
        ((wordAtRegion & MASK_END_PACKET) == END_OF_DUPLEX))
      break;
    regionAtOffset -= sizeof(uint32_t);
  }
  return ((start + offset) - regionAtOffset);
}

bool HexagonLDBackend::ltoNeedAssembler() {
  if (config().options().ltoUseAssembler())
    return true;
  if (config().options().hasLTOAsmFile())
    return true;
  return false;
}

bool HexagonLDBackend::ltoCallExternalAssembler(const std::string &Input,
                                                std::string RelocModel,
                                                const std::string &Output) {
  bool traceLTO = config().options().traceLTO();
  // Invoke assembler.
  std::string assembler = "llvm-mc";

  llvm::ErrorOr<std::string> assemblerPath =
      llvm::sys::findProgramByName(assembler);
  if (!assemblerPath) {
    // Look for the assembler within the folder where the linker is
    std::string apath = config().options().linkerPath();
    apath += "/" + assembler;
    if (!llvm::sys::fs::exists(apath))
      llvm::report_fatal_error("Assembler not found!\n");
    else
      assemblerPath = apath;
  }

  std::string triple = "-triple=" + config().targets().triple().str();
  std::string cpu = "-mcpu=" + config().targets().getTargetCPU();

  std::vector<StringRef> assemblerArgs;
  assemblerArgs.push_back(assemblerPath->c_str());
  assemblerArgs.push_back(triple.c_str());
  assemblerArgs.push_back(cpu.c_str());
  assemblerArgs.push_back("-filetype=obj");

  // Do target feature
  assemblerArgs.push_back(Input.c_str());
  assemblerArgs.push_back("-o");
  assemblerArgs.push_back(Output.c_str());
  if (config().options().asmopts())
    for (auto i : config().options().asmOpts())
      assemblerArgs.push_back(i.c_str());

  if (traceLTO) {
    std::stringstream ss;
    for (auto s : assemblerArgs) {
      if (s.data())
        ss << s.data() << " ";
    }
    config().raise(Diag::process_launch) << ss.str();
  }

  int exec = llvm::sys::ExecuteAndWait(assemblerPath->c_str(), assemblerArgs);
  if (exec)
    return false;
  return true;
}

void HexagonLDBackend::AddLTOOptions(std::vector<std::string> &Options) {
  Options.push_back("-hexagon-small-data-threshold=" +
                    std::to_string(config().options().getGPSize()));
}

void HexagonLDBackend::initializeAttributes() {
  getInfo().initializeAttributes(m_Module.getIRBuilder()->getInputBuilder());
}

// Create GOT entry.
HexagonGOT *HexagonLDBackend::createGOT(GOT::GOTType T, ELFObjectFile *Obj,
                                        ResolveInfo *R) {

  if (R != nullptr && ((config().options().isSymbolTracingRequested() &&
                        config().options().traceSymbol(*R)) ||
                       m_Module.getPrinter()->traceDynamicLinking()))
    config().raise(Diag::create_got_entry) << R->name();
  // If we are creating a GOT, always create a .got.plt.
  if (!getGOTPLT()->getFragmentList().size()) {
    LDSymbol *Dynamic = m_Module.getNamePool().findSymbol("_DYNAMIC");
    HexagonGOTPLT0::Create(getGOTPLT(),
                           Dynamic ? Dynamic->resolveInfo() : nullptr);
  }

  HexagonGOT *G = nullptr;
  bool GOT = true;
  switch (T) {
  case GOT::Regular:
    G = HexagonGOT::Create(Obj->getGOT(), R);
    break;
  case GOT::GOTPLT0:
    G = llvm::dyn_cast<HexagonGOT>(*getGOTPLT()->getFragmentList().begin());
    GOT = false;
    break;
  case GOT::GOTPLTN: {
    // No PLT0 for immediate binding.
    Fragment *PLT0 = config().options().hasNow()
                         ? nullptr
                         : *getPLT()->getFragmentList().begin();
    G = HexagonGOTPLTN::Create(Obj->getGOTPLT(), R, PLT0);
    GOT = false;
    break;
  }
  case GOT::TLS_GD:
    G = HexagonGDGOT::Create(Obj->getGOT(), R);
    break;
  case GOT::TLS_LD:
    G = HexagonLDGOT::Create(getGOT(), R);
    break;
  case GOT::TLS_IE:
    G = HexagonIEGOT::Create(Obj->getGOT(), R);
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
void HexagonLDBackend::recordGOT(ResolveInfo *I, HexagonGOT *G) {
  m_GOTMap[I] = G;
}

// Record GOTPLT entry.
void HexagonLDBackend::recordGOTPLT(ResolveInfo *I, HexagonGOT *G) {
  m_GOTPLTMap[I] = G;
}

// Find an entry in the GOT.
HexagonGOT *HexagonLDBackend::findEntryInGOT(ResolveInfo *I) const {
  auto Entry = m_GOTMap.find(I);
  if (Entry == m_GOTMap.end())
    return nullptr;
  return Entry->second;
}

// Create PLT entry.
HexagonPLT *HexagonLDBackend::createPLT(ELFObjectFile *Obj, ResolveInfo *R) {
  bool hasNow = config().options().hasNow();
  if (R != nullptr && ((config().options().isSymbolTracingRequested() &&
                        config().options().traceSymbol(*R)) ||
                       m_Module.getPrinter()->traceDynamicLinking()))
    config().raise(Diag::create_plt_entry) << R->name();
  // If there is no entries GOTPLT and PLT, we dont have a PLT0.
  if (!hasNow && !getPLT()->getFragmentList().size()) {
    HexagonPLT0::Create(*m_Module.getIRBuilder(),
                        createGOT(GOT::GOTPLT0, nullptr, nullptr), getPLT(),
                        nullptr);
  }
  HexagonPLT *P =
      HexagonPLTN::Create(*m_Module.getIRBuilder(),
                          createGOT(GOT::GOTPLTN, Obj, R), Obj->getPLT(), R);
  // init the corresponding rel entry in .rela.plt
  Relocation &rela_entry = *Obj->getRelaPLT()->createOneReloc();
  rela_entry.setType(llvm::ELF::R_HEX_JMP_SLOT);
  Fragment *F = P->getGOT();
  rela_entry.setTargetRef(make<FragmentRef>(*F, 0));
  rela_entry.setSymInfo(R);
  if (R)
    recordPLT(R, P);
  return P;
}

// Record GOT entry.
void HexagonLDBackend::recordPLT(ResolveInfo *I, HexagonPLT *P) {
  m_PLTMap[I] = P;
}

// Find an entry in the GOT.
HexagonPLT *HexagonLDBackend::findEntryInPLT(ResolveInfo *I) const {
  auto Entry = m_PLTMap.find(I);
  if (Entry == m_PLTMap.end())
    return nullptr;
  return Entry->second;
}

// Create a stub for GD and LDToLE, GDToIE transformations.
HexagonTLSStub *HexagonLDBackend::createTLSStub(HexagonTLSStub::StubType T) {
  std::string stubName = HexagonTLSStub::stubName(T).str();
  HexagonTLSStub *S = findTLSStub(stubName);
  if (S)
    return S;
  switch (T) {
  case HexagonTLSStub::GD:
    S = HexagonGDStub::Create(m_Module, m_TLSStubs[stubName]);
    break;
  case HexagonTLSStub::GDtoIE:
    S = HexagonGDIEStub::Create(m_Module, m_TLSStubs[stubName]);
    break;
  case HexagonTLSStub::LDtoLE:
    S = HexagonLDLEStub::Create(m_Module, m_TLSStubs[stubName]);
    break;
  }
  recordTLSStub(stubName, S);
  return S;
}

void HexagonLDBackend::recordTLSStub(std::string stubName, HexagonTLSStub *S) {
  m_TLSStubMap[stubName] = S;
}

Stub *HexagonLDBackend::getBranchIslandStub(Relocation *pReloc,
                                            int64_t targetValue) const {
  (void)pReloc;
  (void)targetValue;
  return *(getStubFactory()->getAllStubs().cbegin());
}

HexagonTLSStub *HexagonLDBackend::findTLSStub(std::string stubName) {
  auto s = m_TLSStubMap.find(stubName);
  if (s != m_TLSStubMap.end())
    return s->getValue();
  return nullptr;
}

bool HexagonLDBackend::readSection(InputFile &pInput, ELFSection *S) {
  switch (S->getKind()) {
  case LDFileFormat::Target:
    if (S->getType() == llvm::ELF::SHT_HEXAGON_ATTRIBUTES) {
      if (!AttributeSection)
        createAttributeSection();
      AttributeFragment->update(*S, *getModule().getConfig().getDiagEngine(),
                                *llvm::dyn_cast<ObjectFile>(&pInput),
                                getModule().getLayoutInfo());
      return true;
    }
    break;
  default:
    break;
  }

  // Support Hexagon relaxation
  if (!canSectionBeRelaxed(pInput, S))
    return GNULDBackend::readSection(pInput, S);

  // Create a optimal fragment.
  eld::LayoutInfo *layoutInfo = m_Module.getLayoutInfo();
  const char *Buf = pInput.getCopyForWrite(S->offset(), S->size());
  eld::RegionFragmentEx *F =
      make<RegionFragmentEx>(Buf, S->size(), S, S->getAddrAlign());
  S->addFragment(F);
  if (layoutInfo)
    layoutInfo->recordFragment(&pInput, S, F);
  return true;
}

bool HexagonLDBackend::canSectionBeRelaxed(InputFile &pInput,
                                           ELFSection *S) const {
  if (!config().options().isLinkerRelaxationEnabled(S->name()))
    return false;

  // Check if section is code.
  if (!S->isCode())
    return false;

  // If the section size is less than a word
  if (S->size() < sizeof(uint32_t))
    return false;

  llvm::StringRef SectionContents = pInput.getSlice(S->offset(), S->size());
  uint32_t Word = 0;
  // Extract the last word in the section
  std::memcpy(
      &Word,
      (const char *)(SectionContents.data() + SectionContents.size() - 4),
      sizeof(Word));
  // if the last instruction in the section is a jump, we may be able to relax
  // this section by deleting the instruction.
  return (Word == HEXAGON_JUMP_INSTRUCTION);
}

bool HexagonLDBackend::canFragmentBeRelaxed(Fragment *F) const {
  RegionFragmentEx *R = llvm::dyn_cast<RegionFragmentEx>(F);
  if (!R || !R->size())
    return false;
  uint32_t Word = 0;
  // Extract the last word in the section
  std::memcpy(&Word, (const char *)(R->getRegion().data() + (R->size() - 4)),
              sizeof(Word));
  // if the last instruction in the section is a jump, we may be able to relax
  // this section by deleting the instruction.
  return (Word == HEXAGON_JUMP_INSTRUCTION);
}

bool HexagonLDBackend::addSymbols() {
  eld::ObjectLinker *ObjLinker = m_Module.getLinker()->getObjLinker();
  if (m_Module.needLTOToBeInvoked() && !ObjLinker->isPostLTOPhase())
    return true;
  if (!m_Module.getLinker()->getObjLinker()->provideGlobalSymbolAndContents(
          "__lw_image_layout_checksum", 4, 4))
    return false;
  return true;
}

bool HexagonLDBackend::isRelocationRelaxed(Relocation *R) const {
  return m_RelaxedRelocs.count(R);
}

//===----------------------------------------------------------------------===//
/// createHexagonLDBackend - the help function to create corresponding
/// HexagonLDBackend
GNULDBackend *createHexagonLDBackend(eld::Module &pModule) {
  if (pModule.getConfig().targets().triple().isOSLinux())
    return make<HexagonLDBackend>(pModule,
                                  make<HexagonLinuxInfo>(pModule.getConfig()));
  return make<HexagonLDBackend>(
      pModule, make<HexagonStandaloneInfo>(pModule.getConfig()));
}

std::string HexagonLDBackend::computeInternalCommonSectionName(
    const LDSymbol *comSym) const {
  int8_t maxGPSize = config().options().getGPSize();
  uint32_t shndx = comSym->sectionIndex();
  LDSymbol::SizeType comSymSize = comSym->size();

  switch (shndx) {
  case llvm::ELF::SHN_HEXAGON_SCOMMON_1:
    return std::string(".scommon.1.") + comSym->name();
  case llvm::ELF::SHN_HEXAGON_SCOMMON_2:
    return std::string(".scommon.2.") + comSym->name();
  case llvm::ELF::SHN_HEXAGON_SCOMMON_4:
    return std::string(".scommon.4.") + comSym->name();
  case llvm::ELF::SHN_HEXAGON_SCOMMON_8:
    return std::string(".scommon.8.") + comSym->name();
  default:
    if (comSymSize <= 1 && maxGPSize >= 1)
      return std::string(".scommon.1.") + comSym->name();
    else if (comSymSize <= 2 && maxGPSize >= 2)
      return std::string(".scommon.2.") + comSym->name();
    else if (comSymSize <= 4 && maxGPSize >= 4)
      return std::string(".scommon.4.") + comSym->name();
    else if (comSymSize <= 8 && maxGPSize >= 8)
      return std::string(".scommon.8.") + comSym->name();
    else
      return std::string("COMMON.") + comSym->name();
  }
}

//===----------------------------------------------------------------------===//
// Force static initialization.
//===----------------------------------------------------------------------===//
extern "C" void ELDInitializeHexagonLDBackend() {
  // Register the linker backend
  eld::TargetRegistry::RegisterGNULDBackend(TheHexagonTarget,
                                            createHexagonLDBackend);
}
