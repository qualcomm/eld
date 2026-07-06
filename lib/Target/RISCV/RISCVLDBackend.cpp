//===- RISCVLDBackend.cpp--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "RISCVLDBackend.h"
#include "RISCV.h"
#include "RISCVAttributeFragment.h"
#include "RISCVELFDynamic.h"
#include "RISCVGOT.h"
#include "RISCVLLVMExtern.h"
#include "RISCVPLT.h"
#include "RISCVRelaxationStats.h"
#include "RISCVRelocationHelper.h"
#include "RISCVRelocationInternal.h"
#include "RISCVRelocator.h"
#include "RISCVStandaloneInfo.h"
#include "RISCVTableJump.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Fragment/FillFragment.h"
#include "eld/Fragment/RegionFragment.h"
#include "eld/Fragment/RegionFragmentEx.h"
#include "eld/Fragment/Stub.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/Object/ObjectBuilder.h"
#include "eld/Object/ObjectLinker.h"
#include "eld/Support/Memory.h"
#include "eld/Support/MemoryArea.h"
#include "eld/Support/MsgHandling.h"
#include "eld/Support/TargetRegistry.h"
#include "eld/Support/Utils.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "eld/Target/ELFFileFormat.h"
#include "eld/Target/ELFSegmentFactory.h"
#include "eld/Target/GNULDBackend.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/Hashing.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/Casting.h"
#include <optional>
#include <string>

using namespace eld;
using namespace llvm;

//===----------------------------------------------------------------------===//
// RISCVLDBackend
//===----------------------------------------------------------------------===//
RISCVLDBackend::RISCVLDBackend(eld::Module &pModule, RISCVInfo *pInfo)
    : GNULDBackend(pModule, pInfo) {}

RISCVLDBackend::~RISCVLDBackend() {}

bool RISCVLDBackend::initRelocator() {
  if (nullptr == m_pRelocator)
    m_pRelocator = make<RISCVRelocator>(*this, config(), m_Module);
  return true;
}

Relocator *RISCVLDBackend::getRelocator() const {
  assert(nullptr != m_pRelocator);
  return m_pRelocator;
}

Relocation::Address RISCVLDBackend::getSymbolValuePLT(const Relocation &R) {
  ResolveInfo *rsym = R.symInfo();
  if (rsym && (rsym->reserved() & Relocator::ReservePLT)) {
    if (const Fragment *S = findEntryInPLT(rsym))
      return S->getAddr(config().getDiagEngine());
    if (const ResolveInfo *S = findAbsolutePLT(rsym))
      return S->value();
  }
  return getRelocator()->getSymValue(&R);
}

Relocation::Address RISCVLDBackend::getSymbolValuePLT(ResolveInfo &Sym) {
  if (Sym.reserved() & Relocator::ReservePLT) {
    if (const Fragment *S = findEntryInPLT(&Sym))
      return S->getAddr(config().getDiagEngine());
    if (const ResolveInfo *S = findAbsolutePLT(&Sym))
      return S->value();
  }

  if (const LDSymbol *Out = Sym.outSymbol())
    return Out->value();
  return Sym.value();
}

Relocation::Type RISCVLDBackend::getCopyRelType() const {
  return llvm::ELF::R_RISCV_COPY;
}

void RISCVLDBackend::initDynamicSections(ELFObjectFile &InputFile) {
  InputFile.setDynamicSections(
      *m_Module.createInternalSection(
          InputFile, LDFileFormat::Internal, ".got", llvm::ELF::SHT_PROGBITS,
          llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE,
          config().targets().is32Bits() ? 4 : 8),
      *m_Module.createInternalSection(
          InputFile, LDFileFormat::Internal, ".got.plt",
          llvm::ELF::SHT_PROGBITS, llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE,
          config().targets().is32Bits() ? 4 : 8),
      *m_Module.createInternalSection(
          InputFile, LDFileFormat::Internal, ".plt", llvm::ELF::SHT_PROGBITS,
          llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_EXECINSTR,
          config().targets().is32Bits() ? 4 : 16),
      *m_Module.createInternalSection(
          InputFile, LDFileFormat::DynamicRelocation, ".rela.dyn",
          llvm::ELF::SHT_RELA, llvm::ELF::SHF_ALLOC,
          config().targets().is32Bits() ? 4 : 8),
      *m_Module.createInternalSection(
          InputFile, LDFileFormat::DynamicRelocation, ".rela.plt",
          llvm::ELF::SHT_RELA, llvm::ELF::SHF_ALLOC,
          config().targets().is32Bits() ? 4 : 8));
}

void RISCVLDBackend::initTargetSections(ObjectBuilder &pBuilder) {
  m_pRISCVAttributeSection = m_Module.createInternalSection(
      Module::InternalInputType::Attributes, LDFileFormat::Internal,
      ".riscv.attributes", llvm::ELF::SHT_RISCV_ATTRIBUTES, 0, 1);
  AttributeFragment = make<RISCVAttributeFragment>(m_pRISCVAttributeSection);
  m_pRISCVAttributeSection->addFragment(AttributeFragment);
  LayoutInfo *layoutInfo = getModule().getLayoutInfo();
  if (layoutInfo)
    layoutInfo->recordFragment(m_pRISCVAttributeSection->getInputFile(),
                            m_pRISCVAttributeSection, AttributeFragment);
  if (LinkerConfig::Object == config().codeGenType())
    return;

  if (config().options().getRISCVRelaxTbljal()) {
    m_pRISCVTableJumpSection = m_Module.createInternalSection(
        Module::InternalInputType::TableJump, LDFileFormat::Internal,
        ".riscv.jvt", llvm::ELF::SHT_PROGBITS, llvm::ELF::SHF_ALLOC,
        /*Align=*/64);
    TableJumpFragment =
        make<RISCVTableJumpFragment>(*this, m_pRISCVTableJumpSection);
    m_pRISCVTableJumpSection->addFragmentAndUpdateSize(TableJumpFragment);
    if (layoutInfo)
      layoutInfo->recordFragment(m_pRISCVTableJumpSection->getInputFile(),
                                 m_pRISCVTableJumpSection, TableJumpFragment);
  }

  // Create .dynamic section
  if ((!config().isCodeStatic()) || (config().options().forceDynamic())) {
    if (nullptr == m_pDynamic)
      m_pDynamic = make<RISCVELFDynamic>(*this, config());
  }
}

void RISCVLDBackend::initPatchSections(ELFObjectFile &InputFile) {
  InputFile.setPatchSections(
      *m_Module.createInternalSection(
          InputFile, LDFileFormat::Internal, ".pgot", llvm::ELF::SHT_PROGBITS,
          llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE,
          config().targets().is32Bits() ? 4 : 8),
      *m_Module.createInternalSection(InputFile, LDFileFormat::Relocation,
                                      ".rela.pgot", llvm::ELF::SHT_RELA, 0,
                                      config().targets().is32Bits() ? 4 : 8));
}

void RISCVLDBackend::initTargetSymbols() {
  if (config().codeGenType() == LinkerConfig::Object)
    return;

  if (TableJumpFragment) {
    // The __jvt_base$ symbol contains the Zcmt jump table base address.
    std::string JvtName = "__jvt_base$";
    m_pJvtBase =
        m_Module.getIRBuilder()
            ->addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
                m_Module.getInternalInput(Module::InternalInputType::TableJump),
                JvtName, ResolveInfo::NoType, ResolveInfo::Define,
                ResolveInfo::Global,
                /*Size=*/0x0, /*Value=*/0x0,
                make<FragmentRef>(*TableJumpFragment, 0x0),
                ResolveInfo::Hidden);
    if (m_pJvtBase)
      m_pJvtBase->setShouldIgnore(false);
    if (m_Module.getConfig().options().isSymbolTracingRequested() &&
        m_Module.getConfig().options().traceSymbol(JvtName))
      config().raise(Diag::target_specific_symbol) << JvtName;

    // We put a data marker symbol at the start of the `.riscv.jvt` section to
    // mark it correctly. Use addSymbol directly (not
    // addLinkerInternalLocalSymbol) so addSymbolsToOutput() adds it to
    // Module::Symbols exactly once.
    std::string JvtMarkerName = "$d";
    m_Module.getIRBuilder()->addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
        m_Module.getInternalInput(Module::InternalInputType::TableJump),
        JvtMarkerName, ResolveInfo::NoType, ResolveInfo::Define,
        ResolveInfo::Local, /*Size=*/0, /*Value=*/0,
        make<FragmentRef>(*TableJumpFragment, 0x0), ResolveInfo::Default);
  }

  // Do not create another __global_pointer$ when linking a patch.
  if (config().options().getPatchBase())
    return;
  if (m_Module.getScript().linkerScriptHasSectionsCommand()) {
    m_pGlobalPointer = m_Module.getNamePool().findSymbol("__global_pointer$");
    return;
  }
  std::string SymbolName = "__global_pointer$";
  m_pGlobalPointer =
      m_Module.getIRBuilder()->addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
          m_Module.getInternalInput(Module::Script), SymbolName,
          ResolveInfo::Object, ResolveInfo::Define, ResolveInfo::Absolute,
          0x0, // size
          0x0, // value
          FragmentRef::null(), ResolveInfo::Hidden);
  if (m_pGlobalPointer)
    m_pGlobalPointer->setShouldIgnore(false);
  if (m_Module.getConfig().options().isSymbolTracingRequested() &&
      m_Module.getConfig().options().traceSymbol(SymbolName))
    config().raise(Diag::target_specific_symbol) << SymbolName;
}

void RISCVLDBackend::initTableJump() {
  if (TableJumpInitialized || !TableJumpFragment)
    return;

  // If the jump-table section is discarded by linker script rules, do not
  // generate entries and do not relax calls to table-jump instructions.
  if (m_pRISCVTableJumpSection && (m_pRISCVTableJumpSection->isIgnore() ||
                                   m_pRISCVTableJumpSection->isDiscard())) {
    m_pRISCVTableJumpSection->setSize(0);
    TableJumpInitialized = true;
    return;
  }

  llvm::DenseSet<const ELFSection *> Visited;
  for (auto &Input : m_Module.getObjectList()) {
    ELFObjectFile *ObjFile = llvm::dyn_cast<ELFObjectFile>(Input);
    if (!ObjFile)
      continue;
    for (auto &Rs : ObjFile->getRelocationSections()) {
      if (Rs->isIgnore() || Rs->isDiscard())
        continue;
      ELFSection *Linked = Rs->getLink();
      if (!Linked || !Linked->isCode())
        continue;
      if (!Visited.insert(Linked).second)
        continue;
      TableJumpFragment->scanTableJumpEntries(*Linked);
    }
  }

  TableJumpFragment->finalizeContents();
  // finalizeContents() computes the fragment size, which must be propagated to
  // the owning section for layout.
  if (m_pRISCVTableJumpSection)
    m_pRISCVTableJumpSection->setSize(TableJumpFragment->size());
  if (m_pJvtBase)
    m_pJvtBase->setSize(TableJumpFragment->size());
  TableJumpInitialized = true;
}

bool RISCVLDBackend::initBRIslandFactory() { return true; }

bool RISCVLDBackend::initStubFactory() { return true; }

bool RISCVLDBackend::readSection(InputFile &pInput, ELFSection *S) {
  eld::LayoutInfo *layoutInfo = m_Module.getLayoutInfo();
  if (S->isCode()) {
    const char *Buf = pInput.getCopyForWrite(S->offset(), S->size());
    eld::RegionFragmentEx *F =
        make<RegionFragmentEx>(Buf, S->size(), S, S->getAddrAlign());
    S->addFragment(F);
    if (layoutInfo)
      layoutInfo->recordFragment(&pInput, S, F);
    return true;
  }
  return GNULDBackend::readSection(pInput, S);
}

bool RISCVLDBackend::DoesOverrideMerge(ELFSection *pSection) const {
  if (pSection->getKind() == LDFileFormat::Internal)
    return false;
  if (pSection->getType() == llvm::ELF::SHT_RISCV_ATTRIBUTES)
    return true;
  return false;
}

ELFSection *RISCVLDBackend::mergeSection(ELFSection *S) {
  if (S->getType() == llvm::ELF::SHT_RISCV_ATTRIBUTES) {
    RegionFragment *R = llvm::dyn_cast<RegionFragment>(S->getFrontFragment());
    if (R)
      AttributeFragment->updateInfo(
          R->getRegion(), R->getOwningSection()->getInputFile(),
          config().getDiagEngine(), config().showAttributeMixWarnings());
    S->setKind(LDFileFormat::Discard);
    return m_pRISCVAttributeSection;
  }
  return nullptr;
}

void RISCVLDBackend::relaxDeleteBytes(StringRef Name, RegionFragmentEx &Region,
                                      uint64_t Offset, unsigned NumBytes,
                                      StringRef SymbolName) {
  auto &Section = *Region.getOwningSection();
  Region.deleteInstruction(Offset, NumBytes);
  if (m_Module.getPrinter()->isVerbose())
    config().raise(Diag::deleting_instructions)
        << Name << NumBytes << SymbolName << Section.name()
        << llvm::utohexstr(Offset, true)
        << Section.getInputFile()->getInput()->decoratedPath();
  recordRelaxationStats(Section, NumBytes, 0);
}

