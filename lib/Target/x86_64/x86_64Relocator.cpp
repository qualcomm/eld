//===- x86_64Relocator.cpp-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#include "eld/Config/GeneralOptions.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/Support/MsgHandling.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/Target/ELFFileFormat.h"
#include "eld/Target/ELFSegmentFactory.h"
#include "x86_64PLT.h"
#include "x86_64RelocationFunctions.h"
#include "llvm/ADT/Twine.h"
#include "llvm/BinaryFormat/ELF.h"

using namespace eld;

//===--------------------------------------------------------------------===//
// x86_64Relocator
//===--------------------------------------------------------------------===//
x86_64Relocator::x86_64Relocator(x86_64LDBackend &pParent,
                                 LinkerConfig &pConfig, Module &pModule)
    : Relocator(pConfig, pModule), m_Target(pParent) {
  // Mark force verify bit for specified relcoations
  if (m_Module.getPrinter()->verifyReloc() &&
      config().options().verifyRelocList().size()) {
    auto &list = config().options().verifyRelocList();
    for (auto &i : x86RelocDesc) {
      auto RelocInfo = x86_64Relocs[i.type];
      if (list.find(RelocInfo.Name) != list.end())
        i.forceVerify = true;
    }
  }
}

x86_64Relocator::~x86_64Relocator() {}

Relocator::Result x86_64Relocator::applyRelocation(Relocation &pRelocation) {
  Relocation::Type type = pRelocation.type();

  ResolveInfo *symInfo = pRelocation.symInfo();

  if (type > x86_64_MAXRELOCS)
    return Relocator::Unknown;

  if (symInfo) {
    LDSymbol *outSymbol = symInfo->outSymbol();
    if (outSymbol && outSymbol->hasFragRef()) {
      ELFSection *S = outSymbol->fragRef()->frag()->getOwningSection();
      if (S->isDiscard() ||
          (S->getOutputSection() && S->getOutputSection()->isDiscard())) {
        std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
        issueUndefRef(pRelocation, *S->getInputFile(), S);
        return Relocator::OK;
      }
    }
  }

  // apply the relocation
  return x86RelocDesc[type].func(pRelocation, *this, x86RelocDesc[type]);
}

const char *x86_64Relocator::getName(Relocation::Type pType) const {

  return x86_64Relocs[pType].Name;
}

Relocator::Size x86_64Relocator::getSize(Relocation::Type pType) const {
  return x86_64Relocs[pType].Size;
}

// Check if the relocation is invalid
bool x86_64Relocator::isInvalidReloc(Relocation &pReloc) const {

  switch (pReloc.type()) {
  case llvm::ELF::R_X86_64_NONE:
  case llvm::ELF::R_X86_64_64:
  case llvm::ELF::R_X86_64_PC32:
  case llvm::ELF::R_X86_64_COPY:
  case llvm::ELF::R_X86_64_32:
  case llvm::ELF::R_X86_64_32S:
  case llvm::ELF::R_X86_64_16:
  case llvm::ELF::R_X86_64_PC16:
  case llvm::ELF::R_X86_64_8:
  case llvm::ELF::R_X86_64_PC8:
  case llvm::ELF::R_X86_64_PC64:
  case llvm::ELF::R_X86_64_PLT32:
  case llvm::ELF::R_X86_64_GOTPCREL:
  case llvm::ELF::R_X86_64_GOTPCRELX:
  case llvm::ELF::R_X86_64_REX_GOTPCRELX:
  case llvm::ELF::R_X86_64_TPOFF32:
  case llvm::ELF::R_X86_64_TPOFF64:
  case llvm::ELF::R_X86_64_DTPOFF32:
  case llvm::ELF::R_X86_64_DTPOFF64:
  case llvm::ELF::R_X86_64_GOTTPOFF:
  case llvm::ELF::R_X86_64_TLSGD:
    return false;
  default:
    return true; // Other Relocations are not supported as of now
  }
}

