//===- AArch64Relocator.cpp------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#include "AArch64Relocator.h"
#include "AArch64InsnHelpers.h"
#include "AArch64RelocationFunctions.h"
#include "AArch64RelocationHelpers.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/Object/ObjectBuilder.h"
#include "eld/Support/Memory.h"
#include "eld/Support/MsgHandling.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/Target/ELFFileFormat.h"
#include "llvm/ADT/Twine.h"
#include "llvm/BinaryFormat/ELF.h"
#include <map>

using namespace eld;

namespace {

/// helper_DynRel - Get an relocation entry in .rela.dyn
Relocation *helper_DynRel_init(ELFObjectFile *Obj, Relocation *R,
                               ResolveInfo *pSym, Fragment *F, uint32_t pOffset,
                               Relocator::Type pType,
                               AArch64GNUInfoLDBackend &B) {
  Relocation *rela_entry = nullptr;

  if (pType == R_AARCH64_TLSDESC)
    rela_entry = Obj->getRelaPLT()->createOneReloc();
  else
    rela_entry = Obj->getRelaDyn()->createOneReloc();

  rela_entry->setType(pType);
  rela_entry->setTargetRef(make<FragmentRef>(*F, pOffset));
  rela_entry->setSymInfo(pSym);
  if (R)
    rela_entry->setAddend(R->addend());

  // This is one insane thing, that we need to do. scanRelocations is called
  // rightly before merge sections, so any strings that are merged need to be
  // updated after merge is done to get the right symbol value. Lets record the
  // fact that we created a relative relocation for a relocation that may be
  // pointing to a merge string.
  if (R && (pType == llvm::ELF::R_AARCH64_RELATIVE ||
            pType == llvm::ELF::R_AARCH64_IRELATIVE)) {
    B.recordRelativeReloc(rela_entry, R);
  }

  return rela_entry;
}

AArch64GOT &CreateGOT(ELFObjectFile *Obj, Relocation &pReloc, bool pHasRel,
                      AArch64GNUInfoLDBackend &B, bool isExec) {
  ResolveInfo *rsym = pReloc.symInfo();
  AArch64GOT *G = B.createGOT(GOT::Regular, Obj, rsym);

  if (!pHasRel) {
    G->setValueType(GOT::SymbolValue);
    return *G;
  }

  // If the symbol is not preemptable and we are not building an executable,
  // then try to use a relative reloc. We use a relative reloc if the symbol is
  // hidden otherwise.
  bool useRelative =
      (rsym->isHidden() || (!isExec && !B.isSymbolPreemptible(*rsym)));
  helper_DynRel_init(Obj, &pReloc, rsym, G, 0x0,
                     useRelative ? llvm::ELF::R_AARCH64_RELATIVE
                                 : llvm::ELF::R_AARCH64_GLOB_DAT,
                     B);
  if (useRelative) {
    G->setValueType(GOT::SymbolValue);
  }
  return *G;
}

} // namespace

//===----------------------------------------------------------------------===//
// Relocation Functions and Tables
//===----------------------------------------------------------------------===//
DECL_AARCH64_APPLY_RELOC_FUNCS

/// the prototype of applying function
typedef Relocator::Result (*ApplyFunctionType)(Relocation &pReloc,
                                               AArch64Relocator &pParent);

// the table entry of applying functions
class ApplyFunctionEntry {
public:
  ApplyFunctionEntry() {}
  ApplyFunctionEntry(ApplyFunctionType pFunc, const char *pName,
                     size_t pSize = 0)
      : func(pFunc), name(pName), size(pSize) {}
  ApplyFunctionType func;
  const char *name;
  size_t size;
};
typedef std::map<Relocator::Type, ApplyFunctionEntry> ApplyFunctionMap;

static const ApplyFunctionMap::value_type ApplyFunctionList[] = {
    DECL_AARCH64_APPLY_RELOC_FUNC_PTRS(ApplyFunctionMap::value_type,
                                       ApplyFunctionEntry)};

// declare the table of applying functions
static ApplyFunctionMap ApplyFunctions(ApplyFunctionList,
                                       ApplyFunctionList +
                                           sizeof(ApplyFunctionList) /
                                               sizeof(ApplyFunctionList[0]));

//===----------------------------------------------------------------------===//
// AArch64Relocator
//===----------------------------------------------------------------------===//
AArch64Relocator::AArch64Relocator(AArch64GNUInfoLDBackend &pParent,
                                   LinkerConfig &pConfig, Module &pModule)
    : Relocator(pConfig, pModule), m_Target(pParent) {}

AArch64Relocator::~AArch64Relocator() {}

bool AArch64Relocator::isInvalidReloc(Relocation &pReloc) const {
  if (!config().isCodeIndep())
    return false;
  switch (pReloc.type()) {
  case llvm::ELF::R_AARCH64_ABS32:
  case llvm::ELF::R_AARCH64_ABS16:
  case llvm::ELF::R_AARCH64_TLSIE_LD64_GOTTPREL_LO12_NC:
  case llvm::ELF::R_AARCH64_TLSLE_LDST64_TPREL_LO12_NC:
  case llvm::ELF::R_AARCH64_TLSLE_ADD_TPREL_LO12_NC:
  case llvm::ELF::R_AARCH64_TLSLE_ADD_TPREL_LO12:
  case llvm::ELF::R_AARCH64_TLSLE_ADD_TPREL_HI12:
    return true;
  default:
    break;
  }
  return false;
}

Relocator::Result AArch64Relocator::applyRelocation(Relocation &pRelocation) {
  Relocation::Type type = pRelocation.type();

  // valid types are 0x0, 0x100-0x239
  if ((type < 0x100 || type > 0x239) && (type != 0x0) &&
      (type != R_AARCH64_COPY_INSN))
    return Relocator::Unknown;

  assert(ApplyFunctions.find(type) != ApplyFunctions.end());

  ResolveInfo *symInfo = pRelocation.symInfo();

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

  return ApplyFunctions[type].func(pRelocation, *this);
}