void RISCVLDBackend::reportMissedRelaxation(StringRef Name,
                                            RegionFragmentEx &Region,
                                            uint64_t Offset, unsigned NumBytes,
                                            StringRef SymbolName) {
  auto &Section = *Region.getOwningSection();
  if (m_Module.getPrinter()->isVerbose())
    config().raise(Diag::not_relaxed)
        << Name << NumBytes << SymbolName << Section.name()
        << llvm::utohexstr(Offset, true)
        << Section.getInputFile()->getInput()->decoratedPath();
  recordRelaxationStats(Section, 0, NumBytes);
}

// Select the matching JVT entry lookup for rd (x0 -> cm.jt, x1 -> cm.jalt).
// Returns the table entry index, or -1 when this relocation is not eligible.
static int
getTableJumpEntryIndex(const RISCVTableJumpFragment &TableJumpFragment,
                       const ResolveInfo *Sym, unsigned Rd) {
  if (Rd == 0)
    return TableJumpFragment.getCMJTEntryIndex(Sym);
  if (Rd == 1)
    return TableJumpFragment.getCMJALTEntryIndex(Sym);
  return -1;
}

static void applyTableJumpRelaxation(Relocation *Reloc,
                                     RegionFragmentEx &Region, uint64_t Offset,
                                     unsigned EntryIndex) {
  uint16_t TblJump = static_cast<uint16_t>(0xA002 | (EntryIndex << 2));
  Region.replaceInstruction(Offset, Reloc,
                            reinterpret_cast<uint8_t *>(&TblJump), 2);
  Reloc->setTargetData(TblJump);
  Reloc->setType(eld::ELF::riscv::internal::R_RISCV_TBJAL);
}

bool RISCVLDBackend::doRelaxationCall(Relocation *reloc) {
  /* Three similar relaxations can be applied here, in order of preference:
   * -- auipc;jalr -> c.j/c.jal (saves 6 bytes)
   * -- auipc;jalr -> jal (saves 4 bytes)
   * -- auipc;jalr -> qc.e.j/qc.e.jal (saves 2 bytes) (Xqci only)
   */

  Fragment *frag = reloc->targetRef()->frag();
  RegionFragmentEx *region = llvm::dyn_cast<RegionFragmentEx>(frag);
  if (!region)
    return true;

  Relocator::DWord S = getSymbolValuePLT(*reloc);
  Relocator::DWord A = reloc->addend();
  Relocator::DWord P = reloc->place(m_Module);
  Relocator::DWord X = S + A - P;

  // Be conservative when relaxing CALL into JAL/C.J/C.JAL: later relaxations
  // (notably R_RISCV_ALIGN) can increase the final PC-relative distance due
  // to changes in required alignment padding. Add a max alignment "slack" to
  // the offset for the range check so we only relax when the shortened form is
  // guaranteed to remain in-range.
  int64_t XForRangeCheck = static_cast<int64_t>(X);
  if (llvm::isInt<21>(XForRangeCheck)) {
    uint64_t MaxAlignment = 0;
    ELFSection *CallOutputSection = reloc->targetRef()->getOutputELFSection();
    FragmentRef *SymRef = reloc->symInfo()->outSymbol()->fragRef();
    ELFSection *SymOutputSection =
        SymRef ? SymRef->getOutputELFSection() : nullptr;

    if (CallOutputSection && SymOutputSection) {
      if (CallOutputSection == SymOutputSection)
        MaxAlignment = CallOutputSection->getAddrAlign();
      else
        MaxAlignment = std::max<uint64_t>(CallOutputSection->getAddrAlign(),
                                          SymOutputSection->getAddrAlign());
    }

    if (MaxAlignment)
      XForRangeCheck += (XForRangeCheck < 0)
                            ? -static_cast<int64_t>(MaxAlignment)
                            : static_cast<int64_t>(MaxAlignment);
  }

  uint64_t offset = reloc->targetRef()->offset();

  // extract the next instruction
  uint32_t jalr_instr;
  reloc->targetRef()->memcpy(&jalr_instr, sizeof(uint32_t), 4);

  // double check if the next instruction is jal
  if ((jalr_instr & 0x7F) != 0x67)
    return false;

  unsigned rd = (jalr_instr >> 7) & 0x1fu;

  // For `C.J`/`C.JAL`, we have to have C relaxations enabled,
  // and be within 12 bits, and have a destination of `x0` or `ra`
  bool canRelaxCJ = config().options().getRISCVRelax() &&
                    config().options().getRISCVRelaxToC() &&
                    isInt<12>(XForRangeCheck) &&
                    (rd == 0 || (rd == 1 && config().targets().is32Bits()));

  // For `JAL`, the offset needs to be 21 bits.
  bool canRelaxJal =
      config().options().getRISCVRelax() && llvm::isInt<21>(XForRangeCheck);

  // For `QC.E.J` and `QC.E.JAL`, we have to have xqci relaxations enabled,
  // and be on 32-bit, and the destination must be either `x0` or `ra`.
  bool canRelaxXqci = config().options().getRISCVRelax() &&
                      config().targets().is32Bits() &&
                      config().options().getRISCVRelaxXqci();
  bool canRelaxQcEJ = canRelaxXqci && (rd == 0 || rd == 1);

  const char *msgC = (rd == 1) ? "RISCV_CALL_JAL" : "RISCV_CALL_J";
  if (canRelaxCJ) {
    uint16_t c_j = (rd == 1) ? 0x2001u : 0xa001;

    if (m_Module.getPrinter()->isVerbose())
      config().raise(Diag::relax_to_compress)
          << msgC
          << llvm::utohexstr(reloc->target(), true, 8) + "," +
                 llvm::utohexstr(jalr_instr, true, 8)
          << llvm::utohexstr(c_j, true, 4) << reloc->symInfo()->name()
          << region->getOwningSection()->name() << llvm::utohexstr(offset)
          << region->getOwningSection()
                 ->getInputFile()
                 ->getInput()
                 ->decoratedPath();

    region->replaceInstruction(offset, reloc, reinterpret_cast<uint8_t *>(&c_j), 2);
    reloc->setTargetData(c_j);
    reloc->setType(llvm::ELF::R_RISCV_RVC_JUMP);
    relaxDeleteBytes("RISCV_CALL_C", *region, offset + 2, 6,
                     reloc->symInfo()->name());

    return true;
  }

  if (config().options().getRISCVRelaxTbljal() && TableJumpFragment &&
      TableJumpFragment->size() && m_pRISCVTableJumpSection &&
      !m_pRISCVTableJumpSection->isIgnore() &&
      !m_pRISCVTableJumpSection->isDiscard()) {
    int EntryIndex =
        getTableJumpEntryIndex(*TableJumpFragment, reloc->symInfo(), rd);
    // EntryIndex is the index in the JVT where the address is stored.
    // getTableJumpEntryIndex() returns a valid index [0,255] when it finds one
    // and -1 when the relocation is not applicable.
    // The JVT payload itself is written later from final symbol values, after
    // all relaxation passes have completed.
    if (EntryIndex >= 0) {
      applyTableJumpRelaxation(reloc, *region, offset,
                               static_cast<unsigned>(EntryIndex));
      relaxDeleteBytes("RISCV_TBJAL", *region, offset + 2, 6,
                       reloc->symInfo()->name());
      return true;
    }
  }

  if (canRelaxJal) {
    // Replace the instruction to JAL
    uint32_t jal = 0x6fu | rd << 7;

    region->replaceInstruction(offset, reloc, reinterpret_cast<uint8_t *>(&jal), 4);
    reloc->setTargetData(jal);
    reloc->setType(llvm::ELF::R_RISCV_JAL);
    // Delete the next instruction
    relaxDeleteBytes("RISCV_CALL", *region, offset + 4, 4,
                     reloc->symInfo()->name());

    // Report missed relaxation as we could still do a `C.J`/`C.JAL`
    reportMissedRelaxation("RISCV_CALL_C", *region, offset, 2,
                           reloc->symInfo()->name());

    return true;
  }

  if (canRelaxQcEJ) {
    uint64_t qc_e_j = (rd == 1) ? 0xc01ful : 0x401ful;
    const char *msg =
        (rd == 1) ? "R_RISCV_CALL_QC_E_JAL" : "R_RISCV_CALL_QC_E_J";

    region->replaceInstruction(offset, reloc, reinterpret_cast<uint8_t *>(&qc_e_j), 6);
    reloc->setTargetData(qc_e_j);
    reloc->setType(ELF::riscv::internal::R_RISCV_QC_E_CALL_PLT);
    relaxDeleteBytes(msg, *region, offset + 6, 2, reloc->symInfo()->name());

    reportMissedRelaxation(msg, *region, offset, 4, reloc->symInfo()->name());

    return true;
  }

  reportMissedRelaxation("RISCV_CALL", *region, offset, 6,
                         reloc->symInfo()->name());
  return false;
}

bool RISCVLDBackend::doRelaxationJal(Relocation *reloc) {
  if (!config().options().getRISCVRelaxTbljal() || !TableJumpFragment ||
      !TableJumpFragment->size() || !m_pRISCVTableJumpSection ||
      m_pRISCVTableJumpSection->isIgnore() ||
      m_pRISCVTableJumpSection->isDiscard())
    return false;

  Fragment *frag = reloc->targetRef()->frag();
  RegionFragmentEx *region = llvm::dyn_cast<RegionFragmentEx>(frag);
  if (!region)
    return false;

  uint64_t offset = reloc->targetRef()->offset();
  uint32_t Jal = 0;
  reloc->targetRef()->memcpy(&Jal, sizeof(Jal), 0);
  unsigned rd = (Jal >> 7) & 0x1fu;

  int EntryIndex =
      getTableJumpEntryIndex(*TableJumpFragment, reloc->symInfo(), rd);
  if (EntryIndex < 0)
    return false;

  applyTableJumpRelaxation(reloc, *region, offset,
                           static_cast<unsigned>(EntryIndex));
  relaxDeleteBytes("RISCV_TBJAL", *region, offset + 2, 2,
                   reloc->symInfo()->name());
  return true;
}

bool RISCVLDBackend::doRelaxationQCCall(Relocation *reloc) {
  // This function performs the relaxation to replace: QC.E.JAL or QC.E.J with
  // one of CM.JT/CM.JALT, JAL, C.J, or C.JAL.

  Fragment *frag = reloc->targetRef()->frag();
  RegionFragmentEx *region = llvm::dyn_cast<RegionFragmentEx>(frag);
  if (!region)
    return true;
  uint64_t offset = reloc->targetRef()->offset();

  // extract instruction
  uint64_t qc_e_jump = reloc->target() & 0xffffffffffff;
  bool isTailCall = (qc_e_jump & 0xf1f07f) == 0x00401f;

  Relocator::DWord S = getSymbolValuePLT(*reloc);
  Relocator::DWord A = reloc->addend();
  Relocator::DWord P = reloc->place(m_Module);
  Relocator::DWord X = S + A - P;

  bool doRelax = config().options().getRISCVRelax();
  bool DoCompressed = config().options().getRISCVRelaxToC();
  bool canRelaxXqci =
      config().targets().is32Bits() && config().options().getRISCVRelaxXqci();
  bool canRelax = doRelax && canRelaxXqci;
  bool canCompress = canRelax && DoCompressed && llvm::isInt<12>(X);
  bool canRelaxTbljal = canRelax && DoCompressed &&
                        config().options().getRISCVRelaxTbljal() &&
                        llvm::isUInt<32>(S + A);
  bool canRelaxJal = canRelax && llvm::isInt<21>(X);
  auto reportMissedQCCall = [&]() {
    reportMissedRelaxation("RISCV_QC_E_CALL", *region, offset,
                           (canCompress || canRelaxTbljal) ? 4 : 2,
                           reloc->symInfo()->name());
  };

  if (!canRelax) {
    reportMissedQCCall();
    return false;
  }

  // Prefer direct C.J/C.JAL over table-jump since both end up 2 bytes and
  // direct compression avoids the JVT entry overhead.
  if (canCompress) {
    uint32_t compressed = isTailCall ? 0xa001 : 0x2001;
    const char *msg = isTailCall ? "RISCV_QC_E_J_C" : "RISCV_QC_E_JAL_C";

    if (m_Module.getPrinter()->isVerbose())
      config().raise(Diag::relax_to_compress)
          << msg << llvm::utohexstr(qc_e_jump, true, 12)
          << llvm::utohexstr(compressed, true, 4) << reloc->symInfo()->name()
          << region->getOwningSection()->name() << llvm::utohexstr(offset)
          << region->getOwningSection()
                 ->getInputFile()
                 ->getInput()
                 ->decoratedPath();

    region->replaceInstruction(offset, reloc,
                               reinterpret_cast<uint8_t *>(&compressed), 2);
    // Replace the reloc to R_RISCV_RVC_JUMP
    reloc->setType(llvm::ELF::R_RISCV_RVC_JUMP);
    reloc->setTargetData(compressed);
    relaxDeleteBytes(msg, *region, offset + 2, 4, reloc->symInfo()->name());
    return true;
  }

  if (canRelaxTbljal && TableJumpFragment && TableJumpFragment->size() &&
      m_pRISCVTableJumpSection && !m_pRISCVTableJumpSection->isIgnore() &&
      !m_pRISCVTableJumpSection->isDiscard()) {
    unsigned rd = isTailCall ? /*x0*/ 0 : /*ra*/ 1;
    int EntryIndex =
        getTableJumpEntryIndex(*TableJumpFragment, reloc->symInfo(), rd);
    if (EntryIndex >= 0) {
      applyTableJumpRelaxation(reloc, *region, offset,
                               static_cast<unsigned>(EntryIndex));
      relaxDeleteBytes("RISCV_TBJAL", *region, offset + 2, 4,
                       reloc->symInfo()->name());
      return true;
    }
  }

  if (!canRelaxJal) {
    reportMissedQCCall();
    return false;
  }

  // Replace the instruction to JAL
  unsigned rd = isTailCall ? /*x0*/ 0 : /*ra*/ 1;
  uint32_t jal_instr = 0x6fu | rd << 7;
  region->replaceInstruction(offset, reloc,
                             reinterpret_cast<uint8_t *>(&jal_instr), 4);
  // Replace the reloc to R_RISCV_JAL
  reloc->setType(llvm::ELF::R_RISCV_JAL);
  reloc->setTargetData(jal_instr);
  // Delete the next instruction
  const char *msg = isTailCall ? "RISCV_QC_E_J" : "RISCV_QC_E_JAL";
  relaxDeleteBytes(msg, *region, offset + 4, 2,
                   reloc->symInfo()->name());

  return true;
}