void x86_64Relocator::scanRelocation(Relocation &pReloc,
                                     eld::IRBuilder &pLinker,
                                     ELFSection &pSection,
                                     InputFile &pInputFile,
                                     CopyRelocs &CopyRelocs) {
  if (LinkerConfig::Object == config().codeGenType())
    return;

  // If we are generating a shared library check for invalid relocations
  if (isInvalidReloc(pReloc)) {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    ::llvm::outs() << getName(pReloc.type()) << " not supported currently\n";
    m_Target.getModule().setFailure(true);
    return;
  }

  // rsym - The relocation target symbol
  ResolveInfo *rsym = pReloc.symInfo();
  assert(nullptr != rsym &&
         "ResolveInfo of relocation not set while scanRelocation");

  // Check if we are tracing relocations.
  if (m_Module.getPrinter()->traceReloc()) {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    std::string relocName = getName(pReloc.type());
    if (config().options().traceReloc(relocName))
      config().raise(Diag::reloc_trace)
          << relocName << pReloc.symInfo()->name()
          << pInputFile.getInput()->decoratedPath();
  }

  // check if we should issue undefined reference for the relocation target
  // symbol
  if (rsym->isUndef() || rsym->isBitCode()) {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    if (!m_Target.canProvideSymbol(rsym)) {
      if (m_Target.canIssueUndef(rsym)) {
        if (rsym->visibility() != ResolveInfo::Default)
          issueInvisibleRef(pReloc, pInputFile);
        issueUndefRef(pReloc, pInputFile, &pSection);
      }
    }
  }
  ELFSection *section = pSection.getLink()
                            ? pSection.getLink()
                            : pReloc.targetRef()->frag()->getOwningSection();

  if (!section->isAlloc())
    return;

  if (rsym->isLocal()) // rsym is local
    scanLocalReloc(pInputFile, pReloc, pLinker, *section);
  else // rsym is external
    scanGlobalReloc(pInputFile, pReloc, pLinker, *section, CopyRelocs);
}

namespace {

Relocation *helper_DynRel_init(ELFObjectFile *Obj, Relocation *R,
                               ResolveInfo *pSym, Fragment *F, uint32_t pOffset,
                               Relocator::Type pType, x86_64LDBackend &B) {
  Relocation *rela_entry = Obj->getRelaDyn()->createOneReloc();

  rela_entry->setType(pType);
  rela_entry->setTargetRef(make<FragmentRef>(*F, pOffset));
  rela_entry->setSymInfo(pSym);

  if (pType == llvm::ELF::R_X86_64_GLOB_DAT) {
    // Preemptible symbol: dynamic loader resolves the value; addend must be 0.
    rela_entry->setAddend(0);
  } else if (pType == llvm::ELF::R_X86_64_RELATIVE) {
    if (R->type() == llvm::ELF::R_X86_64_64) {
      // Non-preemptible R_X86_64_64 → preserve original addend
      // Writer will compute final: S + A (see emitRela RELATIVE case)
      rela_entry->setAddend(R->addend());
    } else if (R->type() == llvm::ELF::R_X86_64_GOTPCREL ||
               R->type() == llvm::ELF::R_X86_64_GOTPCRELX ||
               R->type() == llvm::ELF::R_X86_64_REX_GOTPCRELX) {
      // Non-preemptible GOT → addend = 0
      // Writer will compute final: S (see emitRela RELATIVE case)
      rela_entry->setAddend(0);
    }
  } else if (pType == llvm::ELF::R_X86_64_TPOFF64 ||
             pType == llvm::ELF::R_X86_64_DTPMOD64 ||
             pType == llvm::ELF::R_X86_64_DTPOFF64) {
    rela_entry->setAddend(0);
  } else if (R) {
    rela_entry->setAddend(R->addend());
  } else {
    rela_entry->setAddend(0);
  }

  if (R && (pType == llvm::ELF::R_X86_64_RELATIVE)) {
    B.recordRelativeReloc(rela_entry, R);
  }
  return rela_entry;
}

// Create a GOT entry and attach appropriate dynamic relocation when needed.
// - Non-dynamic case (!pHasRel): set link-time content to the symbol value.
// - PIC/PIE: use RELATIVE for non-preemptible, GLOB_DAT for preemptible.
x86_64GOT &CreateGOT(ELFObjectFile *Obj, Relocation &pReloc, bool pHasRel,
                     x86_64LDBackend &B) {
  ResolveInfo *rsym = pReloc.symInfo();
  x86_64GOT *G = B.createGOT(GOT::Regular, Obj, rsym);
  if (!pHasRel) {
    // Write link-time content into GOT for static/non-dynamic case.
    G->setValueType(GOT::SymbolValue);
    return *G;
  }
  bool useRelative = !B.isSymbolPreemptible(*rsym);
  helper_DynRel_init(Obj, &pReloc, rsym, G, 0x0,
                     useRelative ? llvm::ELF::R_X86_64_RELATIVE
                                 : llvm::ELF::R_X86_64_GLOB_DAT,
                     B);
  return *G;
}

} // namespace