const char *AArch64Relocator::getName(Relocator::Type pType) const {
  assert(ApplyFunctions.find(pType) != ApplyFunctions.end());
  return ApplyFunctions[pType].name;
}

uint32_t AArch64Relocator::getNumRelocs() const { return AARCH64_MAXRELOCS; }

Relocator::Size AArch64Relocator::getSize(Relocation::Type pType) const {
  return ApplyFunctions[pType].size;
}

void AArch64Relocator::scanLocalReloc(InputFile &pInput, Relocation &pReloc,
                                      const ELFSection &pSection) {
  ELFObjectFile *Obj = llvm::dyn_cast<ELFObjectFile>(&pInput);
  // rsym - The relocation target symbol
  ResolveInfo *rsym = pReloc.symInfo();
  switch (pReloc.type()) {
  case llvm::ELF::R_AARCH64_ABS64:
    // If buiding PIC object (shared library or PIC executable),
    // a dynamic relocations with RELATIVE type to this location is needed.
    // Reserve an entry in .rel.dyn
    if (config().isCodeIndep()) {
      std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
      // set Rel bit
      rsym->setReserved(rsym->reserved() | ReserveRel);
      getTarget().checkAndSetHasTextRel(pSection);
      // set up the dyn rel directly
      helper_DynRel_init(Obj, &pReloc, rsym, pReloc.targetRef()->frag(),
                         pReloc.targetRef()->offset(), R_AARCH64_RELATIVE,
                         m_Target);
    }
    return;

  case llvm::ELF::R_AARCH64_ABS32:
  case llvm::ELF::R_AARCH64_ABS16:
    // If buiding PIC object (shared library or PIC executable),
    // a dynamic relocations with RELATIVE type to this location is needed.
    // Reserve an entry in .rel.dyn
    if (config().isCodeIndep()) {
      std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
      // set up the dyn rel directly
      helper_DynRel_init(Obj, &pReloc, rsym, pReloc.targetRef()->frag(),
                         pReloc.targetRef()->offset(), pReloc.type(), m_Target);
      // set Rel bit
      rsym->setReserved(rsym->reserved() | ReserveRel);
      getTarget().checkAndSetHasTextRel(pSection);
    }
    return;

  case llvm::ELF::R_AARCH64_ADR_GOT_PAGE:
  case llvm::ELF::R_AARCH64_LD64_GOT_LO12_NC: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    // Symbol needs GOT entry, reserve entry in .got
    // return if we already create GOT for this symbol
    if (rsym->reserved() & ReserveGOT)
      return;
    // If building PIC object, a dynamic relocation with
    // type RELATIVE is needed to relocate this GOT entry.
    CreateGOT(Obj, pReloc, config().isCodeIndep(), m_Target,
              config().codeGenType() == LinkerConfig::Exec);
    // set GOT bit
    rsym->setReserved(rsym->reserved() | ReserveGOT);
    return;
  }

  case llvm::ELF::R_AARCH64_TLSIE_ADR_GOTTPREL_PAGE21:
  case llvm::ELF::R_AARCH64_TLSIE_LD64_GOTTPREL_LO12_NC: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    if (rsym->reserved() & ReserveGOT)
      return;

    // Dont use a GOT, convert the instruction.
    if (config().isCodeStatic())
      return;
    AArch64GOT *G = m_Target.createGOT(GOT::TLS_IE, Obj, rsym);
    helper_DynRel_init(Obj, &pReloc, rsym, G, 0x0,
                       llvm::ELF::R_AARCH64_TLS_TPREL64, m_Target);
    if (rsym->reserved() == Relocator::None)
      rsym->setReserved(rsym->reserved() | ReserveGOT);
    return;
  }

  case llvm::ELF::R_AARCH64_TLSDESC_ADR_PAGE21:
  case llvm::ELF::R_AARCH64_TLSDESC_LD64_LO12:
  case llvm::ELF::R_AARCH64_TLSDESC_ADD_LO12: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);

    // Symbol needs GOT entry, reserve entry in .got
    // return if we already create GOT for this symbol
    if (rsym->reserved() & ReserveGOT)
      return;

    if (config().isCodeStatic()) {
      AArch64GOT *G = m_Target.createGOT(GOT::TLS_IE, Obj, rsym);
      rsym->setReserved(rsym->reserved() | ReserveGOT);
      G->setValueType(GOT::TLSStaticSymbolValue);
      return;
    }
    AArch64GOT *G = m_Target.createGOT(GOT::TLS_DESC, Obj, rsym);
    helper_DynRel_init(Obj, &pReloc, rsym, G->getFirst(), 0x0,
                       llvm::ELF::R_AARCH64_TLSDESC, m_Target);
    if (rsym->reserved() == Relocator::None)
      rsym->setReserved(rsym->reserved() | ReserveGOT);
    return;
  }

  default:
    break;
  }
}