bool RISCVLDBackend::doRelaxationLui(Relocation *reloc, Relocator::DWord G) {

  /* Three types of relaxation can be applied here, in order of preference:
   * -- zero-page: LUI is deleted and the other instruction is converted to
   *    x0-relative;
   * -- GP-relative, same as above but relative to GP, not available for PIC;
   * -- compressed LUI. */

  Fragment *frag = reloc->targetRef()->frag();
  RegionFragmentEx *region = llvm::dyn_cast<RegionFragmentEx>(frag);
  if (!region)
    return false;

  size_t SymbolSize = reloc->symInfo()->outSymbol()->size();
  Relocator::DWord S = getSymbolValuePLT(*reloc);
  Relocator::DWord A = reloc->addend();
  Relocator::DWord Value = S + A;
  uint64_t offset = reloc->targetRef()->offset();
  Relocation::Type type = reloc->type();

  // Do not relax complete zeroes because they can be mistaken for
  // not-yet-assigned values. This applies to both zero-page relaxation and GP
  // relaxation when GP is close to zero.

  // First, try zero-page relaxation. It's the cheapest and does not need GP.
  bool canRelaxZero = config().options().getRISCVRelax() &&
                      config().options().getRISCVZeroRelax() &&
                      llvm::isInt<12>(Value) && S != 0;

  // HI will be deleted, LO will be converted to use GP as base.
  // GP must be available and relocation must fit in 12 bits relative to GP.
  // There is no GP for shared objects.
  bool canRelaxToGP =
      config().options().getRISCVRelax() &&
      config().options().getRISCVGPRelax() && !config().isCodeIndep() &&
      G != 0 && S != 0 &&
      fitsInGP<12>(G, Value, frag, reloc->targetSection(), SymbolSize);

  if (type == llvm::ELF::R_RISCV_HI20) {

    StringRef Msg;
    if (canRelaxZero)
      Msg = "RISCV_LUI_ZERO";
    else if (canRelaxToGP)
      Msg = "RISCV_LUI_GP";
    if (!Msg.empty()) {
      reloc->setType(llvm::ELF::R_RISCV_NONE);
      relaxDeleteBytes(Msg, *region, offset, 4, reloc->symInfo()->name());
      return true;
    }

    // If cannot delete LUI, try compression.
    // RISCV ABI is not very precise on the conditions when relaxations must be
    // applied. First, this relaxation is selected based on the type of
    // relocations, not the actual instruction. It appears only LUI can have
    // R_RISCV_HI20 relocation, but if this is not the case, this code should be
    // revisited. The ABI also specifies that the next instruction should have a
    // R_RISCV_LO12_I or R_RISCV_LO12_S relocation. However, the replacement of
    // LUI with C.LUI does not change the semantics at all, and the next
    // instruction is not changed so this requirement seems unnecessary.
    // Binutils LD 2.30 also applies this relaxation when the next instruction
    // is not one with a LO12 relocations.
    // TODO: Check if compressed instruction set is available.

    unsigned instr = reloc->target();
    unsigned rd = (instr >> 7) & 0x1fu;

    // low 12 bits are signed
    int64_t lo_imm = llvm::SignExtend64<12>(Value);

    // The signed value must fit in 6 bits and not be zero.
    int64_t hi_imm =
        (static_cast<int64_t>(Value) - lo_imm) >> 12; //  note, arithmetic shift

    // Check for the LUI instruction that does not use x0 or x2 ( these are not
    // allowed in C.LUI) and 6-bit non-zero offset.
    // hi_imm == 0 will be relaxed as zero-page.
    bool canCompressLUI = config().options().getRISCVRelax() &&
                          config().options().getRISCVRelaxToC() &&
                          (instr & 0x00000007fu) == 0x00000037u && rd != 0 &&
                          rd != 2 && hi_imm != 0 && llvm::isInt<6>(hi_imm);
    if (canCompressLUI) {
      // Still report missing 2-byte relaxation opportunity because we only save
      // two bytes out of four.
      reportMissedRelaxation("RISCV_LUI_GP", *region, offset, 2,
                             reloc->symInfo()->name());

      // Replace encoding and relocation type, keep the register.
      unsigned compressed = 0x6001u | rd << 7;
      reloc->setTargetData(compressed);
      reloc->setType(ELF::riscv::internal::R_RISCV_RVC_LUI);
      relaxDeleteBytes("RISCV_LUI_C", *region, offset + 2, 2,
                       reloc->symInfo()->name());
      if (m_Module.getPrinter()->isVerbose())
        config().raise(Diag::relax_to_compress)
            << "RISCV_LUI_C" << llvm::utohexstr(instr, true, 8)
            << llvm::utohexstr(compressed, true, 4) << reloc->symInfo()->name()
            << region->getOwningSection()->name()
            << llvm::utohexstr(offset, true)
            << region->getOwningSection()
                   ->getInputFile()
                   ->getInput()
                   ->decoratedPath();
      return true;
    }

    // There is no GP for shared objects so do not report it as a missed
    // opportunity in that case. However, position-independent code
    // cannot have lui with absolute relocations, anyway.
    if (!config().isCodeIndep())
      reportMissedRelaxation("RISCV_LUI_GP", *region, offset, 4,
                             reloc->symInfo()->name());
    return false;
  }

  // The remaining code deals with LO relocations.
  uint32_t rs;
  if (canRelaxZero)
    rs = 0; // zero = x0
  else if (canRelaxToGP) {
    rs = 3; // x3 = gp
    Relocation::Type new_type;
    switch (type) {
    case llvm::ELF::R_RISCV_LO12_I:
      new_type = ELF::riscv::internal::R_RISCV_GPREL_I;
      break;
    case llvm::ELF::R_RISCV_LO12_S:
      new_type = ELF::riscv::internal::R_RISCV_GPREL_S;
      break;
    default:
      ASSERT(0, "Unexpected relocation type for RISCV_LUI_GP");
      return false;
    }
    reloc->setType(new_type);
  } else
    return false;

  // do relaxation
  uint32_t instr = reloc->target();
  uint32_t mask = 0xF8000;
  instr = (instr & ~mask) | rs << 15;
  reloc->setTargetData(instr);
  return true;
}

bool RISCVLDBackend::doRelaxationQCELi(Relocation *reloc, Relocator::DWord G) {
  /* Three similar relaxations can be applied here, in order of preference:
   * -- qc.e.li -> qc.li (saves 2 bytes)
   * -- qc.e.li -> addi (GP-relative) (saves 2 bytes, not available for PIC)
   */

  Fragment *frag = reloc->targetRef()->frag();
  RegionFragmentEx *region = llvm::dyn_cast<RegionFragmentEx>(frag);
  if (!region)
    return false;

  Relocator::DWord S = getSymbolValuePLT(*reloc);
  Relocator::DWord A = reloc->addend();
  Relocator::DWord Value = S + A;

  uint64_t offset = reloc->targetRef()->offset();
  size_t SymbolSize = reloc->symInfo()->outSymbol()->size();

  uint64_t instr = reloc->target();
  if ((instr & 0x00000000f07fu) != 0x1fu) {
    return false;
  }

  unsigned rd = (instr >> 7) & 0x1fu;
  if (rd == 0) {
    return false;
  }

  // These relaxations need to be enabled, and we need to be 32-bit.
  bool canRelaxXqci = config().options().getRISCVRelax() &&
                      config().targets().is32Bits() &&
                      config().options().getRISCVRelaxXqci();

  // For qc.li, we need to have a compatible value
  bool canRelaxQcLi = canRelaxXqci && llvm::isInt<20>(Value);

  // For addi (gp-relative), we need to enable GP relaxations, not be PIC-code,
  // and be in range of __global_pointer$.
  bool canRelaxGP =
      canRelaxXqci && config().options().getRISCVGPRelax() &&
      !config().isCodeIndep() && G != 0 && S != 0 &&
      fitsInGP<12>(G, Value, frag, reloc->targetSection(), SymbolSize);

  const char *msg = "RISCV_QC_E_LI_QC_LI";
  if (canRelaxQcLi) {
    uint32_t qc_li = 0x0000001bu | rd << 7;

    region->replaceInstruction(offset, reloc, reinterpret_cast<uint8_t *>(&qc_li), 4);
    reloc->setTargetData(qc_li);
    reloc->setType(ELF::riscv::internal::R_RISCV_QC_ABS20_U);
    relaxDeleteBytes(msg, *region, offset + 4, 2, reloc->symInfo()->name());
    return true;
  }

  if (canRelaxGP) {
    const char *msg = "RISCV_QC_E_LI_GP";
    unsigned rs = 3; // x3 = gp
    uint32_t addi = 0x00000013u | (rd << 7) | (rs << 15);

    region->replaceInstruction(offset, reloc, reinterpret_cast<uint8_t *>(&addi), 4);
    reloc->setTargetData(addi);
    reloc->setType(ELF::riscv::internal::R_RISCV_GPREL_I);
    relaxDeleteBytes(msg, *region, offset + 4, 2, reloc->symInfo()->name());
    return true;
  }

  if (canRelaxXqci)
    reportMissedRelaxation(msg, *region, offset, 2, reloc->symInfo()->name());
  return false;
}

uint64_t RISCVLDBackend::QCAccess::build48Bit(unsigned base_reg) const {
  switch (op) {
  case Operation::Unknown:
    return 0;
  case Operation::Lb:
    return qceitype(0x5u, 0x0u, reg, base_reg, 0);
  case Operation::Lbu:
    return qceitype(0x5u, 0x1u, reg, base_reg, 0);
  case Operation::Lh:
    return qceitype(0x5u, 0x2u, reg, base_reg, 0);
  case Operation::Lhu:
    return qceitype(0x5u, 0x3u, reg, base_reg, 0);
  case Operation::Lw:
    return qceitype(0x6u, 0x0u, reg, base_reg, 0);
  case Operation::Sb:
    return qcestype(0x6u, 0x1u, reg, base_reg, 0);
  case Operation::Sh:
    return qcestype(0x6u, 0x2u, reg, base_reg, 0);
  case Operation::Sw:
    return qcestype(0x6u, 0x3u, reg, base_reg, 0);
  }
}

uint32_t RISCVLDBackend::QCAccess::build32Bit(unsigned base_reg) const {
  switch (op) {
  case Operation::Unknown:
    return 0;
  case Operation::Lb:
    return itype(0x03u | (0x0u << 12), reg, base_reg, 0);
  case Operation::Lh:
    return itype(0x03u | (0x1u << 12), reg, base_reg, 0);
  case Operation::Lw:
    return itype(0x03u | (0x2u << 12), reg, base_reg, 0);
  case Operation::Lbu:
    return itype(0x03u | (0x4u << 12), reg, base_reg, 0);
  case Operation::Lhu:
    return itype(0x03u | (0x5u << 12), reg, base_reg, 0);
  case Operation::Sb:
    return stype(0x23u | (0x0u << 12), reg, base_reg, 0);
  case Operation::Sh:
    return stype(0x23u | (0x1u << 12), reg, base_reg, 0);
  case Operation::Sw:
    return stype(0x23u | (0x2u << 12), reg, base_reg, 0);
  }
}