void x86_64Relocator::scanLocalReloc(InputFile &pInputFile, Relocation &pReloc,
                                     eld::IRBuilder &pBuilder,
                                     ELFSection &pSection) {
  ELFObjectFile *Obj = llvm::dyn_cast<ELFObjectFile>(&pInputFile);
  // rsym - The relocation target symbol
  ResolveInfo *rsym = pReloc.symInfo();
  switch (pReloc.type()) {
  case llvm::ELF::R_X86_64_64:
    if (config().isCodeIndep()) {
      std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
      rsym->setReserved(rsym->reserved() | ReserveRel);
      getTarget().checkAndSetHasTextRel(pSection);
      helper_DynRel_init(Obj, &pReloc, rsym, pReloc.targetRef()->frag(),
                         pReloc.targetRef()->offset(),
                         llvm::ELF::R_X86_64_RELATIVE, m_Target);
    }
    return;
  default:
    break;
  }
}

void x86_64Relocator::scanGlobalReloc(InputFile &pInputFile, Relocation &pReloc,
                                      eld::IRBuilder &pBuilder,
                                      ELFSection &pSection, CopyRelocs &) {

  ELFObjectFile *Obj = llvm::dyn_cast<ELFObjectFile>(&pInputFile);
  // rsym - The relocation target symbol
  ResolveInfo *rsym = pReloc.symInfo();

  switch (pReloc.type()) {
  case llvm::ELF::R_X86_64_64: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    bool isSymbolPreemptible = m_Target.isSymbolPreemptible(*rsym);
    if (getTarget().symbolNeedsDynRel(*rsym, (rsym->reserved() & ReservePLT),
                                      true)) {
      rsym->setReserved(rsym->reserved() | ReserveRel);
      getTarget().checkAndSetHasTextRel(pSection);
      helper_DynRel_init(Obj, &pReloc, rsym, pReloc.targetRef()->frag(),
                         pReloc.targetRef()->offset(),
                         isSymbolPreemptible ? llvm::ELF::R_X86_64_64
                                             : llvm::ELF::R_X86_64_RELATIVE,
                         m_Target);
    }
    return;
  }
  case llvm::ELF::R_X86_64_PLT32: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    if (!m_Target.isSymbolPreemptible(*rsym))
      return;
    if (!(rsym->reserved() & ReservePLT)) {
      m_Target.createPLT(Obj, rsym);
      rsym->setReserved(rsym->reserved() | ReservePLT);
    }
    return;
  }

  case llvm::ELF::R_X86_64_GOTPCREL:
  case llvm::ELF::R_X86_64_GOTPCRELX:
  case llvm::ELF::R_X86_64_REX_GOTPCRELX: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    if (rsym->reserved() & ReserveGOT)
      return;
    CreateGOT(Obj, pReloc, !config().isCodeStatic(), m_Target);
    rsym->setReserved(rsym->reserved() | ReserveGOT);
    return;
  }
  case llvm::ELF::R_X86_64_GOTTPOFF: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    if (rsym->reserved() & ReserveGOT)
      return;
    x86_64GOT *G = m_Target.createGOT(GOT::TLS_IE, Obj, rsym);
    const bool isExec = (config().codeGenType() == LinkerConfig::Exec);
    const bool preemptible = m_Target.isSymbolPreemptible(*rsym);
    if (isExec && !preemptible) {
      G->setValueType(GOT::TLSStaticSymbolValue);
    } else {
      helper_DynRel_init(Obj, &pReloc, rsym, G, 0x0,
                         llvm::ELF::R_X86_64_TPOFF64, m_Target);
    }
    rsym->setReserved(rsym->reserved() | ReserveGOT);
    return;
  }
  case llvm::ELF::R_X86_64_TLSGD: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    if (rsym->reserved() & ReserveGOT)
      return;

    // Create GD GOT pair (x86_64GDGOT creates both entries)
    x86_64GOT *G = m_Target.createGOT(GOT::TLS_GD, Obj, rsym);

    // Always emit DTPMOD64 for first entry (module ID unknown for DSO)
    helper_DynRel_init(Obj, &pReloc, rsym, G->getFirst(), 0x0,
                       llvm::ELF::R_X86_64_DTPMOD64, m_Target);

    // Check if symbol is preemptible to decide on second entry
    if (m_Target.isSymbolPreemptible(*rsym)) {
      // Preemptible: emit DTPOFF64 (dynamic loader fills it)
      helper_DynRel_init(Obj, &pReloc, rsym, G->getNext(), 0x0,
                         llvm::ELF::R_X86_64_DTPOFF64, m_Target);
    } else {
      // Non-preemptible: fill second entry at link time
      G->getNext()->setValueType(GOT::TLSStaticSymbolValue);
    }

    rsym->setReserved(rsym->reserved() | ReserveGOT);
    return;
  }
  default:
    break;

  } // end of switch
}