void AArch64Relocator::scanGlobalReloc(InputFile &pInput, Relocation &pReloc,
                                       eld::IRBuilder &pBuilder,
                                       ELFSection &pSection,
                                       CopyRelocs &CopyRelocs) {
  ELFObjectFile *Obj = llvm::dyn_cast<ELFObjectFile>(&pInput);
  // rsym - The relocation target symbol
  ResolveInfo *rsym = pReloc.symInfo();

  switch (pReloc.type()) {
  case llvm::ELF::R_AARCH64_ABS16:
  case llvm::ELF::R_AARCH64_ABS32:
  case llvm::ELF::R_AARCH64_ABS64: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    // Absolute relocation type, symbol may needs PLT entry or
    // dynamic relocation entry
    bool isSymbolPreemptible = m_Target.isSymbolPreemptible(*rsym);
    if (isSymbolPreemptible && (rsym->type() == ResolveInfo::Function)) {
      // create plt for this symbol if it does not have one
      if (!(rsym->reserved() & ReservePLT)) {
        // Symbol needs PLT entry, we need a PLT entry
        // and the corresponding GOT and dynamic relocation entry
        // in .got and .rel.plt.
        m_Target.createPLT(Obj, rsym);
        // set PLT bit
        rsym->setReserved(rsym->reserved() | ReservePLT);
      }
    }

    if (getTarget().symbolNeedsDynRel(
            *rsym, (rsym->reserved() & ReservePLT),
            (pReloc.type() == llvm::ELF::R_AARCH64_ABS64))) {
      // symbol needs dynamic relocation entry, set up the dynrel entry
      if (getTarget().symbolNeedsCopyReloc(pReloc, *rsym)) {
        // check if the option -z nocopyreloc is given
        if (config().options().hasNoCopyReloc()) {
          config().raise(Diag::copyrelocs_is_error)
              << rsym->name() << pInput.getInput()->decoratedPath()
              << rsym->resolvedOrigin()->getInput()->decoratedPath();
          return;
        }
        CopyRelocs.insert(rsym);
      } else {
        // set Rel bit and the dyn rel
        rsym->setReserved(rsym->reserved() | ReserveRel);
        getTarget().checkAndSetHasTextRel(pSection);
        helper_DynRel_init(
            Obj, &pReloc, rsym, pReloc.targetRef()->frag(),
            pReloc.targetRef()->offset(),
            isSymbolPreemptible ? pReloc.type() : R_AARCH64_RELATIVE, m_Target);
      }
    }
  }
    return;

  case llvm::ELF::R_AARCH64_PREL64:
  case llvm::ELF::R_AARCH64_PREL32:
  case llvm::ELF::R_AARCH64_PREL16: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    bool isSymbolPreemptible = m_Target.isSymbolPreemptible(*rsym);
    if (isSymbolPreemptible) {
      if ((rsym->type() == ResolveInfo::Function) &&
          LinkerConfig::DynObj != config().codeGenType()) {
        // create plt for this symbol if it does not have one
        if (!(rsym->reserved() & ReservePLT)) {
          // Symbol needs PLT entry, we need a PLT entry
          // and the corresponding GOT and dynamic relocation entry
          // in .got and .rel.plt.
          m_Target.createPLT(Obj, rsym);
          // set PLT bit
          rsym->setReserved(rsym->reserved() | ReservePLT);
        }
      }
    }

    if (getTarget().symbolNeedsDynRel(*rsym, (rsym->reserved() & ReservePLT),
                                      false) &&
        getTarget().symbolNeedsCopyReloc(pReloc, *rsym)) {
      // check if the option -z nocopyreloc is given
      if (config().options().hasNoCopyReloc()) {
        config().raise(Diag::copyrelocs_is_error)
            << rsym->name() << pInput.getInput()->decoratedPath()
            << rsym->resolvedOrigin()->getInput()->decoratedPath();
        return;
      }
      CopyRelocs.insert(rsym);
    }
    return;
  }

  case llvm::ELF::R_AARCH64_CONDBR19:
  case llvm::ELF::R_AARCH64_JUMP26:
  case llvm::ELF::R_AARCH64_CALL26: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    // return if we already create plt for this symbol
    if (rsym->reserved() & ReservePLT)
      return;

    // create IRELATIVE for IFUNC symbol
    if (rsym->type() == ResolveInfo::IndirectFunc && config().isCodeStatic()) {
      m_Target.createPLT(Obj, rsym, true);
      rsym->setReserved(rsym->reserved() | ReservePLT);
      AArch64GNUInfoLDBackend &backend = getTarget();
      backend.defineIRelativeRange(*rsym);
      return;
    }
    // if symbol is defined in the ouput file and it's not
    // preemptible, no need plt
    if (!getTarget().isSymbolPreemptible(*rsym)) {
      return;
    }

    // Symbol needs PLT entry, we need to reserve a PLT entry
    // and the corresponding GOT and dynamic relocation entry
    // in .got and .rel.plt.
    m_Target.createPLT(Obj, rsym);
    // set PLT bit
    rsym->setReserved(rsym->reserved() | ReservePLT);
    return;
  }

  case llvm::ELF::R_AARCH64_ADR_PREL_PG_HI21:
  case R_AARCH64_ADR_PREL_PG_HI21_NC: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    if (getTarget().symbolNeedsDynRel(*rsym, (rsym->reserved() & ReservePLT),
                                      false)) {
      if (getTarget().symbolNeedsCopyReloc(pReloc, *rsym)) {
        // check if the option -z nocopyreloc is given
        if (config().options().hasNoCopyReloc()) {
          config().raise(Diag::copyrelocs_is_error)
              << rsym->name() << pInput.getInput()->decoratedPath()
              << rsym->resolvedOrigin()->getInput()->decoratedPath();
          return;
        }

        CopyRelocs.insert(rsym);
      }
    }
    bool isSymbolPreemptible = m_Target.isSymbolPreemptible(*rsym);
    if (isSymbolPreemptible) {
      // create plt for this symbol if it does not have one
      if ((rsym->type() == ResolveInfo::Function) &&
          !(rsym->reserved() & ReservePLT)) {
        // Symbol needs PLT entry, we need a PLT entry
        // and the corresponding GOT and dynamic relocation entry
        // in .got and .rel.plt.
        m_Target.createPLT(Obj, rsym);
        // set PLT bit
        rsym->setReserved(rsym->reserved() | ReservePLT);
      }
    }
    return;
  }

  case llvm::ELF::R_AARCH64_ADR_GOT_PAGE:
  case llvm::ELF::R_AARCH64_LD64_GOT_LO12_NC: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    // Symbol needs GOT entry, reserve entry in .got
    // return if we already create GOT for this symbol
    if (rsym->reserved() & ReserveGOT)
      return;
    // if the symbol cannot be fully resolved at link time, then we need a
    // dynamic relocation
    CreateGOT(Obj, pReloc, !config().isCodeStatic(), m_Target,
              config().codeGenType() == LinkerConfig::Exec);
    // set GOT bit
    rsym->setReserved(rsym->reserved() | ReserveGOT);
    return;
  }

  case llvm::ELF::R_AARCH64_TLSIE_ADR_GOTTPREL_PAGE21:
  case llvm::ELF::R_AARCH64_TLSIE_LD64_GOTTPREL_LO12_NC: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    // Symbol needs GOT entry, reserve entry in .got
    // return if we already create GOT for this symbol
    if (rsym->reserved() & ReserveGOT)
      return;

    // set up the got and the corresponding rel entry
    AArch64GOT *G = m_Target.createGOT(GOT::TLS_IE, Obj, rsym);
    if (config().isCodeStatic()) {
      rsym->setReserved(rsym->reserved() | ReserveGOT);
      G->setValueType(GOT::TLSStaticSymbolValue);
      return;
    }
    helper_DynRel_init(Obj, &pReloc, rsym, G, 0x0,
                       llvm::ELF::R_AARCH64_TLS_TPREL64, m_Target);
    rsym->setReserved(rsym->reserved() | ReserveGOT);
    return;
  }

  case llvm::ELF::R_AARCH64_TLSDESC_ADR_PAGE21:
  case llvm::ELF::R_AARCH64_TLSDESC_LD64_LO12:
  case llvm::ELF::R_AARCH64_TLSDESC_ADD_LO12: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);

    // Symbol needs GOT entry, reserve entry in .got
    // return if we already create GOT for this symbol
    if (rsym->reserved() & ReserveGOT)
      return;

    if (config().isCodeStatic()) {
      AArch64GOT *G = m_Target.createGOT(GOT::TLS_IE, Obj, rsym);
      rsym->setReserved(rsym->reserved() | ReserveGOT);
      G->setValueType(GOT::TLSStaticSymbolValue);
      return;
    }
    AArch64GOT *G = m_Target.createGOT(GOT::TLS_DESC, Obj, rsym);
    helper_DynRel_init(Obj, &pReloc, rsym, G->getFirst(), 0x0,
                       llvm::ELF::R_AARCH64_TLSDESC, m_Target);
    rsym->setReserved(rsym->reserved() | ReserveGOT);
    return;
  };

  default:
    break;
  }
}