bool RISCVLDBackend::doRelaxationQCAccess32(Relocation *QCELiReloc,
                                            Relocation *AccessReloc,
                                            Relocator::DWord G) {
  Fragment *frag = QCELiReloc->targetRef()->frag();
  RegionFragmentEx *region = llvm::dyn_cast<RegionFragmentEx>(frag);
  if (!region)
    return false;

  uint64_t qceli_instr = QCELiReloc->target();
  if ((qceli_instr & 0xf07fu) != 0x1fu)
    return false;
  unsigned qceli_rd = extractBits(qceli_instr, 11, 7);
  if (qceli_rd == X_ZERO)
    return false;

  uint32_t access_instr = 0;
  AccessReloc->targetRef()->memcpy(&access_instr, sizeof(access_instr), 0);
  QCAccess access = {};

  // Decode the RV32I access instruction so we have enough information to
  // know what width the access is, what its operand register is (rd for loads,
  // rs2 for stores), and (for loads) whether it does a sign or zero extension.
  // Its size will always be 4 bytes in this function, but we need that for the
  // doRelaxationQCCommon.
  switch (extractBits(access_instr, 6, 0)) {
  case 0x03u:
    // Loads are I-type
    access.reg = extractBits(access_instr, 11, 7);
    access.offset = llvm::SignExtend64<12>(extractBits(access_instr, 31, 20));
    access.size = 4;
    switch (extractBits(access_instr, 14, 12)) {
    case 0b000u:
      access.op = QCAccess::Operation::Lb;
      break;
    case 0b100u:
      access.op = QCAccess::Operation::Lbu;
      break;
    case 0b001u:
      access.op = QCAccess::Operation::Lh;
      break;
    case 0b101u:
      access.op = QCAccess::Operation::Lhu;
      break;
    case 0b010u:
      access.op = QCAccess::Operation::Lw;
      break;
    default:
      return false;
    }
    break;
  case 0x23u:
    // Stores are S-type
    access.reg = extractBits(access_instr, 24, 20);
    access.offset =
        llvm::SignExtend64<12>((extractBits(access_instr, 31, 25) << 5) |
                               extractBits(access_instr, 11, 7));
    access.size = 4;
    switch (extractBits(access_instr, 14, 12)) {
    case 0b000u:
      access.op = QCAccess::Operation::Sb;
      break;
    case 0b001u:
      access.op = QCAccess::Operation::Sh;
      break;
    case 0b010u:
      access.op = QCAccess::Operation::Sw;
      break;
    default:
      return false;
    }
    break;
  default:
    return false;
  }

  // Check rd of qc.e.li against base register of access
  if (qceli_rd != extractBits(access_instr, 19, 15))
    return false;

  assert(access.isValid() && "function should have returned if access invalid");
  return doRelaxationQCAccessCommon(QCELiReloc, AccessReloc, G, access);
}

bool RISCVLDBackend::doRelaxationQCAccess16(Relocation *QCELiReloc,
                                            Relocation *AccessReloc,
                                            Relocator::DWord G) {
  Fragment *frag = QCELiReloc->targetRef()->frag();
  RegionFragmentEx *region = llvm::dyn_cast<RegionFragmentEx>(frag);
  if (!region)
    return false;

  uint64_t qceli_instr = QCELiReloc->target();
  if ((qceli_instr & 0xf07fu) != 0x1fu)
    return false;
  unsigned qceli_rd = extractBits(qceli_instr, 11, 7);
  if (qceli_rd == X_ZERO)
    return false;

  uint16_t access_instr = 0;
  AccessReloc->targetRef()->memcpy(&access_instr, sizeof(access_instr), 0);
  QCAccess access = {};

  access.size = 2;
  access.reg = 8 + extractBits(access_instr, 4, 2);

  // Decode the RV32 Zca/Zcb access instruction so we have enough information to
  // know what width the access is, what its operand register is (rd for loads,
  // rs2 for stores), and (for loads) whether it does a sign or zero extension.
  // Its size will always be 2 bytes in this function, but we need that for the
  // doRelaxationQCCommon.
  //
  // This decoding is more complex than for 32-bit instructions as the 16-bit
  // instructions are much less regular.
  if ((access_instr & 0x3u) != 0x0u)
    return false;
  switch (extractBits(access_instr, 15, 13)) {
  case 0b010u:
    // c.lw: CL-format
    access.op = QCAccess::Operation::Lw;
    access.offset = (extractBits(access_instr, 5, 5) << 6) |
                    (extractBits(access_instr, 12, 10) << 3) |
                    (extractBits(access_instr, 6, 6) << 2);
    break;
  case 0b110u:
    // c.sw: CS-format
    access.op = QCAccess::Operation::Sw;
    access.offset = (extractBits(access_instr, 5, 5) << 6) |
                    (extractBits(access_instr, 12, 10) << 3) |
                    (extractBits(access_instr, 6, 6) << 2);
    break;
  case 0b100u:
    switch (extractBits(access_instr, 11, 10)) {
    case 0b00u:
      // c.lbu: off[1:0] = {instr[5], instr[6]}, byte offset 0-3
      access.op = QCAccess::Operation::Lbu;
      access.offset = (extractBits(access_instr, 5, 5) << 1) |
                      extractBits(access_instr, 6, 6);
      break;
    case 0b01u:
      // c.lhu/c.lh: off[1] = instr[5], halfword offset 0 or 2
      if (extractBits(access_instr, 6, 6) == 0b1u)
        access.op = QCAccess::Operation::Lh;
      else
        access.op = QCAccess::Operation::Lhu;
      access.offset = extractBits(access_instr, 5, 5) << 1;
      break;
    case 0b10u:
      // c.sb: off[1:0] = {instr[5], instr[6]}, byte offset 0-3
      access.op = QCAccess::Operation::Sb;
      access.offset = (extractBits(access_instr, 5, 5) << 1) |
                      extractBits(access_instr, 6, 6);
      break;
    case 0b11u:
      // c.sh: off[1] = instr[5], halfword offset 0 or 2
      access.op = QCAccess::Operation::Sh;
      access.offset = extractBits(access_instr, 5, 5) << 1;
      break;
    default:
      llvm_unreachable("Impossible Encoding");
    }
    break;
  default:
    return false;
  }

  // Check rd of qc.e.li against base register of access
  if (qceli_rd != 8 + extractBits(access_instr, 9, 7))
    return false;

  assert(access.isValid() && "function should have returned if access invalid");
  return doRelaxationQCAccessCommon(QCELiReloc, AccessReloc, G, access);
}

bool RISCVLDBackend::doRelaxationQCAccessCommon(Relocation *QCELiReloc,
                                                Relocation *AccessReloc,
                                                Relocator::DWord G,
                                                QCAccess access) {
  /* Four relaxation variants, applied in priority order:
   * -- GP-relative standard: -> GP-relative 32-bit load/store (saves 6/4 bytes)
   * -- Absolute standard:    -> x0-based 32-bit load/store (saves 6/4 bytes)
   * -- GP-relative xqcilo:   -> 6-byte qc.e.l/qc.e.s (saves 4/2 bytes)
   * -- Absolute xqcilo:      -> 6-byte qc.e.l/qc.e.s (saves 4/2 bytes)
   * All variants require --relax-xqci since qc.e.li is itself an xqci insn.
   */

  assert(access.isValid() &&
         "function should only be called with valid access info");

  RegionFragmentEx *region =
      llvm::cast<RegionFragmentEx>(QCELiReloc->targetRef()->frag());
  uint64_t qceli_offset = QCELiReloc->targetRef()->offset();

  Relocator::Address S = getSymbolValuePLT(*QCELiReloc);
  Relocator::DWord A = QCELiReloc->addend();
  Relocator::Address Value = S + A;
  // The effective address includes the offset from the access instruction;
  // all range checks operate on this adjusted value.
  Relocator::Address AdjValue = Value + (Relocator::DWord)access.offset;
  size_t SymbolSize = QCELiReloc->symInfo()->outSymbol()->size();

  // These relaxations need to be enabled, and we need to be 32-bit.
  bool canRelaxXqci = config().options().getRISCVRelax() &&
                      config().targets().is32Bits() &&
                      config().options().getRISCVRelaxXqci();

  bool canRelaxGPStd = canRelaxXqci && config().options().getRISCVGPRelax() &&
                       !config().isCodeIndep() && G != 0 && S != 0 &&
                       fitsInGP<12>(G, AdjValue, region,
                                    QCELiReloc->targetSection(), SymbolSize);

  bool canRelaxAbsStd = canRelaxXqci &&
                        config().options().getRISCVZeroRelax() && S != 0 &&
                        llvm::isInt<12>(getSignedAddress(AdjValue));

  bool canRelaxGPXqci = canRelaxXqci && config().options().getRISCVGPRelax() &&
                        !config().isCodeIndep() && G != 0 && S != 0 &&
                        fitsInGP<26>(G, AdjValue, region,
                                     QCELiReloc->targetSection(), SymbolSize);

  bool canRelaxAbsXqci = canRelaxXqci &&
                         config().options().getRISCVZeroRelax() && S != 0 &&
                         llvm::isInt<26>(getSignedAddress(AdjValue));

  const char *msg = "RISCV_QC_E_LI_ACCESS";

  // This replaces the `qc.e.li; access` sequence with a 4-byte RVI access
  // instruction
  auto applyStdRelax = [&](unsigned base_reg, Relocation::Type reloc_load,
                           Relocation::Type reloc_store,
                           const char *variant_msg) {
    uint32_t new_access = access.build32Bit(base_reg);
    region->replaceInstruction(qceli_offset, QCELiReloc,
                               reinterpret_cast<uint8_t *>(&new_access), 4);
    QCELiReloc->setTargetData(new_access);
    QCELiReloc->setAddend(A + (Relocator::DWord)access.offset);
    QCELiReloc->setType(access.isLoad() ? reloc_load : reloc_store);
    relaxDeleteBytes(variant_msg, *region, qceli_offset + 4, 2 + access.size,
                     QCELiReloc->symInfo()->name());
    AccessReloc->setType(llvm::ELF::R_RISCV_NONE);
  };

  // This replaces the `qc.e.li; access` sequence with a 6-byte Xqcilo access
  // instruction.
  auto applyXqciloRelax = [&](unsigned base_reg, Relocation::Type reloc_load,
                              Relocation::Type reloc_store,
                              const char *variant_msg) {
    uint64_t new_access = access.build48Bit(base_reg);
    region->replaceInstruction(qceli_offset, QCELiReloc,
                               reinterpret_cast<uint8_t *>(&new_access), 6);
    QCELiReloc->setTargetData(new_access);
    QCELiReloc->setAddend(A + (Relocator::DWord)access.offset);
    QCELiReloc->setType(access.isLoad() ? reloc_load : reloc_store);
    relaxDeleteBytes(variant_msg, *region, qceli_offset + 6, access.size,
                     QCELiReloc->symInfo()->name());
    AccessReloc->setType(llvm::ELF::R_RISCV_NONE);
  };

  // 1. GP-relative standard
  if (canRelaxGPStd) {
    applyStdRelax(X_GP, ELF::riscv::internal::R_RISCV_GPREL_I,
                  ELF::riscv::internal::R_RISCV_GPREL_S,
                  "RISCV_QC_E_LI_ACCESS_GP_STD");
    return true;
  }
  // 2. Absolute standard
  if (canRelaxAbsStd) {
    applyStdRelax(X_ZERO, llvm::ELF::R_RISCV_LO12_I, llvm::ELF::R_RISCV_LO12_S,
                  "RISCV_QC_E_LI_ACCESS_ABS_STD");
    return true;
  }
  // 3. GP-relative xqcilo
  if (canRelaxGPXqci) {
    applyXqciloRelax(X_GP, ELF::riscv::internal::R_RISCV_QC_GPREL26_I,
                     ELF::riscv::internal::R_RISCV_QC_GPREL26_S,
                     "RISCV_QC_E_LI_ACCESS_GP_XQCI");
    reportMissedRelaxation(msg, *region, qceli_offset, 2,
                           QCELiReloc->symInfo()->name());
    return true;
  }
  // 4. Absolute xqcilo
  if (canRelaxAbsXqci) {
    applyXqciloRelax(X_ZERO, ELF::riscv::internal::R_RISCV_QC_ABS26_I,
                     ELF::riscv::internal::R_RISCV_QC_ABS26_S,
                     "RISCV_QC_E_LI_ACCESS_ABS_XQCI");
    reportMissedRelaxation(msg, *region, qceli_offset, 2,
                           QCELiReloc->symInfo()->name());
    return true;
  }

  if (canRelaxXqci)
    reportMissedRelaxation(msg, *region, qceli_offset, access.size,
                           QCELiReloc->symInfo()->name());
  return false;
}

