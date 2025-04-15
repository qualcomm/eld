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
#include "RISCVRelocationInternal.h"
#include "RISCVRelocator.h"
#include "RISCVStandaloneInfo.h"
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

Relocation::Address RISCVLDBackend::getSymbolValuePLT(Relocation &R) {
  ResolveInfo *rsym = R.symInfo();
  if (rsym && (rsym->reserved() & Relocator::ReservePLT)) {
    if (const Fragment *S = findEntryInPLT(rsym))
      return S->getAddr(config().getDiagEngine());
    if (const ResolveInfo *S = findAbsolutePLT(rsym))
      return S->value();
  }
  return getRelocator()->getSymValue(&R);
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

  if (LinkerConfig::Object == config().codeGenType())
    return;

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

bool RISCVLDBackend::initBRIslandFactory() { return true; }

bool RISCVLDBackend::initStubFactory() { return true; }

bool RISCVLDBackend::readSection(InputFile &pInput, ELFSection *S) {
  eld::LayoutPrinter *P = m_Module.getLayoutPrinter();
  if (S->isCode()) {
    const char *Buf = pInput.getCopyForWrite(S->offset(), S->size());
    eld::RegionFragmentEx *F =
        make<RegionFragmentEx>(Buf, S->size(), S, S->getAddrAlign());
    S->addFragment(F);
    if (P)
      P->recordFragment(&pInput, S, F);
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
    if (!AttributeFragment) {
      AttributeFragment =
          make<RISCVAttributeFragment>(m_pRISCVAttributeSection);
      m_pRISCVAttributeSection->getFragmentList().push_back(AttributeFragment);
      LayoutPrinter *printer = getModule().getLayoutPrinter();
      if (printer)
        printer->recordFragment(m_pRISCVAttributeSection->getInputFile(),
                                m_pRISCVAttributeSection, AttributeFragment);
    }
    RegionFragment *R =
        llvm::dyn_cast<RegionFragment>(S->getFragmentList().front());
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

bool RISCVLDBackend::doRelaxationCall(Relocation *reloc, bool DoCompressed) {
  Fragment *frag = reloc->targetRef()->frag();
  RegionFragmentEx *region = llvm::dyn_cast<RegionFragmentEx>(frag);
  if (!region)
    return true;
  uint64_t offset = reloc->targetRef()->offset();

  // extract the next instruction
  uint32_t jalr_instr;
  reloc->targetRef()->memcpy(&jalr_instr, sizeof(uint32_t), 4);

  // double check if the next instruction is jal
  if ((jalr_instr & 0x7F) != 0x67)
    return false;

  unsigned rd = (jalr_instr >> 7) & 0x1fu;
  bool canCompress = (rd == 0 || (rd == 1 && config().targets().is32Bits()));

  // test if it can fall into 21bits
  Relocator::DWord S = getSymbolValuePLT(*reloc);
  Relocator::DWord A = reloc->addend();
  Relocator::DWord P = reloc->place(m_Module);
  Relocator::DWord X = S + A - P;
  bool canRelax = config().options().getRISCVRelax() && llvm::isInt<21>(X);

  if (!canRelax) {
    reportMissedRelaxation("RISCV_CALL", *region, offset, canCompress ? 6 : 4,
                           reloc->symInfo()->name());
    return false;
  }

  // Test if we can use C.JAL or C.J.
  if (canCompress) {
    bool canRelaxToCompressed = config().options().getRISCVRelax() &&
                                DoCompressed && llvm::isInt<12>(X);
    if (canRelaxToCompressed) {
      // C.J uses x0 as return register (writes are ignored)
      // C.JAL uses x1 as return register (ABI link register)
      uint32_t compressed = rd == 1 ? 0x2001 : 0xa001;
      const char *msg = rd == 1 ? "RISCV_CALL_JAL" : "RISCV_CALL_J";
      if (m_Module.getPrinter()->isVerbose())
        config().raise(Diag::relax_to_compress)
            << msg
            << llvm::utohexstr(reloc->target(), true, 8) + "," +
                   llvm::utohexstr(jalr_instr, true, 8)
            << llvm::utohexstr(compressed, true, 4) << reloc->symInfo()->name()
            << region->getOwningSection()->name() << llvm::utohexstr(offset)
            << region->getOwningSection()
                   ->getInputFile()
                   ->getInput()
                   ->decoratedPath();

      region->replaceInstruction(offset, reloc, compressed, 2);
      // Replace the reloc to R_RISCV_RVC_JUMP
      reloc->setType(llvm::ELF::R_RISCV_RVC_JUMP);
      reloc->setTargetData(compressed);
      // Delete the next instruction
      relaxDeleteBytes("RISCV_CALL_C", *region, offset + 2, 6,
                       reloc->symInfo()->name());
      return true;
    }
    reportMissedRelaxation("RISCV_CALL_C", *region, offset, 2,
                           reloc->symInfo()->name());
  }

  // Replace the instruction to JAL
  uint32_t instr = 0x6fu | rd << 7;
  region->replaceInstruction(offset, reloc, instr, 4 /* Replace bytes */);
  // Replace the reloc to R_RISCV_JAL
  reloc->setType(llvm::ELF::R_RISCV_JAL);
  reloc->setTargetData(instr);
  // Delete the next instruction
  relaxDeleteBytes("RISCV_CALL", *region, offset + 4, 4,
                   reloc->symInfo()->name());
  return true;
}

bool RISCVLDBackend::doRelaxationLui(Relocation *reloc, Relocator::DWord G) {

  /* Three types of relaxation can be applied here, in order of preference:
   * -- zero-page: LUI is deleted and the other instruction is converted to
   *    x0-relative [ not implemented ];
   * -- GP-relative, same as above but relative to GP, not available for PIC;
   * -- compressed LUI. */

  Fragment *frag = reloc->targetRef()->frag();
  RegionFragmentEx *region = llvm::dyn_cast<RegionFragmentEx>(frag);
  if (!region)
    return false;

  size_t SymbolSize = reloc->symInfo()->outSymbol()->size();
  Relocator::DWord S = getRelocator()->getSymValue(reloc);
  Relocator::DWord A = reloc->addend();
  uint64_t offset = reloc->targetRef()->offset();
  Relocation::Type type = reloc->type();

  // HI will be deleted, LO will be converted to use GP as base.
  // GP must be available and relocation must fit in 12 bits relative to GP.
  // There is no GP for shared objects.
  bool canRelaxToGP =
      config().options().getRISCVRelax() &&
      config().options().getRISCVGPRelax() && !config().isCodeIndep() &&
      G != 0 && fitsInGP(G, S + A, frag, reloc->targetSection(), SymbolSize);

  if (type == llvm::ELF::R_RISCV_HI20) {

    if (canRelaxToGP) {
      reloc->setType(llvm::ELF::R_RISCV_NONE);
      relaxDeleteBytes("RISCV_LUI_GP", *region, offset, 4,
                       reloc->symInfo()->name());
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
    int64_t lo_imm = llvm::SignExtend64<12>(S + A);

    // The signed value must fit in 6 bits and not be zero.
    int64_t hi_imm =
        (static_cast<int64_t>(A + S) - lo_imm) >> 12; //  note, arithmetic shift

    // Check for the LUI instruction that does not use x0 or x2 ( these are not
    // allowed in C.LUI) and 6-bit non-zero offset.
    // TODO: hi_imm == 0 will be relaxed as zero-page.
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
      reloc->setType(eld::ELF::R_RISCV_RVC_LUI);
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

  if (!canRelaxToGP)
    return false;

  Relocation::Type new_type = 0x0;
  switch (type) {
  case llvm::ELF::R_RISCV_LO12_I:
    new_type = ELF::R_RISCV_GPREL_I;
    break;
  case llvm::ELF::R_RISCV_LO12_S:
    new_type = ELF::R_RISCV_GPREL_S;
    break;
  default:
    ASSERT(0, "Unexpected relocation type for RISCV_LUI_GP");
    return false;
  }

  // do relaxation
  uint64_t instr = reloc->target();
  uint64_t mask = 0xF8000;
  instr = (instr & ~mask) | 0x18000;
  reloc->setType(new_type);
  reloc->setTargetData(instr);
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
  return llvm::isInt<12>(X);
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
bool RISCVLDBackend::isGOTReloc(Relocation *reloc) const {
  switch (reloc->type()) {
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
  Relocator::DWord S = getRelocator()->getSymValue(reloc);
  Relocator::DWord A = reloc->addend();

  Relocation::Type new_type = 0x0;
  Relocation::Type type = reloc->type();
  switch (type) {
  case llvm::ELF::R_RISCV_PCREL_LO12_I:
    new_type = ELF::R_RISCV_GPREL_I;
    break;
  case llvm::ELF::R_RISCV_PCREL_LO12_S:
    new_type = ELF::R_RISCV_GPREL_S;
    break;
  default:
    break;
  }

  if (new_type) {
    // Lookup reloc to get actual addend of HI.
    Relocation *HIReloc = m_PairedRelocs[reloc];
    // If this is a GOT relocation, we cannot convert
    // this relative to GP.
    if (isGOTReloc(HIReloc))
      return false;
    if (!HIReloc)
      ASSERT(0, "HIReloc not found! Internal Error!");
    S = getRelocator()->getSymValue(HIReloc);
    A = HIReloc->addend();
    SymbolSize = HIReloc->symInfo()->outSymbol()->size();
  }

  uint64_t offset = reloc->targetRef()->offset();
  bool canRelax = config().options().getRISCVRelax() &&
                  config().options().getRISCVGPRelax() && G != 0 &&
                  fitsInGP(G, S + A, frag, reloc->targetSection(), SymbolSize);

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
  m_PairedRelocs[reloc_jalr] = reloc;
  reloc_jalr->setSymInfo(reloc->symInfo());
  m_InternalRelocs.push_back(reloc_jalr);
}

enum RelaxationPass {
  RELAXATION_CALL, // Must start at zero
  RELAXATION_PC,
  RELAXATION_LUI,
  RELAXATION_ALIGN,
  RELAXATION_PASS_COUNT, // Number of passes
};

void RISCVLDBackend::mayBeRelax(int relaxation_pass, bool &pFinished) {
  pFinished = true;
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

  // Compress
  bool DoCompressed = config().options().getRISCVRelaxToC();

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
      llvm::SmallVectorImpl<Relocation *> &relocList =
          rs->getLink()->getRelocations();
      for (llvm::SmallVectorImpl<Relocation *>::iterator it = relocList.begin();
           it != relocList.end(); ++it) {
        auto relocation = *it;
        // Check if the next relocation is a RELAX relocation.
        Relocation::Type type = relocation->type();
        llvm::SmallVectorImpl<Relocation *>::iterator it2 = it + 1;
        Relocation *nextRelax = nullptr;
        if (it2 != relocList.end()) {
          nextRelax = *it2;
          if (nextRelax->type() != llvm::ELF::R_RISCV_RELAX)
            nextRelax = nullptr;
        }

        // try to relax
        switch (type) {
        case llvm::ELF::R_RISCV_CALL:
        case llvm::ELF::R_RISCV_CALL_PLT: {
          if (nextRelax && relaxation_pass == RELAXATION_CALL)
            doRelaxationCall(relocation, DoCompressed);
          break;
        }
        case llvm::ELF::R_RISCV_PCREL_HI20:
        case llvm::ELF::R_RISCV_PCREL_LO12_I:
        case llvm::ELF::R_RISCV_PCREL_LO12_S: {
          if (nextRelax && relaxation_pass == RELAXATION_PC)
            doRelaxationPC(relocation, GP);
          break;
        }
        case llvm::ELF::R_RISCV_LO12_S:
        case llvm::ELF::R_RISCV_LO12_I:
        case llvm::ELF::R_RISCV_HI20: {
          if (nextRelax && relaxation_pass == RELAXATION_LUI)
            doRelaxationLui(relocation, GP);
          break;
        }
        case llvm::ELF::R_RISCV_ALIGN: {
          if (relaxation_pass == RELAXATION_ALIGN)
            if (doRelaxationAlign(relocation))
              pFinished = false;
          break;
        }
        }
        if (!config().getDiagEngine()->diagnose()) {
          m_Module.setFailure(true);
          pFinished = true;
          return;
        }
      } // for all relocations
    } // for all relocation section
  }

  // On RISC-V, relaxation consists of a fixed number of passes, except
  // R_RISCV_ALIGN will cause another empty pass if it made changes.
  if (relaxation_pass < llvm::ELF::R_RISCV_ALIGN)
    pFinished = false;
}

/// finalizeSymbol - finalize the symbol value
bool RISCVLDBackend::finalizeTargetSymbols() {
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

Relocation *RISCVLDBackend::findHIRelocation(ELFSection *S, uint64_t Value) {
  Relocation *HIReloc = S->findRelocation(Value, llvm::ELF::R_RISCV_PCREL_HI20);
  if (HIReloc)
    return HIReloc;
  HIReloc = S->findRelocation(Value, llvm::ELF::R_RISCV_GOT_HI20);
  if (HIReloc)
    return HIReloc;
  HIReloc = S->findRelocation(Value, llvm::ELF::R_RISCV_TLS_GD_HI20);
  if (HIReloc)
    return HIReloc;
  HIReloc = S->findRelocation(Value, llvm::ELF::R_RISCV_TLS_GOT_HI20);
  if (HIReloc)
    return HIReloc;
  return nullptr;
}

bool RISCVLDBackend::handleRelocation(ELFSection *pSection,
                                      Relocation::Type pType, LDSymbol &pSym,
                                      uint32_t pOffset,
                                      Relocation::Address pAddend,
                                      bool pLastVisit) {
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
    else {
      m_PairedRelocs.insert(std::make_pair(reloc, offsetToReloc->second));
    }
    return true;
  }
  // R_RISCV_PCREL_LO* relocations have the corresponding HI reloc as the
  // syminfo, we need to find out the actual target by inspecting this reloc
  // and set the appropriate relocation.
  case llvm::ELF::R_RISCV_PCREL_LO12_I:
  case llvm::ELF::R_RISCV_PCREL_LO12_S: {
    Relocation *hi_reloc = findHIRelocation(pSection, pSym.value());
    if (!hi_reloc && pLastVisit) {
      config().raise(Diag::rv_hi20_not_found)
          << pSym.name() << getRISCVRelocName(pType)
          << pSection->originalInput()->getInput()->decoratedPath();
      m_Module.setFailure(true);
      return false;
    }
    if (!hi_reloc) {
      // We might be seeing a pcrel_lo with a forward reference to pcrel_hi.
      // Add this to the pending relocations so that it can be revisited again
      // after processing the entire relocation table once.
      m_PendingRelocations.push_back(
          std::make_tuple(pSection, pType, &pSym, pOffset, pAddend));
      return true;
    }
    if (pAddend) {
      config().raise(Diag::warn_ignore_pcrel_lo_addend)
          << pSym.name() << getRISCVRelocName(pType)
          << pSection->originalInput()->getInput()->decoratedPath();
      pAddend = 0;
    }
    Relocation *reloc = IRBuilder::addRelocation(
        getRelocator(), pSection, pType, *hi_reloc->symInfo()->outSymbol(),
        pOffset, pAddend);
    m_PairedRelocs[reloc] = hi_reloc;
    if (reloc) {
      reloc->setSymInfo(hi_reloc->symInfo());
      pSection->addRelocation(reloc);
    }
    if (pLastVisit) {
      // Disable GP Relaxation for this pair to mimic GNU
      m_DisableGPRelocs.insert(reloc);
      m_DisableGPRelocs.insert(hi_reloc);
    }
    return true;
  }
  default: {
    using namespace eld::ELF::riscv;

    // Handle R_RISCV_CUSTOM<n> relocations with their paired R_RISCV_VENDOR
    // relocation - by trying to find the relevant vendor symbol, and using
    // that to translate them into their relevant internal relocation type.
    if (pType >= internal::FirstNonstandardRelocation &&
        pType <= internal::LastNonstandardRelocation) {
      Relocation *VendorReloc =
          pSection->findRelocation(pOffset, llvm::ELF::R_RISCV_VENDOR);
      if (!VendorReloc) {
        // The ABI requires that R_RISCV_VENDOR precedes any R_RISCV_CUSTOM<n>
        // Relocation.
        config().raise(Diag::error_rv_vendor_not_found)
            << getRISCVRelocName(pType)
            << pSection->originalInput()->getInput()->decoratedPath();
        m_Module.setFailure(true);
        return false;
      }

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
      if (handleVendorRelocation(pSection, InternalType, pSym, pOffset, pAddend,
                                 pLastVisit))
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

  if (m_PendingRelocations.empty())
    return true;

  for (auto &r : m_PendingRelocations)
    if (!handleRelocation(std::get<0>(r), std::get<1>(r), *(std::get<2>(r)),
                          std::get<3>(r), std::get<4>(r), /*pLastVisit*/ true))
      return false;

  // Sort the relocation table, in offset order, since the pending relocations
  // that got added at end of the relocation table may not be in offset order.
  std::stable_sort(section->getRelocations().begin(),
                   section->getRelocations().end(),
                   [](Relocation *A, Relocation *B) {
                     return A->getOffset() < B->getOffset();
                   });

  m_PendingRelocations.clear();

  return true;
}

bool RISCVLDBackend::handleVendorRelocation(ELFSection *pSection,
                                            Relocation::Type pType,
                                            LDSymbol &pSym, uint32_t pOffset,
                                            Relocation::Address pAddend,
                                            bool pLastVisit) {
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

void RISCVLDBackend::doPreLayout() {
  m_psdata = m_Module.getScript().sectionMap().find(".sdata");
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
  if (ELFSection *S = getRelaPatch()) {
    S->setSize(S->getRelocations().size() * getRelaEntrySize());
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

bool RISCVLDBackend::finalizeScanRelocations() {
  Fragment *frag = nullptr;
  if (auto *GOT = getGOT())
    if (GOT->hasSectionData())
      frag = *GOT->getFragmentList().begin();
  if (frag)
    defineGOTSymbol(*frag);
  return true;
}

// Create GOT entry.
RISCVGOT *RISCVLDBackend::createGOT(GOT::GOTType T, ELFObjectFile *Obj,
                                    ResolveInfo *R) {

  if (R != nullptr && ((config().options().isSymbolTracingRequested() &&
                        config().options().traceSymbol(*R)) ||
                       m_Module.getPrinter()->traceDynamicLinking()))
    config().raise(Diag::create_got_entry) << R->name();
  // If we are creating a GOT, always create a .got.plt.
  if (!getGOTPLT()->getFragmentList().size()) {
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
RISCVPLT *RISCVLDBackend::createPLT(ELFObjectFile *Obj, ResolveInfo *R) {
  bool is32Bits = config().targets().is32Bits();
  if ((config().options().isSymbolTracingRequested() &&
       config().options().traceSymbol(*R)) ||
      m_Module.getPrinter()->traceDynamicLinking())
    config().raise(Diag::create_plt_entry) << R->name();
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
      if (getPLT()->getFragmentList().empty())
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
    // Create a dynamic relocation for the GOTPLT slot.
    Relocation *dynRel =
        Relocation::Create(llvm::ELF::R_RISCV_JUMP_SLOT, is32Bits ? 32 : 64,
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
  eld::LayoutPrinter *P = m_Module.getLayoutPrinter();
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
      getModule().getLayoutPrinter()->registerStats(O, R);
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
  for (auto &RelocPair : m_PairedRelocs) {
    Relocation *pReloc = RelocPair.first;
    if (pReloc->type() != llvm::ELF::R_RISCV_SUB_ULEB128)
      continue;
    FragmentRef::Offset Off = pReloc->targetRef()->getOutputOffset(m_Module);
    if (Off >= pReloc->targetRef()->getOutputELFSection()->size())
      continue;
    size_t out_offset =
        pReloc->targetRef()->getOutputELFSection()->offset() + Off;
    uint8_t *target_addr = pOutput.getBufferStart() + out_offset;
    if (!overwriteLEB128(target_addr, pReloc->target())) {
      pReloc->issueOverflow(*getRelocator());
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