void x86_64Relocator::defineSymbolforGuard(eld::IRBuilder &pBuilder,
                                           ResolveInfo *pSym,
                                           x86_64LDBackend &pTarget) {
  return;
}

void x86_64Relocator::partialScanRelocation(Relocation &pReloc,
                                            const ELFSection &pSection) {
  pReloc.updateAddend(module());

  // if we meet a section symbol
  if (pReloc.symInfo()->type() == ResolveInfo::Section) {
    LDSymbol *input_sym = pReloc.symInfo()->outSymbol();

    // 1. update the relocation target offset
    assert(input_sym->hasFragRef());
    // 2. get the output ELFSection which the symbol defined in
    ELFSection *out_sect = input_sym->fragRef()->getOutputELFSection();

    ResolveInfo *sym_info = m_Module.getSectionSymbol(out_sect);
    // set relocation target symbol to the output section symbol's resolveInfo
    pReloc.setSymInfo(sym_info);
  }
}

uint32_t x86_64Relocator::getNumRelocs() const { return x86_64_MAXRELOCS; }

//=========================================//
// Relocation Verifier
//=========================================//
template <typename T>
Relocator::Result VerifyRelocAsNeededHelper(
    Relocation &pReloc, T Result, const RelocationDescription &pRelocDesc,
    DiagnosticEngine *DiagEngine, const GeneralOptions &options,
    x86_64Relocator &Parent) {
  uint32_t RelocType = pReloc.type();
  auto RelocInfo = x86_64Relocs[RelocType];
  Relocator::Result R = Relocator::OK;

  auto PreShift = Result;
  Result >>= x86_64Relocs[RelocType].Shift;

  if (RelocInfo.VerifyRange && !verifyRangeX86_64(RelocInfo, Result)) {
    unsigned EffectiveBits =
        getNumberOfBits(RelocInfo.EncType) + RelocInfo.Shift;
    if (RelocInfo.IsSigned)
      return checkSignedRange(pReloc, Parent, PreShift, EffectiveBits);
    return checkUnsignedRange(pReloc, Parent, PreShift, EffectiveBits);
  }

  if ((pRelocDesc.forceVerify) && (isTruncatedX86_64(RelocInfo, Result))) {
    DiagEngine->raise(Diag::reloc_truncated)
        << RelocInfo.Name << pReloc.symInfo()->name()
        << pReloc.getTargetPath(options) << pReloc.getSourcePath(options);
  }
  return R;
}