bool RISCVLDBackend::doRelaxationTLSDESC(Relocation &R, bool Relax) {

  Fragment *frag = R.targetRef()->frag();
  RegionFragmentEx *region = llvm::dyn_cast<RegionFragmentEx>(frag);
  if (!region)
    return false;

  const StringRef RelaxType = "RISCV_TLSDESC";
  const ResolveInfo &Sym = *R.symInfo();
  size_t offset = R.targetRef()->offset();

  // In executables, TLSDESC relocations are either removed, when the
  // instruction is replaced with a NOP, or replaced with one of a
  // different type. The conditions and the instruction substitution rules are
  // the same whether or not relaxation is enabled.
  auto attempt = [&]() -> bool {
    bool Relaxed = Relax && config().options().getRISCVRelax() &&
                   config().options().getRISCVRelaxTLSDESC();
    if (Relaxed)
      relaxDeleteBytes(RelaxType, *region, offset, 4, Sym.name());
    else {
      // Otherwise, the instruction is replaced with a NOP.
      reportMissedRelaxation(RelaxType, *region, offset, 4, Sym.name());
      uint32_t NOPi32 = static_cast<uint32_t>(NOP);
      region->replaceInstruction(
          offset, &R, reinterpret_cast<uint8_t *>(&NOPi32), 4);
    }
    R.setType(llvm::ELF::R_RISCV_NONE);
    return Relaxed;
  };

  // The first two instructions are always deleted.
  if (R.type() == llvm::ELF::R_RISCV_TLSDESC_HI20 ||
      R.type() == llvm::ELF::R_RISCV_TLSDESC_LOAD_LO12)
    return attempt();

  // We need the base relocation to get the symbol and the original addend.
  const Relocation *BaseReloc = getBaseReloc(R);
  if (!BaseReloc)
    return false;

  bool isLocalExec = !isSymbolPreemptible(*BaseReloc->symInfo());
  if (isLocalExec) {
    // We only need the symbol value to determine if it fits in 12 bits so we
    // can use the short form. Hopefully, it will not increase later.
    int64_t S = getRelocator()->getSymValue(BaseReloc);
    int64_t A = BaseReloc->addend();
    int64_t Value = S + A;
    bool isShortForm = hi20(Value) == 0;
    if (isShortForm) {
      // LE short form is one ADDI instruction.
      switch (R.type()) {
      case llvm::ELF::R_RISCV_TLSDESC_ADD_LO12:
        return attempt();
      case llvm::ELF::R_RISCV_TLSDESC_CALL:
        // addi a0, zero, %lo(S)
        R.setTargetData(itype(ADDI, X_A0, X_ZERO, 0));
        R.setType(llvm::ELF::R_RISCV_LO12_I);
        break;
      }
    } else {
      // LE long form is LUI + ADDI.
      switch (R.type()) {
      case llvm::ELF::R_RISCV_TLSDESC_ADD_LO12:
        // lui a0, %hi(S)
        R.setTargetData(utype(LUI, X_A0, 0));
        R.setType(llvm::ELF::R_RISCV_HI20);
        // Inherit the original addend from the instruction we have deleted.
        R.setAddend(BaseReloc->addend());
        break;
      case llvm::ELF::R_RISCV_TLSDESC_CALL:
        // addi a0, a0, %lo(S)
        R.setTargetData(itype(ADDI, X_A0, X_A0, 0));
        R.setType(llvm::ELF::R_RISCV_LO12_I);
        break;
      }
    }
  } else {
    // Initial executable: AUIPC + GOT LW/LD:
    switch (R.type()) {
    case llvm::ELF::R_RISCV_TLSDESC_ADD_LO12:
      // auipc a0, %got_pcrel_hi(S)
      R.setTargetData(utype(AUIPC, X_A0, 0));
      R.setType(llvm::ELF::R_RISCV_GOT_HI20);
      // Inherit the original addend from the instruction we have deleted.
      R.setAddend(BaseReloc->addend());
      break;
    case llvm::ELF::R_RISCV_TLSDESC_CALL:
      // lw/ld a0, a0+%pcrel_lo(S)
      // The correspondence between this relocation and the corresponding HI20
      // stays the same even its type changes.
      R.setTargetData(
          itype(config().targets().is32Bits() ? LW : LD, X_A0, X_A0, 0));
      R.setType(llvm::ELF::R_RISCV_PCREL_LO12_I);
      break;
    }
  }

  return true;
}

bool RISCVLDBackend::doRelaxationAlign(Relocation *pReloc) {
  FragmentRef *Ref = pReloc->targetRef();
  Fragment *frag = Ref->frag();
  RegionFragmentEx *region = llvm::dyn_cast<RegionFragmentEx>(frag);
  uint64_t offset = Ref->offset();
  if (!region)
    return false;
  uint32_t Alignment = 1;

  // Compute the smallest power of 2, greater than the addend.
  while (Alignment <= pReloc->addend())
    Alignment = Alignment * 2;

  uint64_t SymValue =
      frag->getOutputELFSection()->addr() + Ref->getOutputOffset(m_Module);

  // Figure out how far we are from the TargetAddress
  uint64_t TargetAddress = SymValue;
  alignAddress(TargetAddress, Alignment);
  uint32_t NopBytesToAdd = TargetAddress - SymValue;
  if (NopBytesToAdd == pReloc->addend())
    return false;

  if (NopBytesToAdd > pReloc->addend()) {
    config().raise(Diag::error_riscv_relaxation_align)
        << pReloc->addend() << NopBytesToAdd
        << region->getOwningSection()->name()
        << llvm::utohexstr(offset + NopBytesToAdd, true)
        << region->getOwningSection()
               ->getInputFile()
               ->getInput()
               ->decoratedPath();
    return false;
  }

  if (m_Module.getPrinter()->isVerbose())
    config().raise(Diag::add_nops)
        << "RISCV_ALIGN" << NopBytesToAdd << region->getOwningSection()->name()
        << llvm::utohexstr(offset, true)
        << region->getOwningSection()
               ->getInputFile()
               ->getInput()
               ->decoratedPath();

  region->addRequiredNops(offset, NopBytesToAdd);
  relaxDeleteBytes("RISCV_ALIGN", *region, offset + NopBytesToAdd,
                   pReloc->addend() - NopBytesToAdd, "");
  // Set the reloc to do nothing.
  pReloc->setType(llvm::ELF::R_RISCV_NONE);
  return true;
}

template <unsigned N>
bool RISCVLDBackend::fitsInGP(Relocator::DWord G, Relocation::DWord Value,
                              Fragment *frag, ELFSection *TargetSection,
                              size_t SymSize) const {
  int64_t X = 0;

  int64_t Alignment = 0;
  // TargetSection may be invalid when using Absolute symbols
  OutputSectionEntry *targetFragOutputSection =
      TargetSection ? TargetSection->getOutputSection() : nullptr;

  // Dont try to relax if the target section is associated with NOLOAD
  // and is not assigned a segment.
  if (targetFragOutputSection && !targetFragOutputSection->getLoadSegment())
    return false;

  // Handle symbols not in the output section
  OutputSectionEntry *GPOutputSection =
      m_GlobalPointerSection ? m_GlobalPointerSection->getOutputSection()
                             : nullptr;
  if (TargetSection) {
    if (GPOutputSection != targetFragOutputSection && (TargetSection->size())) {
      Alignment =
          targetFragOutputSection->getLoadSegment()->getMaxSectionAlign();
    } else {
      Alignment = TargetSection->getAddrAlign();
    }
  }
  if (Value >= G)
    X = Value - G + Alignment + SymSize;
  else
    X = Value - G - Alignment - SymSize;
  return llvm::isInt<N>(X);
}

bool RISCVLDBackend::addSymbolToOutput(ResolveInfo *Info) {
  // For Partial Links, we want to preserve all the symbols.
  if (LinkerConfig::Object == config().codeGenType())
    return true;
  // If the linker is using emit relocs, all relocations need to be
  // emitted.
  if (config().options().emitRelocs())
    return true;
  // Any local labels are discarded.
  if (!config().options().shouldKeepLabels() && Info->isLocal() &&
      Info->getName().starts_with(".L")) {
    FragmentRef *Ref = Info->outSymbol()->fragRef();
    if (Ref && Ref->frag())
      Ref->frag()->addSymbol(Info);
    m_LabeledSymbols.push_back(Info);
    return false;
  }
  return true;
}
bool RISCVLDBackend::isGOTReloc(const Relocation &reloc) const {
  switch (reloc.type()) {
  case llvm::ELF::R_RISCV_GOT_HI20:
  case llvm::ELF::R_RISCV_TLS_GOT_HI20:
  case llvm::ELF::R_RISCV_TLS_GD_HI20:
    return true;
  default:
    break;
  }
  return false;
}

bool RISCVLDBackend::doRelaxationPC(Relocation *reloc, Relocator::DWord G) {

  // There is no GP for shared objects.
  if (config().isCodeIndep())
    return false;

  if (m_DisableGPRelocs.count(reloc))
    return false;

  Fragment *frag = reloc->targetRef()->frag();
  RegionFragmentEx *region = llvm::dyn_cast<RegionFragmentEx>(frag);
  if (!region)
    return false;

  // Test if the symbol with size can fall in 12 bits.
  size_t SymbolSize = reloc->symInfo()->outSymbol()->size();
  Relocator::DWord S = getSymbolValuePLT(*reloc);
  Relocator::DWord A = reloc->addend();

  Relocation::Type new_type = 0x0;
  Relocation::Type type = reloc->type();
  switch (type) {
  case llvm::ELF::R_RISCV_PCREL_LO12_I:
    new_type = ELF::riscv::internal::R_RISCV_GPREL_I;
    break;
  case llvm::ELF::R_RISCV_PCREL_LO12_S:
    new_type = ELF::riscv::internal::R_RISCV_GPREL_S;
    break;
  default:
    break;
  }

  if (new_type) {
    // Lookup reloc to get actual addend of HI.
    const Relocation *HIReloc = getBaseReloc(*reloc);
    // If this is a GOT relocation, we cannot convert
    // this relative to GP.
    if (isGOTReloc(*HIReloc))
      return false;
    if (!HIReloc)
      ASSERT(0, "HIReloc not found! Internal Error!");
    S = getSymbolValuePLT(*HIReloc);
    A = HIReloc->addend();
    SymbolSize = HIReloc->symInfo()->outSymbol()->size();
  }

  uint64_t offset = reloc->targetRef()->offset();
  bool canRelax =
      config().options().getRISCVRelax() &&
      config().options().getRISCVGPRelax() && G != 0 &&
      fitsInGP<12>(G, S + A, frag, reloc->targetSection(), SymbolSize);

  // HI will be deleted, Low will be converted to use gp as base.
  if (type == llvm::ELF::R_RISCV_PCREL_HI20) {
    if (!canRelax) {
      reportMissedRelaxation("RISCV_PC_GP", *region, offset, 4,
                             reloc->symInfo()->name());
      return false;
    }

    reloc->setType(llvm::ELF::R_RISCV_NONE);
    relaxDeleteBytes("RISCV_PC_GP", *region, offset, 4,
                     reloc->symInfo()->name());
    return true;
  }

  if (!canRelax)
    return false;

  uint64_t instr = reloc->target();
  uint64_t mask = 0x1F << 15;
  instr = (instr & ~mask) | (0x3 << 15);
  reloc->setType(new_type);
  reloc->setTargetData(instr);
  reloc->setAddend(A);
  return true;
}

bool RISCVLDBackend::shouldIgnoreRelocSync(Relocation *pReloc) const {
  // Ignore all relaxation relocations for now, later based on
  // user specified command line flags.
  switch (pReloc->type()) {
  case llvm::ELF::R_RISCV_NONE:
  // Must ignore Relax and Align even relaxation is enabled
  case llvm::ELF::R_RISCV_RELAX:
  case llvm::ELF::R_RISCV_ALIGN:
  case llvm::ELF::R_RISCV_VENDOR:
  // ULEB128 relocations are handled separately
  case llvm::ELF::R_RISCV_SET_ULEB128:
  case llvm::ELF::R_RISCV_SUB_ULEB128:
    return true;
  default: {
    using namespace eld::ELF::riscv;
    if (pReloc->type() >= internal::FirstNonstandardRelocation &&
        pReloc->type() <= internal::LastNonstandardRelocation)
      return true;
    break;
  }
  }
  return false;
}

void RISCVLDBackend::translatePseudoRelocation(Relocation *reloc) {
  // Convert the call to R_RISCV_PCREL_HI20
  reloc->setType(llvm::ELF::R_RISCV_PCREL_HI20);

  // THe JALR is for a label created when PC was added to the high part of
  // address and saved in a register. Account for the change in PC when
  // computing lower 12 bits.
  uint64_t offset = reloc->targetRef()->offset();
  FragmentRef *fragRef =
      make<FragmentRef>(*(reloc->targetRef()->frag()), offset + 4);
  Relocation *reloc_jalr = Relocation::Create(llvm::ELF::R_RISCV_PCREL_LO12_I,
                                              32, fragRef, reloc->addend());
  m_BaseRelocs[reloc_jalr] = reloc;
  reloc_jalr->setSymInfo(reloc->symInfo());
  m_InternalRelocs.push_back(reloc_jalr);
}

enum RelaxationPass {
  RELAXATION_CALL, // Must start at zero
  RELAXATION_PC,
  RELAXATION_LUI,
  RELAXATION_TLSDESC,
  RELAXATION_ALIGN,
  RELAXATION_PASS_COUNT, // Number of passes
};

void RISCVLDBackend::preRelaxation() {
  // Table-jump candidates require full relocation/symbol state, so defer this
  // initialization until just before the relaxation pass.
  initTableJump();
}