void AArch64Relocator::partialScanRelocation(Relocation &pReloc,
                                             const ELFSection &pSection) {
  pReloc.updateAddend(m_Module);

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

void AArch64Relocator::scanRelocation(Relocation &pReloc,
                                      eld::IRBuilder &pBuilder,
                                      ELFSection &pSection,
                                      InputFile &pInputFile,
                                      CopyRelocs &CopyRelocs) {
  if (LinkerConfig::Object == config().codeGenType())
    return;

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
  {
    if (rsym->isUndef() || rsym->isBitCode()) {
      std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
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
    scanLocalReloc(pInputFile, pReloc, *section);
  else // rsym is external
    scanGlobalReloc(pInputFile, pReloc, pBuilder, *section, CopyRelocs);
}

//===----------------------------------------------------------------------===//
// Each relocation function implementation
//===----------------------------------------------------------------------===//

// R_AARCH64_NONE
Relocator::Result none(Relocation &pReloc, AArch64Relocator &pParent) {
  return Relocator::OK;
}

Relocator::Result unsupport(Relocation &pReloc, AArch64Relocator &pParent) {
  return Relocator::Unsupport;
}

// R_AARCH64_ABS64: S + A
// R_AARCH64_ABS32: S + A
// R_AARCH64_ABS16: S + A
Relocator::Result abs(Relocation &pReloc, AArch64Relocator &pParent) {
  ResolveInfo *rsym = pReloc.symInfo();
  Relocator::DWord A = pReloc.addend();
  Relocator::DWord S = pParent.getSymValue(&pReloc);

  ELFSection *target_sect = pReloc.targetRef()->getOutputELFSection();
  // If the flag of target section is not ALLOC, we will not scan this
  // relocation but perform static relocation. (e.g., applying .debug section)
  if (!target_sect->isAlloc()) {
    pReloc.target() = S + A;
    return Relocator::OK;
  }

  if (rsym && (rsym->reserved() & Relocator::ReserveRel) &&
      (pParent.getTarget().isSymbolPreemptible(*rsym)))
    return Relocator::OK;

  if (rsym && rsym->reserved() & Relocator::ReservePLT)
    S = pParent.getTarget().findEntryInPLT(rsym)->getAddr(
        pParent.config().getDiagEngine());

  if (rsym && rsym->isWeakUndef() &&
      (pParent.config().codeGenType() == LinkerConfig::Exec))
    S = 0;

  switch (pReloc.type()) {
  case llvm::ELF::R_AARCH64_ABS32:
    if (!llvm::isUInt<32>(S + A) && !llvm::isInt<32>(S + A))
      return Relocator::Overflow;
    break;
  case llvm::ELF::R_AARCH64_ABS16:
    if (!llvm::isUInt<16>(S + A) && !llvm::isInt<16>(S + A))
      return Relocator::Overflow;
    break;
  default:
    break;
  }

  // A local symbol may need RELATIVE Type dynamic relocation
  // perform static relocation
  pReloc.target() = S + A;
  return Relocator::OK;
}

// R_AARCH64_PREL64: S + A - P
// R_AARCH64_PREL32: S + A - P
// R_AARCH64_PREL16: S + A - P
Relocator::Result rel(Relocation &pReloc, AArch64Relocator &pParent) {
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  ResolveInfo *rsym = pReloc.symInfo();
  Relocator::Address S = pParent.getSymValue(&pReloc);
  // similar to how we have handled getSymValue, and geSymInfo.
  // take relocation as an argument.
  Relocator::DWord A = pReloc.addend();
  Relocator::DWord P = pReloc.place(pParent.module());

  if (llvm::ELF::R_AARCH64_PREL64 != pReloc.type())
    A += pReloc.target() & get_mask(pParent.getSize(pReloc.type()));
  else
    A += pReloc.target();

  ELFSection *target_sect = pReloc.targetRef()->getOutputELFSection();
  // If the flag of target section is not ALLOC, we will not scan this
  // relocation but perform static relocation. (e.g., applying .debug section)
  if (target_sect->isAlloc()) {
    // if plt entry exists, the S value is the plt entry address
    if (!rsym->isLocal()) {
      if (rsym->reserved() & Relocator::ReservePLT) {
        S = pParent.getTarget().findEntryInPLT(rsym)->getAddr(DiagEngine);
      }
    }
  }

  Relocator::DWord X = S + A - P;
  pReloc.target() = X;

  if (llvm::ELF::R_AARCH64_PREL64 != pReloc.type() &&
      helper_check_signed_overflow(X, pParent.getSize(pReloc.type())))
    return Relocator::Overflow;
  return Relocator::OK;
}

// R_AARCH64_ADD_ABS_LO12_NC: S + A
Relocator::Result add_abs_lo12(Relocation &pReloc, AArch64Relocator &pParent) {
  Relocator::Address value = 0x0;
  Relocator::Address S = pParent.getSymValue(&pReloc);
  Relocator::DWord A = pReloc.addend();

  // S depends on PLT exists or not
  if (pReloc.symInfo()->reserved() & Relocator::ReservePLT)
    S = pParent.getTarget()
            .findEntryInPLT(pReloc.symInfo())
            ->getAddr(pParent.config().getDiagEngine());

  value = helper_get_page_offset(S + A);
  pReloc.target() = helper_reencode_add_imm(pReloc.target(), value);

  return Relocator::OK;
}

// R_AARCH64_ADR_PREL_PG_HI21: ((PG(S + A) - PG(P)) >> 12)
// R_AARCH64_ADR_PREL_PG_HI21_NC: ((PG(S + A) - PG(P)) >> 12)
Relocator::Result adr_prel_pg_hi21(Relocation &pReloc,
                                   AArch64Relocator &pParent) {
  ResolveInfo *rsym = pReloc.symInfo();
  Relocator::Address S = pParent.getSymValue(&pReloc);
  // if plt entry exists, the S value is the plt entry address
  if (rsym->reserved() & Relocator::ReservePLT) {
    S = pParent.getTarget().findEntryInPLT(rsym)->getAddr(
        pParent.config().getDiagEngine());
  }
  Relocator::DWord A = pReloc.addend();
  Relocator::DWord P = pReloc.place(pParent.module());
  Relocator::DWord X =
      helper_get_page_address(S + A) - helper_get_page_address(P);

  pReloc.target() = helper_reencode_adr_imm(pReloc.target(), (X >> 12));

  return Relocator::OK;
}

// R_AARCH64_ADR_PREL_LO21: (S + A) - P
Relocator::Result adr_prel_lo21(Relocation &pReloc, AArch64Relocator &pParent) {
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  ResolveInfo *rsym = pReloc.symInfo();
  Relocator::Address S = pParent.getSymValue(&pReloc);
  // if plt entry exists, the S value is the plt entry address
  if (rsym->reserved() & Relocator::ReservePLT) {
    S = pParent.getTarget().findEntryInPLT(rsym)->getAddr(DiagEngine);
  }
  Relocator::DWord A = pReloc.addend();
  Relocator::DWord P = pReloc.place(pParent.module());

  Relocator::DWord X = S + A - P;
  // TODO:: check overflow

  pReloc.target() = helper_reencode_adr_imm(pReloc.target(), X);

  return Relocator::OK;
}

// R_AARCH64_LD_PREL_LO19: (S + A) - P
Relocator::Result ld_prel_lo19(Relocation &pReloc, AArch64Relocator &pParent) {
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  ResolveInfo *rsym = pReloc.symInfo();
  Relocator::Address S = pParent.getSymValue(&pReloc);
  // if plt entry exists, the S value is the plt entry address
  if (rsym->reserved() & Relocator::ReservePLT) {
    S = pParent.getTarget().findEntryInPLT(rsym)->getAddr(DiagEngine);
  }
  Relocator::DWord A = pReloc.addend();
  Relocator::DWord P = pReloc.place(pParent.module());

  Relocator::DWord X = S + A - P;
  // TODO:: check overflow

  pReloc.target() = helper_reencode_ld_literal_19(pReloc.target(), X >> 2);

  return Relocator::OK;
}

// R_AARCH64_CALL26: S + A - P
// R_AARCH64_JUMP26: S + A - P
Relocator::Result call(Relocation &pReloc, AArch64Relocator &pParent) {
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  // If target is undefined weak symbol, we only need to jump to the
  // next instruction unless it has PLT entry. Rewrite instruction
  // to NOP.
  if (pReloc.symInfo()->isWeak() && pReloc.symInfo()->isUndef() &&
      !pReloc.symInfo()->isDyn() &&
      !(pReloc.symInfo()->reserved() & Relocator::ReservePLT)) {
    // change target to NOP
    pReloc.target() = 0xd503201f;
    return Relocator::OK;
  }

  Relocator::Address S = pParent.getSymValue(&pReloc);
  Relocator::DWord A = pReloc.addend();
  Relocator::Address P = pReloc.place(pParent.module());

  // S depends on PLT exists or not
  if (pReloc.symInfo()->reserved() & Relocator::ReservePLT)
    S = pParent.getTarget()
            .findEntryInPLT(pReloc.symInfo())
            ->getAddr(DiagEngine);

  Relocator::DWord X = S + A - P;
  // TODO: check overflow..

  pReloc.target() = helper_reencode_branch_offset_26(pReloc.target(), X >> 2);

  return Relocator::OK;
}

// R_AARCH64_CONDBR19: S + A - P
Relocator::Result condbr(Relocation &pReloc, AArch64Relocator &pParent) {
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  // If target is undefined weak symbol, we only need to jump to the
  // next instruction unless it has PLT entry. Rewrite instruction
  // to NOP.
  if (pReloc.symInfo()->isWeak() && pReloc.symInfo()->isUndef() &&
      !pReloc.symInfo()->isDyn() &&
      !(pReloc.symInfo()->reserved() & Relocator::ReservePLT)) {
    // change target to NOP
    pReloc.target() = 0xd503201f;
    return Relocator::OK;
  }

  Relocator::Address S = pParent.getSymValue(&pReloc);
  Relocator::DWord A = pReloc.addend();
  Relocator::Address P = pReloc.place(pParent.module());

  // S depends on PLT exists or not
  if (pReloc.symInfo()->reserved() & Relocator::ReservePLT)
    S = pParent.getTarget()
            .findEntryInPLT(pReloc.symInfo())
            ->getAddr(DiagEngine);

  Relocator::DWord X = S + A - P;
  Relocator::SWord SX = X;

  if (pReloc.type() == llvm::ELF::R_AARCH64_CONDBR19) {
    // check -2^20 <= X < 2^20
    if (SX >= 0x100000)
      return Relocator::Overflow;
    if ((0 - SX) >= 0x100000)
      return Relocator::Overflow;
    pReloc.target() =
        helper_reencode_cond_branch_ofs_19(pReloc.target(), X >> 2);
  } else if (pReloc.type() == llvm::ELF::R_AARCH64_TSTBR14) {
    // check -2^15 <= X < 2^15
    if (SX >= 0x8000)
      return Relocator::Overflow;
    if ((0 - SX) >= 0x8000)
      return Relocator::Overflow;
    pReloc.target() = helper_reencode_tbz_imm_14(pReloc.target(), X >> 2);
  }

  return Relocator::OK;
}

// R_AARCH64_ADR_GOT_PAGE: Page(G(GDAT(S+A))) - Page(P)
Relocator::Result adr_got_page(Relocation &pReloc, AArch64Relocator &pParent) {
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  if (!(pReloc.symInfo()->reserved() & Relocator::ReserveGOT)) {
    return Relocator::BadReloc;
  }

  Relocator::Address GOT_S =
      pParent.getTarget().findEntryInGOT(pReloc.symInfo())->getAddr(DiagEngine);
  Relocator::DWord A = pReloc.addend();
  Relocator::Address P = pReloc.place(pParent.module());
  Relocator::DWord X =
      helper_get_page_address(GOT_S + A) - helper_get_page_address(P);

  pReloc.target() = helper_reencode_adr_imm(pReloc.target(), (X >> 12));

  // setup relocation addend if needed
  return Relocator::OK;
}

// R_AARCH64_LD64_GOT_LO12_NC: G(GDAT(S+A))
Relocator::Result ld64_got_lo12(Relocation &pReloc, AArch64Relocator &pParent) {
  if (!(pReloc.symInfo()->reserved() & Relocator::ReserveGOT)) {
    return Relocator::BadReloc;
  }

  Relocator::Address GOT_S = pParent.getTarget()
                                 .findEntryInGOT(pReloc.symInfo())
                                 ->getAddr(pParent.config().getDiagEngine());
  Relocator::DWord A = pReloc.addend();
  Relocator::DWord X = helper_get_page_offset(GOT_S + A);

  pReloc.target() = helper_reencode_ldst_pos_imm(pReloc.target(), (X >> 3));

  return Relocator::OK;
}

// R_AARCH64_LDST8_ABS_LO12_NC: S + A
// R_AARCH64_LDST16_ABS_LO12_NC: S + A
// R_AARCH64_LDST32_ABS_LO12_NC: S + A
// R_AARCH64_LDST64_ABS_LO12_NC: S + A
// R_AARCH64_LDST128_ABS_LO12_NC: S + A
Relocator::Result ldst_abs_lo12(Relocation &pReloc, AArch64Relocator &pParent) {
  Relocator::Address S = pParent.getSymValue(&pReloc);
  Relocator::DWord A = pReloc.addend();
  Relocator::DWord X = helper_get_page_offset(S + A);

  switch (pReloc.type()) {
  case llvm::ELF::R_AARCH64_LDST8_ABS_LO12_NC:
    pReloc.target() = helper_reencode_ldst_pos_imm(pReloc.target(), X);
    break;
  case llvm::ELF::R_AARCH64_LDST16_ABS_LO12_NC:
    pReloc.target() = helper_reencode_ldst_pos_imm(pReloc.target(), (X >> 1));
    break;
  case llvm::ELF::R_AARCH64_LDST32_ABS_LO12_NC:
    pReloc.target() = helper_reencode_ldst_pos_imm(pReloc.target(), (X >> 2));
    break;
  case llvm::ELF::R_AARCH64_LDST64_ABS_LO12_NC:
    pReloc.target() = helper_reencode_ldst_pos_imm(pReloc.target(), (X >> 3));
    break;
  case llvm::ELF::R_AARCH64_LDST128_ABS_LO12_NC:
    pReloc.target() = helper_reencode_ldst_pos_imm(pReloc.target(), (X >> 4));
    break;
  default:
    break;
  }
  return Relocator::OK;
}

// R_AARCH64_MOVW_UABS_G0: S + A
// R_AARCH64_MOVW_UABS_G0_NC: S + A
// R_AARCH64_MOVW_UABS_G1: S + A
// R_AARCH64_MOVW_UABS_G1_NC: S + A
// R_AARCH64_MOVW_UABS_G2: S + A
// R_AARCH64_MOVW_UABS_G2_NC: S + A
// R_AARCH64_MOVW_UABS_G3: S + A
// R_AARCH64_MOVW_SABS_G0: S + A
// R_AARCH64_MOVW_SABS_G1: S + A
// R_AARCH64_MOVW_SABS_G2: S + A
Relocator::Result movw_abs_g(Relocation &pReloc, AArch64Relocator &pParent) {
  Relocator::Address S = pParent.getSymValue(&pReloc);
  Relocator::DWord A = pReloc.addend();
  Relocator::DWord X = S + A;
  Relocator::SWord SX = X;

  switch (pReloc.type()) {
  case llvm::ELF::R_AARCH64_MOVW_UABS_G0:
    // Check 0 <= X < 2^16
    if (X >= 0x10000)
      return Relocator::Overflow;
    LLVM_FALLTHROUGH;
  case llvm::ELF::R_AARCH64_MOVW_UABS_G0_NC:
    pReloc.target() = helper_reencode_movzk_imm(pReloc.target(), (X & 0xFFFF));
    break;
  case llvm::ELF::R_AARCH64_MOVW_UABS_G1:
    // Check 0 <= X < 2^32
    if (X >= 0x100000000)
      return Relocator::Overflow;
    LLVM_FALLTHROUGH;
  case llvm::ELF::R_AARCH64_MOVW_UABS_G1_NC:
    pReloc.target() =
        helper_reencode_movzk_imm(pReloc.target(), ((X >> 16) & 0xFFFF));
    break;
  case llvm::ELF::R_AARCH64_MOVW_UABS_G2:
    // Check 0 <= X < 2^48
    if (X >= 0x1000000000000)
      return Relocator::Overflow;
    LLVM_FALLTHROUGH;
  case llvm::ELF::R_AARCH64_MOVW_UABS_G2_NC:
    pReloc.target() =
        helper_reencode_movzk_imm(pReloc.target(), ((X >> 32) & 0xFFFF));
    break;
  case llvm::ELF::R_AARCH64_MOVW_UABS_G3:
    pReloc.target() =
        helper_reencode_movzk_imm(pReloc.target(), ((X >> 48) & 0xFFFF));
    break;
  case llvm::ELF::R_AARCH64_MOVW_SABS_G0:
    // Check -2^16 <= X < 2^16
    if (SX >= 0x10000)
      return Relocator::Overflow;
    if ((0 - SX) >= 0x10000)
      return Relocator::Overflow;
    pReloc.target() = helper_reencode_movzk_imm(pReloc.target(), (X & 0xFFFF));
    break;
  case llvm::ELF::R_AARCH64_MOVW_SABS_G1:
    // Check -2^32 <= X < 2^32
    if (SX >= 0x100000000)
      return Relocator::Overflow;
    if ((0 - SX) >= 0x100000000)
      return Relocator::Overflow;
    pReloc.target() =
        helper_reencode_movzk_imm(pReloc.target(), ((X >> 16) & 0xFFFF));
    break;
  case llvm::ELF::R_AARCH64_MOVW_SABS_G2:
    // Check -2^48 <= X < 2^48
    if (SX >= 0x1000000000000)
      return Relocator::Overflow;
    if ((0 - SX) >= 0x1000000000000)
      return Relocator::Overflow;
    pReloc.target() =
        helper_reencode_movzk_imm(pReloc.target(), ((X >> 32) & 0xFFFF));
    break;
  default:
    return Relocator::Unsupport;
  }
  return Relocator::OK;
}

// R_AARCH64_TLSIE_ADR_GOTTPREL_PAGE21 : PAGE(G(GTPREL(S+A))) - PAGE(P)
Relocator::Result tls_gottprel_page(Relocation &pReloc,
                                    AArch64Relocator &pParent) {
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  Relocator::DWord A = pReloc.addend();
  Relocator::DWord X = pParent.getSymValue(&pReloc) + 0x10;

  if (!(pReloc.symInfo()->reserved() & Relocator::ReserveGOT)) {
    // Convert to movz
    uint32_t movz = 0xD2A00000 | (pReloc.target() & 0x0000001F);
    pReloc.target() = helper_reencode_movzk_imm(movz, X >> 16);
    return Relocator::OK;
  }

  Relocator::Address GOT_S =
      pParent.getTarget().findEntryInGOT(pReloc.symInfo())->getAddr(DiagEngine);
  Relocator::Address P = pReloc.place(pParent.module());
  Relocator::DWord GX =
      helper_get_page_address(GOT_S + A) - helper_get_page_address(P);

  pReloc.target() = helper_reencode_adr_imm(pReloc.target(), GX >> 12);

  return Relocator::OK;
}

// R_AARCH64_TLSIE_LD64_GOTTPREL_LO12_NC : G(GTPREL(S+A))
Relocator::Result tls_gottprel_lo(Relocation &pReloc,
                                  AArch64Relocator &pParent) {
  Relocator::DWord A = pReloc.addend();
  Relocator::DWord X = pParent.getSymValue(&pReloc) + 0x10;

  if (!(pReloc.symInfo()->reserved() & Relocator::ReserveGOT)) {
    // Convert to movk
    uint32_t movk = 0xF2800000 | (pReloc.target() & 0x0000001F);
    pReloc.target() = helper_reencode_movzk_imm(movk, X);
    return Relocator::OK;
  }

  Relocator::Address GOT_S = pParent.getTarget()
                                 .findEntryInGOT(pReloc.symInfo())
                                 ->getAddr(pParent.config().getDiagEngine());
  Relocator::DWord GX = helper_get_page_offset(GOT_S + A);

  pReloc.target() = helper_reencode_ldst_pos_imm(pReloc.target(), GX >> 3);

  return Relocator::OK;
}

// R_AARCH64_TLSLE_ADD_TPREL_HI12
// R_AARCH64_TLSLE_ADD_TPREL_LO12
// R_AARCH64_TLSLE_ADD_TPREL_LO12_NC : TPREL(S+A)
Relocator::Result tls_tprel(Relocation &pReloc, AArch64Relocator &pParent) {
  Relocator::DWord X = pParent.getSymValue(&pReloc) + 0x10;

  if (pReloc.type() == llvm::ELF::R_AARCH64_TLSLE_ADD_TPREL_HI12) {
    if (X >= 0x1000000)
      return Relocator::Overflow;
  } else {
    if (X >= 0x1000)
      return Relocator::Overflow;
  }

  if (pReloc.type() == llvm::ELF::R_AARCH64_TLSLE_ADD_TPREL_HI12)
    pReloc.target() = helper_reencode_add_imm(pReloc.target(), X >> 12);
  else
    pReloc.target() = helper_reencode_add_imm(pReloc.target(), X);
  return Relocator::OK;
}

// R_AARCH64_TLSDESC_ADR_PAGE21 : PAGE(G(GTLSDESC(S+A))) - PAGE(P)
Relocator::Result tls_tlsdesc_page(Relocation &pReloc,
                                   AArch64Relocator &pParent) {
  Relocator::DWord A = pReloc.addend();
  Relocator::DWord X = pParent.getSymValue(&pReloc) + 0x10;

  if (!(pReloc.symInfo()->reserved() & Relocator::ReserveGOT)) {
    // Convert to movz
    uint32_t movz = 0xD2A00000 | (pReloc.target() & 0x0000001F);
    pReloc.target() = helper_reencode_movzk_imm(movz, X >> 16);
    return Relocator::OK;
  }

  Relocator::Address GOT_S = pParent.getTarget()
                                 .findEntryInGOT(pReloc.symInfo())
                                 ->getAddr(pParent.config().getDiagEngine());
  Relocator::Address P = pReloc.place(pParent.module());
  Relocator::DWord GX =
      helper_get_page_address(GOT_S + A) - helper_get_page_address(P);

  pReloc.target() = helper_reencode_adr_imm(pReloc.target(), GX >> 12);

  return Relocator::OK;
}

// R_AARCH64_TLSDESC_LD64_LO12_NC : G(GTLSDESC(S+A))
Relocator::Result tls_tlsdesc_lo(Relocation &pReloc,
                                 AArch64Relocator &pParent) {
  Relocator::DWord A = pReloc.addend();
  Relocator::DWord X = pParent.getSymValue(&pReloc) + 0x10;

  if (!(pReloc.symInfo()->reserved() & Relocator::ReserveGOT)) {
    // Convert to movk, save to x0
    uint32_t movk = 0xF2800000;
    pReloc.target() = helper_reencode_movzk_imm(movk, X);
    return Relocator::OK;
  }

  Relocator::Address GOT_S = pParent.getTarget()
                                 .findEntryInGOT(pReloc.symInfo())
                                 ->getAddr(pParent.config().getDiagEngine());
  Relocator::DWord GX = helper_get_page_offset(GOT_S + A);
  pReloc.target() = helper_reencode_ldst_pos_imm(pReloc.target(), GX >> 3);

  // Convert Rt to X0 if static
  if (pParent.config().isCodeStatic())
    pReloc.target() = pReloc.target() & ~0x1F;

  return Relocator::OK;
}

// R_AARCH64_TLSDESC_ADD_LO12_NC : G(GTLSDESC(S+A))
Relocator::Result tls_tlsdesc_add(Relocation &pReloc,
                                  AArch64Relocator &pParent) {
  Relocator::DWord A = pReloc.addend();
  if (pParent.config().isCodeStatic()) {
    // Convert to nop
    pReloc.target() = 0xD503201F;
    return Relocator::OK;
  }

  Relocator::Address GOT_S = pParent.getTarget()
                                 .findEntryInGOT(pReloc.symInfo())
                                 ->getAddr(pParent.config().getDiagEngine());
  Relocator::DWord GX = helper_get_page_offset(GOT_S + A);
  pReloc.target() = helper_reencode_ldst_pos_imm(pReloc.target(), GX >> 3);

  return Relocator::OK;
}

// R_AARCH64_TLSDESC_CALL
Relocator::Result tls_call(Relocation &pReloc, AArch64Relocator &pParent) {
  if (pParent.config().isCodeStatic()) {
    // Convert to nop
    pReloc.target() = 0xD503201F;
    return Relocator::OK;
  }

  return Relocator::OK;
}

// R_AARCH64_COPY_INSN
Relocator::Result copyInstruction(Relocation &pReloc,
                                  AArch64Relocator &pParent) {
  ResolveInfo *rsym = pReloc.symInfo();
  Fragment *frag = rsym->outSymbol()->fragRef()->frag();
  RegionFragment *rfrag = llvm::dyn_cast<RegionFragment>(frag);
  int64_t offset = rsym->outSymbol()->fragRef()->offset();
  const char *data =
      rfrag->getRegion().data() + offset - AArch64InsnHelpers::InsnSize;
  uint32_t insn = 0;
  std::memcpy((void *)&insn, data, AArch64InsnHelpers::InsnSize);
  pReloc.target() = insn;
  return Relocator::OK;
}