void x86_64Relocator::computeTLSOffsets() {
  std::vector<ELFSegment *> tlsSegments =
      getTarget().elfSegmentTable().getSegments(llvm::ELF::PT_TLS);

  if (tlsSegments.empty()) {
    return;
  }

  ASSERT(tlsSegments.size() == 1,
         "Multiple TLS segments not supported in x86_64 backend");

  ELFSegment *tlsSegment = tlsSegments[0];
  uint64_t templateSize = tlsSegment->memsz();
  uint64_t alignment = tlsSegment->align();
  templateSize = llvm::alignTo(templateSize, alignment);
  GNULDBackend::setTLSTemplateSize(templateSize);
}

template <typename T>
Relocator::Result ApplyReloc(Relocation &pReloc, T Result,
                             const RelocationDescription &pRelocDesc,
                             DiagnosticEngine *DiagEngine,
                             const GeneralOptions &options,
                             x86_64Relocator &Parent) {
  auto RelocInfo = x86_64Relocs[pReloc.type()];

  // Verify the Relocation.
  Relocator::Result R = Relocator::OK;
  R = VerifyRelocAsNeededHelper(pReloc, Result, pRelocDesc, DiagEngine, options,
                                Parent);
  if (R != Relocator::OK)
    return R;

  // Apply the relocation
  pReloc.target() = doRelocX86_64(RelocInfo, pReloc.target(), Result);
  return R;
}

//=========================================//
// Each relocation function implementation //
//=========================================//
// R_X86_64_NONE
Relocator::Result eld::none(Relocation &pReloc, x86_64Relocator &pParent,
                            RelocationDescription &pRelocDesc) {
  return Relocator::OK;
}

Relocator::Result applyRel(Relocation &pReloc, uint32_t Result,
                           const RelocationDescription &pRelocDesc,
                           DiagnosticEngine *DiagEngine,
                           const GeneralOptions &options,
                           x86_64Relocator &Parent) {
  return ApplyReloc(pReloc, Result, pRelocDesc, DiagEngine, options, Parent);
}

Relocator::Result eld::relocAbs(Relocation &pReloc, x86_64Relocator &pParent,
                                RelocationDescription &pRelocDesc) {
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  ResolveInfo *rsym = pReloc.symInfo();
  Relocator::Address S = pReloc.symValue(pParent.module());
  Relocator::DWord A = pReloc.addend();
  const GeneralOptions &options = pParent.config().options();
  // For absolute relocations, and If we are building a static executable and if
  // the symbol is a weak undefined symbol, it should still use the undefined
  // symbol value which is 0. For non absolute relocations, the call is set to a
  // symbol defined by the linker which returns back to the caller.
  if (rsym && rsym->isWeakUndef() &&
      (pParent.config().codeGenType() == LinkerConfig::Exec)) {
    S = 0;
    return ApplyReloc(pReloc, S + A, pRelocDesc, DiagEngine, options, pParent);
  }

  // if the flag of target section is not ALLOC, we eprform only static
  // relocation.
  if (!pReloc.targetRef()->getOutputELFSection()->isAlloc()) {
    return ApplyReloc(pReloc, S + A, pRelocDesc, DiagEngine, options, pParent);
  }

  if (rsym && (rsym->reserved() & Relocator::ReserveRel)) {
    return Relocator::OK; // Skip writing
  }
  // FIXME PLT STUFF
  //  if (rsym && rsym->reserved() & Relocator::ReservePLT)
  //    S =
  //    pParent.getTarget().findEntryInPLT(rsym)->getAddr(config().getDiagEngine());

  return ApplyReloc(pReloc, S + A, pRelocDesc, DiagEngine, options, pParent);
}

Relocator::Result eld::relocPCREL(Relocation &pReloc, x86_64Relocator &pParent,
                                  RelocationDescription &pRelocDesc) {
  //  ResolveInfo *rsym = pReloc.symInfo();
  uint32_t Result;
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  Relocator::Address S = pReloc.symValue(pParent.module());
  Relocator::DWord A = pReloc.addend();
  Relocator::DWord P = pReloc.place(pParent.module());

  FragmentRef *target_fragref = pReloc.targetRef();
  Fragment *target_frag = target_fragref->frag();
  ELFSection *target_sect = target_frag->getOutputELFSection();

  Result = S + A - P;
  const GeneralOptions &options = pParent.config().options();
  // for relocs inside non ALLOC, just apply
  if (!target_sect->isAlloc()) {
    return applyRel(pReloc, Result, pRelocDesc, DiagEngine, options, pParent);
  }

  // FIXME PLT STUFF
  //  if (!rsym->isLocal()) {
  //    if (rsym->reserved() & Relocator::ReservePLT) {
  //      S =
  //      pParent.getTarget().findEntryInPLT(rsym)->getAddr(config().getDiagEngine());
  //      Result = S + A - P;
  //      applyRel(pReloc, Result, pRelocDesc, DiagEngine);
  //      return Relocator::OK;
  //    }
  //  }

  return applyRel(pReloc, Result, pRelocDesc, DiagEngine, options, pParent);
}