void RISCVLDBackend::mayBeRelax(int relaxation_pass, bool &pFinished) {
  pFinished = true;

  // TLSDESC relaxations only apply to executables.
  if (relaxation_pass == RELAXATION_TLSDESC && !config().isBuildingExecutable())
    return;

  // RELAXATION_ALIGN pass, which is the last pass, will set pFinished to
  // false if it has made changes. It is needed to call createProgramHdrs()
  // again in the outer loop. Therefore, this function may be entered once more,
  // for no good reason.
  if (relaxation_pass >= RELAXATION_PASS_COUNT) {
    return;
  }

  // retrieve gp value, .data + 0x800
  Relocator::DWord GP = 0x0;
  if (m_pGlobalPointer)
    GP = m_pGlobalPointer->value();

  // start relocation relaxation
  for (auto &input : m_Module.getObjectList()) {
    ELFObjectFile *ObjFile = llvm::dyn_cast<ELFObjectFile>(input);
    if (!ObjFile)
      continue;
    for (auto &rs : ObjFile->getRelocationSections()) {
      // bypass the reloc section if section is ignored/discarded.
      if (rs->isIgnore())
        continue;
      if (rs->isDiscard())
        continue;
      auto relocList = rs->getLink()->getRelocations();
      for (llvm::SmallVectorImpl<Relocation *>::iterator it = relocList.begin();
           config().getDiagEngine()->diagnose() && it != relocList.end();
           ++it) {
        auto relocation = *it;
        Relocation::Type type = relocation->type();

        // Processing of R_RISCV_ALIGN is unconditional.
        if (relaxation_pass == RELAXATION_ALIGN) {
          if (type == llvm::ELF::R_RISCV_ALIGN && doRelaxationAlign(relocation))
            pFinished = false;
          continue;
        }

        if (relaxation_pass == RELAXATION_TLSDESC) {
          // doRelaxationTLSDESC is used for both TLSDESC optimizations and
          // relaxations, therefore this function should be called regardless
          // of whether relaxations are enabled.
          switch (type) {
          case llvm::ELF::R_RISCV_TLSDESC_HI20:
          case llvm::ELF::R_RISCV_TLSDESC_LOAD_LO12:
          case llvm::ELF::R_RISCV_TLSDESC_ADD_LO12:
          case llvm::ELF::R_RISCV_TLSDESC_CALL: {
            // In the TLSDESC relaxation sequence, only the instruction with
            // R_RISCV_TLSDESC_HI20 can be marked with R_RISCV_RELAX to indicate
            // that the whole sequence is relaxable. So the other three
            // relocation types will inherit this knowledge from the
            // R_RISCV_TLSDESC_HI20 relocation.
            bool Relax;
            if (type == llvm::ELF::R_RISCV_TLSDESC_HI20)
              Relax = hasRelax(*relocation);
            else {
              const Relocation *BaseReloc = getBaseReloc(*relocation);
              Relax = BaseReloc && hasRelax(*BaseReloc);
            }
            doRelaxationTLSDESC(*relocation, Relax);
            break;
          }
          }
          continue;
        }

        // try to relax
        if (!hasRelax(*relocation))
          continue;

        switch (relaxation_pass) {
        case RELAXATION_CALL:
          switch (type) {
          case llvm::ELF::R_RISCV_CALL:
          case llvm::ELF::R_RISCV_CALL_PLT:
            doRelaxationCall(relocation);
            break;
          case llvm::ELF::R_RISCV_JAL:
            doRelaxationJal(relocation);
            break;
          case ELF::riscv::internal::R_RISCV_QC_E_CALL_PLT:
            doRelaxationQCCall(relocation);
            break;
          }
          break;
        case RELAXATION_PC:
          switch (type) {
          case llvm::ELF::R_RISCV_PCREL_HI20:
          case llvm::ELF::R_RISCV_PCREL_LO12_I:
          case llvm::ELF::R_RISCV_PCREL_LO12_S:
            doRelaxationPC(relocation, GP);
            break;
          }
          break;
        case RELAXATION_LUI:
          switch (type) {
          case llvm::ELF::R_RISCV_LO12_S:
          case llvm::ELF::R_RISCV_LO12_I:
          case llvm::ELF::R_RISCV_HI20:
            doRelaxationLui(relocation, GP);
            break;
          case ELF::riscv::internal::R_RISCV_QC_E_32: {
            bool relaxed = false;
            if (Relocation *AccessReloc = getBaseReloc(*relocation)) {
              if (hasRelax(*AccessReloc)) {
                if (AccessReloc->type() ==
                    ELF::riscv::internal::R_RISCV_QC_ACCESS_32)
                  relaxed = doRelaxationQCAccess32(relocation, AccessReloc, GP);
                else if (AccessReloc->type() ==
                         ELF::riscv::internal::R_RISCV_QC_ACCESS_16)
                  relaxed = doRelaxationQCAccess16(relocation, AccessReloc, GP);
              }
            }
            if (!relaxed)
              doRelaxationQCELi(relocation, GP);
            break;
          }
          }
          break;
        }
      } // for all relocations
      if (!config().getDiagEngine()->diagnose()) {
        m_Module.setFailure(true);
        pFinished = true;
        return;
      }
    } // for all relocation section
  }

  // On RISC-V, relaxation consists of a fixed number of passes, except
  // R_RISCV_ALIGN will cause another empty pass if it made changes.
  if (relaxation_pass < llvm::ELF::R_RISCV_ALIGN)
    pFinished = false;
}

/// finalizeSymbol - finalize the symbol value
bool RISCVLDBackend::finalizeTargetSymbols() {
  if (m_pIRelativeStart && m_pIRelativeEnd) {
    m_pIRelativeStart->setValue(
        getRelaPLT()->getOutputSection()->getSection()->addr());
    m_pIRelativeEnd->setValue(
        getRelaPLT()->getOutputSection()->getSection()->addr() +
        getRelaPLT()->getOutputSection()->getSection()->size());
    addSectionInfo(m_pIRelativeStart, getRelaPLT());
    addSectionInfo(m_pIRelativeEnd, getRelaPLT());
  }

  for (auto &I : m_LabeledSymbols)
    m_Module.getLinker()->getObjLinker()->finalizeSymbolValue(I);

  ELFSegment *attr_segment =
      elfSegmentTable().find(llvm::ELF::PT_RISCV_ATTRIBUTES);
  if (attr_segment)
    attr_segment->setMemsz(0);

  if (config().codeGenType() == LinkerConfig::Object)
    return true;

  return true;
}

void RISCVLDBackend::initializeAttributes() {
  getInfo().initializeAttributes(m_Module.getIRBuilder()->getInputBuilder());
}

bool RISCVLDBackend::validateArchOpts() const {
  return checkABIStr(config().options().abiString());
}

bool RISCVLDBackend::checkABIStr(llvm::StringRef abi) const {
  // Valid strings for abi in RV32: ilp32, ilp32d, ilp32f and optional
  // 'c' with each of these.
  bool hasError = false;
  size_t idx = 0;
  if (abi.size() < 5 || !abi.starts_with("ilp32")) {
    hasError = true;
  } else {
    abi = abi.drop_front(5);
    idx = 5;
  }
  while (idx < abi.size() && !hasError) {
    switch (*(abi.data() + idx)) {
    case 'c':
    case 'd':
    case 'f':
      idx++;
      break;
    default:
      hasError = true;
      break;
    }
  }
  if (hasError) {
    config().raise(Diag::unsupported_abi) << abi;
    return false;
  }
  return true;
}

bool RISCVLDBackend::handleRelocation(ELFSection *pSection,
                                      Relocation::Type pType, LDSymbol &pSym,
                                      uint32_t pOffset,
                                      Relocation::Address pAddend) {
  if (config().codeGenType() == LinkerConfig::Object)
    return false;
  if (SectionRelocMap.find(pSection) == SectionRelocMap.end())
    SectionRelocMap[pSection];

  switch (pType) {
  case llvm::ELF::R_RISCV_TLS_DTPREL32:
  case llvm::ELF::R_RISCV_TLS_DTPREL64:
  case llvm::ELF::R_RISCV_TLS_TPREL32:
  case llvm::ELF::R_RISCV_TLS_TPREL64: {
    config().raise(Diag::unsupported_rv_reloc)
        << getRISCVRelocName(pType) << pSym.name()
        << pSection->originalInput()->getInput()->decoratedPath();
    m_Module.setFailure(true);
    return false;
  }

  // R_RISCV_RELAX is a different beast. It has proper r_offset but has no
  // symbol. It is a simple placeholder to relaxation hint. Other hints have
  // real symbols but not this one. We need to map it to null, otherwise
  // --emit relocs will not find symbol in index map.
  case llvm::ELF::R_RISCV_RELAX: {
    Relocation *reloc = IRBuilder::addRelocation(
        getRelocator(), pSection, pType, *(LDSymbol::null()), pOffset, pAddend);
    pSection->addRelocation(reloc);
    return true;
  }

  case llvm::ELF::R_RISCV_SUB_ULEB128:
  case llvm::ELF::R_RISCV_32:
  case llvm::ELF::R_RISCV_64:
  case llvm::ELF::R_RISCV_ADD8:
  case llvm::ELF::R_RISCV_ADD16:
  case llvm::ELF::R_RISCV_ADD32:
  case llvm::ELF::R_RISCV_ADD64:
  case llvm::ELF::R_RISCV_SUB8:
  case llvm::ELF::R_RISCV_SUB16:
  case llvm::ELF::R_RISCV_SUB32:
  case llvm::ELF::R_RISCV_SUB64:
  case llvm::ELF::R_RISCV_SUB6:
  case llvm::ELF::R_RISCV_SET6:
  case llvm::ELF::R_RISCV_SET8:
  case llvm::ELF::R_RISCV_SET16:
  case llvm::ELF::R_RISCV_SET32:
  case llvm::ELF::R_RISCV_SET_ULEB128: {
    Relocation *reloc = IRBuilder::addRelocation(getRelocator(), pSection,
                                                 pType, pSym, pOffset, pAddend);
    pSection->addRelocation(reloc);
    std::unordered_map<uint32_t, Relocation *> &relocMap =
        SectionRelocMap[pSection];
    auto offsetToReloc = relocMap.find(pOffset);
    if (offsetToReloc == relocMap.end())
      relocMap.insert(std::make_pair(pOffset, reloc));
    else
      m_GroupRelocs.insert(std::make_pair(reloc, offsetToReloc->second));
    return true;
  }
  // R_RISCV_PCREL_LO* and TLSDESC relocations have the corresponding HI reloc
  // as the syminfo, we need to find out the actual target by inspecting this
  // reloc and set the appropriate relocation.
  case llvm::ELF::R_RISCV_PCREL_LO12_I:
  case llvm::ELF::R_RISCV_PCREL_LO12_S:
  case llvm::ELF::R_RISCV_TLSDESC_LOAD_LO12:
  case llvm::ELF::R_RISCV_TLSDESC_ADD_LO12:
  case llvm::ELF::R_RISCV_TLSDESC_CALL: {
    if (pAddend) {
      config().raise(Diag::warn_ignore_pcrel_lo_addend)
          << pSym.name() << getRISCVRelocName(pType)
          << pSection->originalInput()->getInput()->decoratedPath();
      pAddend = 0;
    }
    Relocation *reloc = IRBuilder::addRelocation(getRelocator(), pSection,
                                                 pType, pSym, pOffset, pAddend);
    if (reloc)
      pSection->addRelocation(reloc);
    return true;
  }
  default: {
    using namespace eld::ELF::riscv;

    // Handle R_RISCV_CUSTOM<n> relocations with their paired R_RISCV_VENDOR
    // relocation - by trying to find the relevant vendor symbol, and using
    // that to translate them into their relevant internal relocation type.
    if (pType >= internal::FirstNonstandardRelocation &&
        pType <= internal::LastNonstandardRelocation) {
      // The internal list of relocations is not sorted yet, scan the whole
      // list.
      auto RI =
          std::find_if(pSection->getRelocations().begin(),
                       pSection->getRelocations().end(), [&](Relocation *R) {
                         return R->type() == llvm::ELF::R_RISCV_VENDOR &&
                                !R->targetRef()->isNull() &&
                                R->targetRef()->offset() == pOffset;
                       });
      if (RI == pSection->getRelocations().end()) {
        // The ABI requires that R_RISCV_VENDOR precedes any R_RISCV_CUSTOM<n>
        // Relocation.
        config().raise(Diag::error_rv_vendor_not_found)
            << getRISCVRelocName(pType)
            << pSection->originalInput()->getInput()->decoratedPath();
        m_Module.setFailure(true);
        return false;
      }

      Relocation *VendorReloc = *RI;
      std::string VendorSymbol = VendorReloc->symInfo()->getName().str();
      auto [VendorOffset, VendorFirst, VendorLast] =
          llvm::StringSwitch<std::tuple<uint32_t, uint32_t, uint32_t>>(
              VendorSymbol)
              .Case("QUALCOMM", {internal::QUALCOMMVendorRelocationOffset,
                                 internal::FirstQUALCOMMVendorRelocation,
                                 internal::LastQUALCOMMVendorRelocation})
              .Default({0, 0, 0});

      // Check if we support this vendor at all
      if (VendorOffset == 0) {
        config().raise(Diag::error_rv_unknown_vendor_symbol)
            << VendorSymbol << getRISCVRelocName(pType)
            << pSection->originalInput()->getInput()->decoratedPath();
        m_Module.setFailure(true);
        return false;
      }

      Relocation::Type InternalType = pType + VendorOffset;

      // Check if it's an internal vendor relocation we support
      if ((InternalType < VendorFirst) || (VendorLast < InternalType)) {
        // This uses the original (not vendor) relocation name
        config().raise(Diag::error_rv_unknown_vendor_relocation)
            << VendorSymbol << getRISCVRelocName(pType)
            << pSection->originalInput()->getInput()->decoratedPath();
        m_Module.setFailure(true);
        return false;
      }

      // Allow custom handling of vendor relocations (using the internal type)
      if (handleVendorRelocation(pSection, InternalType, pSym, pOffset,
                                 pAddend))
        return true;

      // Add a relocation using the internal type
      Relocation *InternalReloc = IRBuilder::addRelocation(
          getRelocator(), pSection, InternalType, pSym, pOffset, pAddend);
      pSection->addRelocation(InternalReloc);
      return true;
    }
    break;
  }
  }
  return false;
}

bool RISCVLDBackend::handlePendingRelocations(ELFSection *section) {

  std::optional<Relocation *> lastRelocationVisited;
  std::optional<Relocation *> lastSetUleb128RelocationVisited;

  for (auto &Relocation : section->getRelocations()) {
    switch (Relocation->type()) {
    case llvm::ELF::R_RISCV_SUB_ULEB128:
      if (!lastRelocationVisited ||
          ((lastRelocationVisited.value()->type() !=
            llvm::ELF::R_RISCV_SET_ULEB128) ||
           (lastRelocationVisited.value()->getOffset() !=
            Relocation->getOffset()))) {
        config().raise(Diag::error_relocation_not_paired)
            << section->originalInput()->getInput()->decoratedPath()
            << section->name() << Relocation->getOffset()
            << getRISCVRelocName(Relocation->type()) << "R_RISCV_SET_ULEB128";
        return false;
      }
      lastRelocationVisited.reset();
      lastSetUleb128RelocationVisited.reset();
      break;
    case llvm::ELF::R_RISCV_SET_ULEB128:
      lastSetUleb128RelocationVisited = Relocation;
      LLVM_FALLTHROUGH;
    default:
      lastRelocationVisited = Relocation;
      break;
    }
  }

  if (lastSetUleb128RelocationVisited) {
    config().raise(Diag::error_relocation_not_paired)
        << section->originalInput()->getInput()->decoratedPath()
        << section->name()
        << lastSetUleb128RelocationVisited.value()->getOffset()
        << getRISCVRelocName(lastSetUleb128RelocationVisited.value()->type())
        << "R_RISCV_SUB_ULEB128";
    return false;
  }

  // Sort the relocation table in offset order to quickly find a relocation at
  // the offset, or by the symbol offset. This is needed for several reasons:
  // find out which relocations are relaxable, find the correponding HI20
  // relocation for some LO12, and for "group relocation".
  std::stable_sort(section->getRelocations().begin(),
                   section->getRelocations().end(),
                   [](Relocation *A, Relocation *B) {
                     return A->getOffset() < B->getOffset();
                   });

  // Skip HI20/LO12 pairing, R_RISCV_RELAX marking, and vendor relocation
  // grouping under -r.
  if (config().isLinkPartial())
    return true;

  struct Less {
    bool operator()(const Relocation *X, uint64_t Y) const {
      return !X->targetRef()->isNull() && X->targetRef()->offset() < Y;
    }
    bool operator()(uint64_t X, const Relocation *Y) const {
      return Y->targetRef()->isNull() || X < Y->targetRef()->offset();
    }
  };

  // Iterate over groups of relocations with equal offsets.
  llvm::SmallVectorImpl<Relocation *>::iterator RI;
  for (auto GroupI = section->getRelocations().begin();
       GroupI != section->getRelocations().end(); GroupI = RI) {
    uint64_t Offset = (*GroupI)->getOffset();

    // Iterate over relocations within a group.
    for (RI = GroupI;
         RI != section->getRelocations().end() && (*RI)->getOffset() == Offset;
         ++RI) {
      Relocation *R = *RI;

      if (R->type() == llvm::ELF::R_RISCV_RELAX) {
        if (RI != GroupI)
          m_RelocsWithRelax.insert(*(RI - 1));
        continue;
      }

      switch (R->type()) {
      // R_RISCV_PCREL_LO* and TLSDESC relocations have the corresponding HI
      // reloc as the syminfo, we need to find out the actual target by
      // inspecting this reloc and set the appropriate relocation.
      case llvm::ELF::R_RISCV_PCREL_LO12_I:
      case llvm::ELF::R_RISCV_PCREL_LO12_S:
      case llvm::ELF::R_RISCV_TLSDESC_LOAD_LO12:
      case llvm::ELF::R_RISCV_TLSDESC_ADD_LO12:
      case llvm::ELF::R_RISCV_TLSDESC_CALL: {
        bool IsPCRel = R->type() == llvm::ELF::R_RISCV_PCREL_LO12_I ||
                       R->type() == llvm::ELF::R_RISCV_PCREL_LO12_S;
        uint64_t HiOffset = R->symInfo()->outSymbol()->value();
        auto HiRelocRange =
            std::equal_range(section->getRelocations().begin(),
                             section->getRelocations().end(), HiOffset, Less());

        Relocation *HiReloc = nullptr;
        for (auto HiRI = HiRelocRange.first; HiRI != HiRelocRange.second;
             ++HiRI) {
          if ((IsPCRel &&
               ((*HiRI)->type() == llvm::ELF::R_RISCV_PCREL_HI20 ||
                (*HiRI)->type() == llvm::ELF::R_RISCV_GOT_HI20 ||
                (*HiRI)->type() == llvm::ELF::R_RISCV_TLS_GD_HI20 ||
                (*HiRI)->type() == llvm::ELF::R_RISCV_TLS_GOT_HI20)) ||
              (!IsPCRel &&
               (*HiRI)->type() == llvm::ELF::R_RISCV_TLSDESC_HI20)) {
            HiReloc = *HiRI;
            break;
          }
        }
        if (!HiReloc) {
          config().raise(Diag::rv_hi20_not_found)
              << R->symInfo()->outSymbol()->name()
              << getRISCVRelocName(R->type())
              << section->originalInput()->getInput()->decoratedPath();
          m_Module.setFailure(true);
          return false;
        }

        R->setSymInfo(HiReloc->symInfo());
        m_BaseRelocs[R] = HiReloc;

        if (IsPCRel && Offset < HiOffset) {
          // Disable GP Relaxation for this pair to mimic GNU
          m_DisableGPRelocs.insert(R);
          m_DisableGPRelocs.insert(HiReloc);
        }
        break;
      }
      case ELF::riscv::internal::R_RISCV_QC_E_32: {
        // R_RISCV_QC_E_32 is special as in addition to knowing if it is
        // relaxable, we need to distinguish between 32-bit and 16-bit types of
        // relaxation based on the "access" relocation type. We reuse
        // m_BaseRelocs to store the pointer to the access relocation.
        // We look for the access relocation after loading all the relocations
        // because it is at a higher offset and usually follows the original
        // one in the list.
        uint64_t AccessOffset = Offset + 6;
        auto AccessRelocRange = std::equal_range(
            section->getRelocations().begin(), section->getRelocations().end(),
            AccessOffset, Less());
        if (AccessRelocRange.first != AccessRelocRange.second) {
          for (auto AccessRI = AccessRelocRange.first + 1;
               AccessRI != AccessRelocRange.second; ++AccessRI) {
            if ((*AccessRI)->type() ==
                    ELF::riscv::internal::R_RISCV_QC_ACCESS_32 ||
                (*AccessRI)->type() ==
                    ELF::riscv::internal::R_RISCV_QC_ACCESS_16) {
              m_BaseRelocs[R] = *AccessRI;
              break;
            }
          }
        }
        break;
      }
      }
    }
  }

  return true;
}

bool RISCVLDBackend::handleVendorRelocation(ELFSection *pSection,
                                            Relocation::Type pType,
                                            LDSymbol &pSym, uint32_t pOffset,
                                            Relocation::Address pAddend) {
  using namespace eld::ELF::riscv;
  assert((internal::FirstInternalRelocation <= pType) &&
         (pType <= internal::LastInternalRelocation) &&
         "handleVendorRelocation should only be called with internal "
         "relocation types");

  switch (pType) {
  default:
    break;
  };
  return false;
}

Relocation::Type RISCVLDBackend::getRemappedInternalRelocationType(
    Relocation::Type pType) const {
  using namespace eld::ELF::riscv;

  // Valid public relocations are left alone.
  if (pType <= internal::LastPublicRelocation)
    return pType;

  // Qualcomm vendor relocations are mapped back to R_RISCV_CUSTOM<N>.
  if ((internal::FirstQUALCOMMVendorRelocation <= pType) &&
      (pType <= internal::LastQUALCOMMVendorRelocation))
    return pType - internal::QUALCOMMVendorRelocationOffset;

  // All other internal relocations are mapped to R_RISCV_NONE as we have no
  // better information.
  return llvm::ELF::R_RISCV_NONE;
}

void RISCVLDBackend::doPreLayout() {
  m_psdata = m_Module.getScript().sectionMap().find(".sdata");
  if (getRelaPLT()) {
    getRelaPLT()->setSize(getRelaPLT()->getRelocationCount() *
                          getRelaEntrySize());
    m_Module.addOutputSection(getRelaPLT());
  }
  if (getRelaDyn()) {
    getRelaDyn()->setSize(getRelaDyn()->getRelocationCount() *
                          getRelaEntrySize());
    m_Module.addOutputSection(getRelaDyn());
  }
  if (ELFSection *S = getRelaPatch()) {
    S->setSize(S->getRelocationCount() * getRelaEntrySize());
    m_Module.addOutputSection(S);
  }
}

void RISCVLDBackend::evaluateTargetSymbolsBeforeRelaxation() {
  if (m_Module.getScript().linkerScriptHasSectionsCommand()) {
    if (m_pGlobalPointer) {
      auto F = m_SymbolToSection.find(m_pGlobalPointer);
      if (F != m_SymbolToSection.end())
        m_GlobalPointerSection = F->second;
    }
    return;
  }

  if (m_pGlobalPointer) {
    m_GlobalPointerSection = m_psdata;
    if (m_psdata)
      m_pGlobalPointer->setValue(m_psdata->addr() + 0x800);
    if (m_Module.getPrinter()->isVerbose())
      config().raise(Diag::set_symbol)
          << m_pGlobalPointer->str() << m_pGlobalPointer->value();
    if (m_pGlobalPointer) {
      m_pGlobalPointer->resolveInfo()->setBinding(ResolveInfo::Global);
      addSectionInfo(m_pGlobalPointer, m_psdata);
    }
  }
}

void RISCVLDBackend::defineGOTSymbol(Fragment &pFrag) {
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
                pFrag.getOwningSection()->getInputFile(), SymbolName,
                ResolveInfo::Object, ResolveInfo::Define, ResolveInfo::Local,
                0x0, // size
                0x0, // value
                make<FragmentRef>(pFrag, 0x0), ResolveInfo::Hidden);
  }
  if (m_pGOTSymbol)
    m_pGOTSymbol->setShouldIgnore(false);
  if (m_Module.getConfig().options().isSymbolTracingRequested() &&
      m_Module.getConfig().options().traceSymbol(SymbolName))
    config().raise(Diag::target_specific_symbol) << SymbolName;
}

void RISCVLDBackend::defineIRelativeRange(ResolveInfo &pSym) {
  if (m_Module.getScript().linkerScriptHasSectionsCommand())
    return;

  if (!m_pIRelativeStart && !m_pIRelativeEnd) {
    auto SymbolName = "__rela_iplt_start";
    m_pIRelativeStart =
        m_Module.getIRBuilder()
            ->addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
                m_Module.getInternalInput(Module::Script), SymbolName,
                ResolveInfo::Type::NoType, ResolveInfo::Define,
                ResolveInfo::Binding::Local,
                0,   // size
                0x0, // value
                FragmentRef::null(), ResolveInfo::Visibility::Default);
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
                ResolveInfo::Type::NoType, ResolveInfo::Define,
                ResolveInfo::Binding::Local,
                0x0, // size
                0x0, // value
                FragmentRef::null(), ResolveInfo::Visibility::Default);
    if (m_Module.getConfig().options().isSymbolTracingRequested() &&
        m_Module.getConfig().options().traceSymbol(SymbolName)) {
      config().raise(Diag::target_specific_symbol) << SymbolName;
    }
    m_pIRelativeEnd->setShouldIgnore(false);
  }
}

bool RISCVLDBackend::finalizeScanRelocations() {
  Fragment *frag = nullptr;
  if (auto *GOT = getGOT())
    if (GOT->hasSectionData())
      frag = *GOT->getFragmentList().begin();
  if (frag)
    defineGOTSymbol(*frag);

  if (!config().isCodeStatic())
    return true;

  bool is32Bits = config().targets().is32Bits();

  for (auto &[symInfo, plt] : m_PLTMap) {
    if (!symInfo->isIFunc() || !symInfo->hasIFuncDirectRef() ||
        !symInfo->hasIFuncNeedsGOT())
      continue;

    ELFObjectFile *PLTSlotObjFile =
        llvm::cast<ELFObjectFile>(plt->getOwningSection()->getInputFile());
    RISCVGOT *G = createGOT(GOT::GOTType::Regular, PLTSlotObjFile, symInfo);

    FragmentRef *PLTFragRef = make<FragmentRef>(*plt, 0);
    Relocation *r = Relocation::Create(
        is32Bits ? llvm::ELF::R_RISCV_32 : llvm::ELF::R_RISCV_64,
        is32Bits ? 32 : 64, make<FragmentRef>(*G, 0), 0);
    PLTSlotObjFile->getGOT()->addRelocation(r);
    r->modifyRelocationFragmentRef(PLTFragRef);

    recordGOT(symInfo, G);
    symInfo->setReserved(symInfo->reserved() | Relocator::ReserveGOT);
  }
  return true;
}