// R_X86_64_PLT32 - PC-relative 32-bit relocation for function calls
// Formula: S + A - P (or PLT_entry + A - P if symbol has PLT)
Relocator::Result eld::relocPLT32(Relocation &pReloc, x86_64Relocator &pParent,
                                  RelocationDescription &pRelocDesc) {
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  ResolveInfo *symInfo = pReloc.symInfo();
  Relocator::Address S;
  if (symInfo->reserved() & Relocator::ReservePLT) {
    // Symbol has PLT entry - redirect through PLT
    x86_64PLT *pltEntry = pParent.getTarget().findEntryInPLT(symInfo);
    S = pltEntry->getAddr(DiagEngine);
  } else {
    // No PLT entry - use direct symbol address
    S = pReloc.symValue(pParent.module());
  }
  // Calculate PC-relative offset: S + A - P
  Relocator::DWord A = pReloc.addend();
  Relocator::DWord P = pReloc.place(pParent.module());
  Relocator::DWord Result = S + A - P;
  return applyRel(pReloc, Result, pRelocDesc, DiagEngine,
                  pParent.config().options(), pParent);
}

Relocator::Result eld::unsupport(Relocation &pReloc, x86_64Relocator &pParent,
                                 RelocationDescription &pRelocDesc) {
  return x86_64Relocator::Unsupport;
}

/// Apply GOT-relative relocations: GOT[S] + A - P
///
/// Unified handler for GOTPCREL, GOTPCRELX, REX_GOTPCRELX,
// and TLS related relocations GOTTPOFF and TLSGD.
/// These relocations share the same application formula but differ in GOT
/// entry type (regular vs TLS) determined during relocation scanning.
Relocator::Result eld::relocGOTRelative(Relocation &pReloc,
                                        x86_64Relocator &pParent,
                                        RelocationDescription &pRelocDesc) {
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  ResolveInfo *symInfo = pReloc.symInfo();
  const GeneralOptions &options = pParent.config().options();

  Relocator::DWord A = pReloc.addend();
  Relocator::DWord P = pReloc.place(pParent.module());
  x86_64GOT *gotEntry = pParent.getTarget().findEntryInGOT(symInfo);
  uint64_t Result = gotEntry->getAddr(DiagEngine) + A - P;

  return applyRel(pReloc, Result, pRelocDesc, DiagEngine, options, pParent);
}

Relocator::Result eld::relocTPOFF(Relocation &pReloc, x86_64Relocator &pParent,
                                  RelocationDescription &pRelocDesc) {
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  const GeneralOptions &options = pParent.config().options();

  uint64_t TLSTemplateSize = pParent.getTarget().getTLSTemplateSize();

  if (TLSTemplateSize == 0) {
    pParent.config().raise(Diag::no_pt_tls_segment);
    return Relocator::BadReloc;
  }

  uint64_t S = pParent.getSymValue(&pReloc);
  Relocator::DWord A = pReloc.addend();
  uint64_t Result = S + A - TLSTemplateSize;

  return ApplyReloc(pReloc, Result, pRelocDesc, DiagEngine, options, pParent);
}

Relocator::Result eld::relocDTPOFF(Relocation &pReloc, x86_64Relocator &pParent,
                                   RelocationDescription &pRelocDesc) {
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  const GeneralOptions &options = pParent.config().options();
  uint64_t S = pParent.getSymValue(&pReloc);
  Relocator::DWord A = pReloc.addend();
  int64_t Result = S + A;
  return ApplyReloc(pReloc, Result, pRelocDesc, DiagEngine, options, pParent);
}