// Create GOT entry.
RISCVGOT *RISCVLDBackend::createGOT(GOT::GOTType T, ELFObjectFile *Obj,
                                    ResolveInfo *R) {

  if (R != nullptr && ((config().options().isSymbolTracingRequested() &&
                        config().options().traceSymbol(*R)) ||
                       m_Module.getPrinter()->traceDynamicLinking()))
    config().raise(Diag::create_got_entry)
        << GOT::getGOTTypeAsStr(T) << R->name();
  // If we are creating a GOT, always create a .got.plt.
  if (!getGOTPLT()->hasFragments()) {
    LDSymbol *Dynamic = m_Module.getNamePool().findSymbol("_DYNAMIC");
    // TODO: This should be GOT0, not GOTPLT0.
    RISCVGOT::CreateGOT0(getGOT(), Dynamic ? Dynamic->resolveInfo() : nullptr,
                         config().targets().is32Bits());
    RISCVGOT::CreateGOTPLT0(getGOTPLT(), nullptr,
                            config().targets().is32Bits());
  }

  RISCVGOT *G = nullptr;
  bool GOT = true;
  switch (T) {
  case GOT::Regular:
    G = RISCVGOT::Create(Obj->getGOT(), R, config().targets().is32Bits());
    break;
  case GOT::GOTPLT0:
    G = llvm::dyn_cast<RISCVGOT>(*getGOTPLT()->getFragmentList().begin());
    GOT = false;
    break;
  case GOT::GOTPLTN: {
    G = RISCVGOT::CreateGOTPLTN(R->isPatchable() ? getGOTPatch()
                                                 : Obj->getGOTPLT(),
                                R, config().targets().is32Bits());
    GOT = false;
    break;
  }
  case GOT::TLS_GD: {
    G = RISCVGOT::CreateGD(Obj->getGOT(), R, config().targets().is32Bits());
    break;
  }
  case GOT::TLS_LD:
    // TODO: Apparently, this case is called either from getTLSModuleID (for a
    // unique slot) or from R_RISCV_TLS_GD_HI20 relocation (per relocation).
    // Handle both cases for now, but this may need to be double checked.
    G = RISCVGOT::CreateLD(Obj ? Obj->getGOT() : getGOT(), R,
                           config().targets().is32Bits());
    break;
  case GOT::TLS_IE:
    G = RISCVGOT::CreateIE(Obj->getGOT(), R, config().targets().is32Bits());
    break;
  case GOT::TLS_DESC:
    G = RISCVGOT::CreateTLSDESC(Obj->getGOTPLT(), R,
                                config().targets().is32Bits());
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
void RISCVLDBackend::recordGOT(ResolveInfo *I, RISCVGOT *G) { m_GOTMap[I] = G; }

// Record GOTPLT entry.
void RISCVLDBackend::recordGOTPLT(ResolveInfo *I, RISCVGOT *G) {
  m_GOTPLTMap[I] = G;
}

// Find an entry in the GOT.
RISCVGOT *RISCVLDBackend::findEntryInGOT(ResolveInfo *I) const {
  auto Entry = m_GOTMap.find(I);
  if (Entry == m_GOTMap.end())
    return nullptr;
  return Entry->second;
}

// Create PLT entry.
RISCVPLT *RISCVLDBackend::createPLT(ELFObjectFile *Obj, ResolveInfo *R,
                                    bool isIRelative) {
  bool is32Bits = config().targets().is32Bits();
  if ((config().options().isSymbolTracingRequested() &&
       config().options().traceSymbol(*R)) ||
      m_Module.getPrinter()->traceDynamicLinking())
    config().raise(Diag::create_plt_entry) << R->name();

  reportErrorIfPLTIsDiscarded(R);

  RISCVGOT *G = createGOT(GOT::GOTPLTN, Obj, R);
  RISCVPLT *P = RISCVPLT::CreatePLTN(G, Obj->getPLT(), R, is32Bits);
  recordPLT(R, P);
  if (R->isPatchable()) {
    G->setValueType(GOT::SymbolValue);
    // Create a static relocation in the patch relocation section, which will
    // be written to the output but will not be applied statically. Static
    // relocations are normally resolved to the PLT for functions that have
    // a PLT, but since this value is written by the GOT slot directly,
    // it will store the real symbol value.
    Relocation *Rel = Relocation::Create(
        is32Bits ? llvm::ELF::R_RISCV_32 : llvm::ELF::R_RISCV_64,
        is32Bits ? 32 : 64, make<FragmentRef>(*G));
    Rel->setSymInfo(R);
    getRelaPatch()->addRelocation(Rel);
    // Point the `__llvm_patchable` alias to the PLT slot. If a patchable
    // symbol is not referenced, the PLT and alias will not be created.
    LDSymbol *PatchableAlias = m_Module.getNamePool().findSymbol(
        std::string("__llvm_patchable_") + R->name());
    if (!PatchableAlias || PatchableAlias->shouldIgnore())
      config().raise(Diag::error_patchable_alias_not_found)
          << std::string("__llvm_patchable_") + R->name();
    else
      PatchableAlias->setFragmentRef(make<FragmentRef>(*P));
  } else {
    if (!config().options().hasNow()) {
      // For lazy binding, create GOTPLT0 and PLT0, if they don't exist.
      if (!getPLT()->hasFragments())
        RISCVPLT::CreatePLT0(*this, createGOT(GOT::GOTPLT0, Obj, nullptr),
                             getPLT(), is32Bits);
      // Create a static relocation to the PLT0 fragment.
      Relocation *r0 = Relocation::Create(
          is32Bits ? llvm::ELF::R_RISCV_32 : llvm::ELF::R_RISCV_64,
          is32Bits ? 32 : 64, make<FragmentRef>(*G));
      r0->modifyRelocationFragmentRef(
          make<FragmentRef>(**getPLT()->getFragmentList().begin()));
      Obj->getGOTPLT()->addRelocation(r0);
    }
    Relocation::Type relocType = (isIRelative ? llvm::ELF::R_RISCV_IRELATIVE
                                              : llvm::ELF::R_RISCV_JUMP_SLOT);
    // Create a dynamic relocation for the GOTPLT slot.
    Relocation *dynRel = Relocation::Create(relocType, is32Bits ? 32 : 64,
                                            make<FragmentRef>(*G));
    dynRel->setSymInfo(R);
    Obj->getRelaPLT()->addRelocation(dynRel);
  }
  return P;
}

// Record PLT entry
void RISCVLDBackend::recordPLT(ResolveInfo *I, RISCVPLT *P) { m_PLTMap[I] = P; }

// Find an entry in the PLT
RISCVPLT *RISCVLDBackend::findEntryInPLT(ResolveInfo *I) const {
  auto Entry = m_PLTMap.find(I);
  if (Entry == m_PLTMap.end())
    return nullptr;
  return Entry->second;
}

uint64_t
RISCVLDBackend::getValueForDiscardedRelocations(const Relocation *R) const {
  ELFSection *applySect = R->targetRef()->frag()->getOwningSection();
  llvm::StringRef applySectionName = applySect->name();
  if ((applySectionName == ".debug_loc") ||
      (applySectionName == ".debug_ranges"))
    return 1;
  return GNULDBackend::getValueForDiscardedRelocations(R);
}

/// dynamic - the dynamic section of the target machine.
ELFDynamic *RISCVLDBackend::dynamic() { return m_pDynamic; }

std::optional<bool>
RISCVLDBackend::shouldProcessSectionForGC(const ELFSection &pSec) const {
  if (pSec.getType() == llvm::ELF::SHT_RISCV_ATTRIBUTES)
    return false;
  return GNULDBackend::shouldProcessSectionForGC(pSec);
}

unsigned int
RISCVLDBackend::getTargetSectionOrder(const ELFSection &pSectHdr) const {
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

void RISCVLDBackend::recordRelaxationStats(ELFSection &Section,
                                           size_t NumBytesDeleted,
                                           size_t NumBytesNotDeleted) {
  OutputSectionEntry *O = Section.getOutputSection();
  eld::LayoutInfo *P = m_Module.getLayoutInfo();
  LinkStats *R = m_Stats[O];
  if (!m_ModuleStats) {
    m_ModuleStats = eld::make<RISCVRelaxationStats>();
    if (P)
      P->registerStats(&getModule(), m_ModuleStats);
  }
  if (!R) {
    R = eld::make<RISCVRelaxationStats>();
    m_Stats[O] = R;
    if (P)
      getModule().getLayoutInfo()->registerStats(O, R);
  }
  llvm::dyn_cast<RISCVRelaxationStats>(R)->addBytesDeleted(NumBytesDeleted);
  m_ModuleStats->addBytesDeleted(NumBytesDeleted);
  llvm::dyn_cast<RISCVRelaxationStats>(R)->addBytesNotDeleted(
      NumBytesNotDeleted);
  m_ModuleStats->addBytesNotDeleted(NumBytesNotDeleted);
}

/// doCreateProgramHdrs - backend can implement this function to create the
/// target-dependent segments
void RISCVLDBackend::doCreateProgramHdrs() {
  ELFSection *attr =
      m_Module.getScript().sectionMap().find(".riscv.attributes");
  if (!attr || !attr->size())
    return;
  ELFSegment *attr_seg = make<ELFSegment>(llvm::ELF::PT_RISCV_ATTRIBUTES, 0);
  elfSegmentTable().addSegment(attr_seg);
  attr_seg->setAlign(1);
  attr_seg->append(attr->getOutputSection());
}

int RISCVLDBackend::numReservedSegments() const {
  ELFSegment *attr_segment =
      elfSegmentTable().find(llvm::ELF::PT_RISCV_ATTRIBUTES);
  if (attr_segment)
    return GNULDBackend::numReservedSegments();
  int numReservedSegments = 0;
  ELFSection *attr =
      m_Module.getScript().sectionMap().find(".riscv.attributes");
  if (nullptr != attr && 0x0 != attr->size())
    ++numReservedSegments;
  return numReservedSegments + GNULDBackend::numReservedSegments();
}

void RISCVLDBackend::addTargetSpecificSegments() {
  ELFSegment *attr_segment =
      elfSegmentTable().find(llvm::ELF::PT_RISCV_ATTRIBUTES);
  if (attr_segment)
    return;
  doCreateProgramHdrs();
}

void RISCVLDBackend::setDefaultConfigs() {
  GNULDBackend::setDefaultConfigs();
  if (config().options().threadsEnabled() &&
      !config().isGlobalThreadingEnabled()) {
    config().disableThreadOptions(
        LinkerConfig::EnableThreadsOpt::ScanRelocations |
        LinkerConfig::EnableThreadsOpt::ApplyRelocations |
        LinkerConfig::EnableThreadsOpt::LinkerRelaxation);
  }
}

eld::Expected<void>
RISCVLDBackend::postProcessing(llvm::FileOutputBuffer &pOutput) {
  eld::Expected<void> expBasePostProcess =
      GNULDBackend::postProcessing(pOutput);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expBasePostProcess);
  for (auto &RelocPair : m_GroupRelocs) {
    const Relocation *pReloc = RelocPair.first;
    if (pReloc->type() != llvm::ELF::R_RISCV_SUB_ULEB128)
      continue;
    FragmentRef::Offset Off = pReloc->targetRef()->getOutputOffset(m_Module);
    if (Off >= pReloc->targetRef()->getOutputELFSection()->size())
      continue;
    size_t out_offset =
        pReloc->targetRef()->getOutputELFSection()->offset() + Off;
    uint8_t *target_addr = pOutput.getBufferStart() + out_offset;
    if (!overwriteLEB128(target_addr, pReloc->target())) {
      /// FIXME: this is not the right error message:
      /// lld: error: overflow.o:(.alloc+0x8): ULEB128 value 128 exceeds
      /// available space; references 'w2'
      pReloc->issueSignedOverflow(*getRelocator(), pReloc->target(), -1, -1);
    }
  }
  return {};
}

namespace eld {

//===----------------------------------------------------------------------===//
/// createRISCVLDBackend - the help function to create corresponding
/// RISCVLDBackend
GNULDBackend *createRISCVLDBackend(Module &pModule) {
  return make<RISCVLDBackend>(pModule,
                              make<RISCVStandaloneInfo>(pModule.getConfig()));
}

} // namespace eld

//===----------------------------------------------------------------------===//
// Force static initialization.
//===----------------------------------------------------------------------===//
extern "C" void ELDInitializeRISCVLDBackend() {
  // Register the linker backend
  eld::TargetRegistry::RegisterGNULDBackend(TheRISCV32Target,
                                            createRISCVLDBackend);
  eld::TargetRegistry::RegisterGNULDBackend(TheRISCV64Target,
                                            createRISCVLDBackend);
}
