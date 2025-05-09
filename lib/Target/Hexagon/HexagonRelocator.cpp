//===- HexagonRelocator.cpp------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "HexagonRelocationFunctions.h"
#include "eld/Config/GeneralOptions.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/Support/MsgHandling.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/SymbolResolver/Resolver.h"
#include "eld/Target/ELFFileFormat.h"
#include "llvm/ADT/Twine.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/DataTypes.h"

namespace eld {
class DiagnosticEngine;
namespace {
//===--------------------------------------------------------------------===//
// Relocation Helper Functions
//===--------------------------------------------------------------------===//
/// helper_DynRel - Get an relocation entry in .rela.dyn
Relocation *helper_DynRel_init(ELFObjectFile *Obj, const Relocation *R,
                               ResolveInfo *pSym, Fragment *F, uint32_t pOffset,
                               Relocator::Type pType, HexagonLDBackend &B) {
  Relocation *rela_entry = Obj->getRelaDyn()->createOneReloc();
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
  if (pType == llvm::ELF::R_HEX_RELATIVE && R) {
    B.recordRelativeReloc(rela_entry, R);
  }
  return rela_entry;
}
} // namespace

void HexagonRelocator::CreateGOTAbsolute(ELFObjectFile *Obj,
                                         const Relocation &pReloc) {
  ResolveInfo *rsym = pReloc.symInfo();

  std::lock_guard<std::mutex> relocGuard(m_RelocMutex);

  // return if we already create GOT for this symbol
  if (rsym->reserved() & Relocator::ReserveGOT)
    return;
  rsym->setReserved(rsym->reserved() | Relocator::ReserveGOT);

  // Symbol needs GOT entry, reserve entry in .got
  HexagonGOT *G = m_Target.createGOT(GOT::Regular, Obj, rsym);

  // If the GOT is used in statically linked binaries,
  // the GOT entry is enough and no relocation is needed.
  if (config().isCodeStatic()) {
    if (!rsym->isWeakUndef() ||
        m_Target.isSectionMagicSymbol(rsym->getName()) ||
        m_Target.isStandardSymbol(rsym->getName()))
      G->setValueType(GOT::SymbolValue);
    return;
  }

  // If the symbol is not preemptible and we are not building an executable,
  // then try to use a relative reloc. We use a relative reloc if the symbol is
  // hidden otherwise.
  bool useRelative =
      (rsym->isHidden() || (config().codeGenType() != LinkerConfig::Exec &&
                            !m_Target.isSymbolPreemptible(*rsym)));
  helper_DynRel_init(Obj, &pReloc, rsym, G, 0x0,
                     useRelative ? llvm::ELF::R_HEX_RELATIVE
                                 : llvm::ELF::R_HEX_GLOB_DAT,
                     m_Target);
  if (useRelative) {
    G->setValueType(GOT::SymbolValue);
  }
}

// TODO : implement optimizations
// Rewriting code is very hard in VLIW
// we set up got to have offset from TP (fill slots with value)
// then we call a stub instead of __tls_get_addr within the executable
void HexagonRelocator::CreateGOTGD(ELFObjectFile *Obj, const Relocation &pReloc,
                                   bool Global) {
  ResolveInfo *rsym = pReloc.symInfo();

  if (rsym->outSymbol()->type() != llvm::ELF::STT_TLS)
    config().raise(Diag::tls_non_tls_mix)
        << (int)pReloc.type() << pReloc.symInfo()->name();

  std::lock_guard<std::mutex> relocGuard(m_RelocMutex);

  if (rsym->reserved() & Relocator::ReserveGOT)
    return;
  rsym->setReserved(rsym->reserved() | Relocator::ReserveGOT);

  if (config().isCodeStatic()) {
    m_Target.createTLSStub(HexagonTLSStub::GDtoIE);
    HexagonGOT *G = m_Target.createGOT(GOT::TLS_IE, Obj, rsym);
    G->setValueType(GOT::TLSStaticSymbolValue);
  } else {
    // set up a pair of got entries and a pair of dyn rel
    HexagonGOT *G = m_Target.createGOT(GOT::TLS_GD, Obj, rsym);
    // setup dyn rel for got entries against rsym
    helper_DynRel_init(Obj, &pReloc, rsym, G->getFirst(), 0x0,
                       llvm::ELF::R_HEX_DTPMOD_32, m_Target);
    if (Global)
      helper_DynRel_init(Obj, &pReloc, rsym, G->getNext(), 0x0,
                         llvm::ELF::R_HEX_DTPREL_32, m_Target);
    else
      // The second slot has the symbol value (TLS Offset).
      G->getNext()->setValueType(GOT::SymbolValue);
  }
}

void HexagonRelocator::CreateGOTIE(ELFObjectFile *Obj,
                                   const Relocation &pReloc) {
  ResolveInfo *rsym = pReloc.symInfo();

  if (rsym->outSymbol()->type() != llvm::ELF::STT_TLS)
    config().raise(Diag::tls_non_tls_mix)
        << (int)pReloc.type() << pReloc.symInfo()->name();

  std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
  m_Target.setHasStaticTLS();

  if (rsym->reserved() & Relocator::ReserveGOT)
    return;
  rsym->setReserved(rsym->reserved() | Relocator::ReserveGOT);

  // set up the got and the corresponding rel entry
  HexagonGOT *G = m_Target.createGOT(GOT::TLS_IE, Obj, rsym);
  if (config().isCodeStatic())
    G->setValueType(GOT::TLSStaticSymbolValue);
  else
    helper_DynRel_init(Obj, &pReloc, rsym, G, 0x0, llvm::ELF::R_HEX_TPREL_32,
                       m_Target);
}

void HexagonRelocator::CreatePLT(ELFObjectFile *Obj, ResolveInfo *pInfo) {
  if (pInfo->reserved() & ReservePLT)
    return;
  m_Target.createPLT(Obj, pInfo);
  pInfo->setReserved(pInfo->reserved() | ReservePLT);
}

void HexagonRelocator::CreateTLSPLT(ELFObjectFile *Obj, Relocation &pReloc,
                                    HexagonTLSStub::StubType StaticStub,
                                    HexagonTLSStub::StubType DynStub) {
  ResolveInfo *rsym = pReloc.symInfo();
  if (rsym->outSymbol()->type() != llvm::ELF::STT_TLS)
    config().raise(Diag::tls_non_tls_mix)
        << (int)pReloc.type() << pReloc.symInfo()->name();
  std::lock_guard<std::mutex> relocGuard(m_RelocMutex);

  if (config().isCodeStatic()) {
    HexagonTLSStub *T = m_Target.createTLSStub(StaticStub);
    pReloc.setSymInfo(T->symInfo());
    return;
  }
  HexagonTLSStub *T = m_Target.createTLSStub(DynStub);
  rsym = T->symInfo();
  pReloc.setSymInfo(rsym);
  CreatePLT(Obj, rsym);
}

//===--------------------------------------------------------------------===//
// HexagonRelocator
//===--------------------------------------------------------------------===//
HexagonRelocator::HexagonRelocator(HexagonLDBackend &pParent,
                                   LinkerConfig &pConfig, Module &pModule)
    : Relocator(pConfig, pModule), m_Target(pParent), m_Guard(nullptr) {
  // Mark force verify bit for specified relcoations
  if (m_Module.getPrinter()->verifyReloc() &&
      config().options().verifyRelocList().size()) {
    auto &list = config().options().verifyRelocList();
    for (auto &i : RelocDesc) {
      auto RelocInfo = llvm::Hexagon::Relocs[i.type];
      if (list.find(RelocInfo.Name) != list.end())
        i.forceVerify = true;
    }
  }
}

HexagonRelocator::~HexagonRelocator() {}

Relocator::Result HexagonRelocator::applyRelocation(Relocation &pRelocation) {
  Relocation::Type type = pRelocation.type();

  ResolveInfo *symInfo = pRelocation.symInfo();

  if (type > HEXAGON_MAXRELOCS)
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
  return RelocDesc[type].func(pRelocation, *this, RelocDesc[type]);
}

const char *HexagonRelocator::getName(Relocation::Type pType) const {
  return llvm::Hexagon::Relocs[pType].Name;
}

Relocator::Size HexagonRelocator::getSize(Relocation::Type pType) const {
  return 32;
}

// Check if the relocation is invalid while generating
// dynamic libraries
bool HexagonRelocator::isInvalidReloc(Relocation &pReloc) const {
  // If not PIC object, no relocation type is invalid
  if (!config().isCodeIndep())
    return false;

  switch (pReloc.type()) {
  case llvm::ELF::R_HEX_LO16:
  case llvm::ELF::R_HEX_HI16:
  case llvm::ELF::R_HEX_16:
  case llvm::ELF::R_HEX_8:
  case llvm::ELF::R_HEX_GPREL16_0:
  case llvm::ELF::R_HEX_GPREL16_1:
  case llvm::ELF::R_HEX_GPREL16_2:
  case llvm::ELF::R_HEX_GPREL16_3:
  case llvm::ELF::R_HEX_HL16:
  case llvm::ELF::R_HEX_32_6_X:
  case llvm::ELF::R_HEX_16_X:
  case llvm::ELF::R_HEX_12_X:
  case llvm::ELF::R_HEX_11_X:
  case llvm::ELF::R_HEX_10_X:
  case llvm::ELF::R_HEX_9_X:
  case llvm::ELF::R_HEX_8_X:
  case llvm::ELF::R_HEX_7_X:
  case llvm::ELF::R_HEX_6_X:
  case llvm::ELF::R_HEX_23_REG:
  case llvm::ELF::R_HEX_IE_LO16:
  case llvm::ELF::R_HEX_IE_HI16:
  case llvm::ELF::R_HEX_IE_32:
  case llvm::ELF::R_HEX_IE_32_6_X:
  case llvm::ELF::R_HEX_IE_16_X:
  case llvm::ELF::R_HEX_TPREL_LO16:
  case llvm::ELF::R_HEX_TPREL_HI16:
  case llvm::ELF::R_HEX_TPREL_32:
  case llvm::ELF::R_HEX_TPREL_32_6_X:
  case llvm::ELF::R_HEX_TPREL_16_X:
  case llvm::ELF::R_HEX_TPREL_11_X:
  case llvm::ELF::R_HEX_TPREL_16:
    return true;
  default:
    return false;
  }
}

bool HexagonRelocator::isRelocSupported(Relocation &pReloc) const {
  return pReloc.type() < HEXAGON_MAXRELOCS;
}

void HexagonRelocator::scanRelocation(Relocation &pReloc,
                                      eld::IRBuilder &pLinker,
                                      ELFSection &pSection,
                                      InputFile &pInputFile,
                                      CopyRelocs &CopyRelocs) {
  if (LinkerConfig::Object == config().codeGenType())
    return;

  if (!isRelocSupported(pReloc)) {
    config().raise(Diag::unsupported_reloc)
        << pReloc.type() << pSection.getDecoratedName(config().options())
        << pInputFile.getInput()->decoratedPath();
    return;
  }

  // If we are generating a shared library check for invalid relocations
  if (isInvalidReloc(pReloc)) {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    config().raise(Diag::non_pic_relocation)
        << getName(pReloc.type()) << pReloc.symInfo()->name()
        << pReloc.getSourcePath(config().options());
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
    scanLocalReloc(pInputFile, pReloc, pLinker, *section);
  else // rsym is external
    scanGlobalReloc(pInputFile, pReloc, pLinker, *section, CopyRelocs);
}

void HexagonRelocator::scanLocalReloc(InputFile &InputFile, Relocation &pReloc,
                                      eld::IRBuilder &pBuilder,
                                      ELFSection &pSection) {
  ELFObjectFile *Obj = llvm::dyn_cast<ELFObjectFile>(&InputFile);
  // rsym - The relocation target symbol
  ResolveInfo *rsym = pReloc.symInfo();

  // Special case when the linker makes a symbol local for example linker
  // defined symbols such as _DYNAMIC
  switch (pReloc.type()) {
  case llvm::ELF::R_HEX_GOT_32_6_X:
  case llvm::ELF::R_HEX_GOT_11_X:
    CreateGOTAbsolute(Obj, pReloc);
    return;
  default:
    break;
  }

  if (rsym && (ResolveInfo::Hidden == rsym->visibility()))
    return;

  switch (pReloc.type()) {
  case llvm::ELF::R_HEX_32:
    // If building PIC object (shared library or PIC executable),
    // a dynamic relocations with RELATIVE type to this location is needed.
    // Reserve an entry in .rel.dyn
    if (config().isCodeIndep()) {
      std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
      helper_DynRel_init(Obj, &pReloc, rsym, pReloc.targetRef()->frag(),
                         pReloc.targetRef()->offset(),
                         llvm::ELF::R_HEX_RELATIVE, m_Target);
      getTarget().checkAndSetHasTextRel(pSection);
    }
    return;
  case llvm::ELF::R_HEX_GD_GOT_LO16:
  case llvm::ELF::R_HEX_GD_GOT_HI16:
  case llvm::ELF::R_HEX_GD_GOT_32:
  case llvm::ELF::R_HEX_GD_GOT_16:
  case llvm::ELF::R_HEX_GD_GOT_32_6_X:
  case llvm::ELF::R_HEX_GD_GOT_16_X:
  case llvm::ELF::R_HEX_GD_GOT_11_X:
    CreateGOTGD(Obj, pReloc, false);
    return;
  case llvm::ELF::R_HEX_LD_GOT_LO16:
  case llvm::ELF::R_HEX_LD_GOT_HI16:
  case llvm::ELF::R_HEX_LD_GOT_32:
  case llvm::ELF::R_HEX_LD_GOT_16:
  case llvm::ELF::R_HEX_LD_GOT_32_6_X:
  case llvm::ELF::R_HEX_LD_GOT_16_X:
  case llvm::ELF::R_HEX_LD_GOT_11_X:

    if (rsym->outSymbol()->type() != llvm::ELF::STT_TLS)
      config().raise(Diag::tls_non_tls_mix)
          << (int)pReloc.type() << pReloc.symInfo()->name();
    getTLSModuleID(rsym);
    return;

  case llvm::ELF::R_HEX_GD_PLT_B22_PCREL:
  case llvm::ELF::R_HEX_GD_PLT_B22_PCREL_X:
  case llvm::ELF::R_HEX_GD_PLT_B32_PCREL_X:
    CreateTLSPLT(Obj, pReloc, HexagonTLSStub::GDtoIE, HexagonTLSStub::GD);
    return;

  case llvm::ELF::R_HEX_LD_PLT_B22_PCREL:
  case llvm::ELF::R_HEX_LD_PLT_B22_PCREL_X:
  case llvm::ELF::R_HEX_LD_PLT_B32_PCREL_X:
    CreateTLSPLT(Obj, pReloc, HexagonTLSStub::LDtoLE, HexagonTLSStub::GD);
    return;

  case llvm::ELF::R_HEX_GOTREL_LO16:
  case llvm::ELF::R_HEX_GOTREL_HI16:
  case llvm::ELF::R_HEX_GOTREL_32:
  case llvm::ELF::R_HEX_GOTREL_32_6_X:
  case llvm::ELF::R_HEX_GOTREL_16_X:
  case llvm::ELF::R_HEX_GOTREL_11_X: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    // This assumes that GOT exists, so we should handle the assumption as well.
    m_Target.createGOT(GOT::GOTPLT0, nullptr, nullptr);
    return;
  }

  case llvm::ELF::R_HEX_IE_LO16:
  case llvm::ELF::R_HEX_IE_HI16:
  case llvm::ELF::R_HEX_IE_32:
  case llvm::ELF::R_HEX_IE_32_6_X:
  case llvm::ELF::R_HEX_IE_16_X:
  // TODO: on code model static, convert IE to LE
  case llvm::ELF::R_HEX_IE_GOT_LO16:
  case llvm::ELF::R_HEX_IE_GOT_HI16:
  case llvm::ELF::R_HEX_IE_GOT_32:
  case llvm::ELF::R_HEX_IE_GOT_16:
  case llvm::ELF::R_HEX_IE_GOT_32_6_X:
  case llvm::ELF::R_HEX_IE_GOT_16_X:
  case llvm::ELF::R_HEX_IE_GOT_11_X:
    CreateGOTIE(Obj, pReloc);
    return;
  default:
    return;
  }
}

void HexagonRelocator::scanGlobalReloc(InputFile &InputFile, Relocation &pReloc,
                                       eld::IRBuilder &pBuilder,
                                       ELFSection &pSection,
                                       CopyRelocs &CopyRelocs) {
  ELFObjectFile *Obj = llvm::dyn_cast<ELFObjectFile>(&InputFile);
  // rsym - The relocation target symbol
  ResolveInfo *rsym = pReloc.symInfo();
  HexagonLDBackend &ld_backend = getTarget();
  bool isBranchReloc = false;

  switch (pReloc.type()) {
  case llvm::ELF::R_HEX_LO16:
  case llvm::ELF::R_HEX_HI16:
  case llvm::ELF::R_HEX_16:
  case llvm::ELF::R_HEX_8:
  case llvm::ELF::R_HEX_32_6_X:
  case llvm::ELF::R_HEX_16_X:
  case llvm::ELF::R_HEX_12_X:
  case llvm::ELF::R_HEX_11_X:
  case llvm::ELF::R_HEX_10_X:
  case llvm::ELF::R_HEX_9_X:
  case llvm::ELF::R_HEX_8_X:
  case llvm::ELF::R_HEX_7_X:
  case llvm::ELF::R_HEX_6_X:
  case llvm::ELF::R_HEX_GPREL16_0:
  case llvm::ELF::R_HEX_GPREL16_1:
  case llvm::ELF::R_HEX_GPREL16_2:
  case llvm::ELF::R_HEX_GPREL16_3:
  case llvm::ELF::R_HEX_32: {
    bool isSymbolPreemptible = m_Target.isSymbolPreemptible(*rsym);

    // Absolute relocation type, symbol may needs PLT entry or
    // dynamic relocation entry
    if (isSymbolPreemptible && (rsym->type() == ResolveInfo::Function)) {
      std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
      // create PLT for this symbol if it does not have.
      CreatePLT(Obj, rsym);
    }

    if (ld_backend.symbolNeedsDynRel(*rsym, (rsym->reserved() & ReservePLT),
                                     true)) {
      std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
      if (ld_backend.symbolNeedsCopyReloc(pReloc, *rsym)) {
        // check if the option -z nocopyreloc is given
        if (config().options().hasNoCopyReloc()) {
          config().raise(Diag::copyrelocs_is_error)
              << rsym->name() << InputFile.getInput()->decoratedPath()
              << rsym->resolvedOrigin()->getInput()->decoratedPath();
          return;
        }
        CopyRelocs.insert(rsym);
      } else {
        helper_DynRel_init(Obj, &pReloc, rsym, pReloc.targetRef()->frag(),
                           pReloc.targetRef()->offset(),
                           isSymbolPreemptible ? llvm::ELF::R_HEX_32
                                               : llvm::ELF::R_HEX_RELATIVE,
                           m_Target);
        getTarget().checkAndSetHasTextRel(pSection);
      }
    }
  }
    return;

  case llvm::ELF::R_HEX_GOTREL_LO16:
  case llvm::ELF::R_HEX_GOTREL_HI16:
  case llvm::ELF::R_HEX_GOTREL_32:
  case llvm::ELF::R_HEX_GOTREL_32_6_X:
  case llvm::ELF::R_HEX_GOTREL_16_X:
  case llvm::ELF::R_HEX_GOTREL_11_X: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    // This assumes that GOT exists, so we should handle the assumption as well.
    m_Target.createGOT(GOT::GOTPLT0, nullptr, nullptr);
    return;
  }

  case llvm::ELF::R_HEX_GOT_LO16:
  case llvm::ELF::R_HEX_GOT_HI16:
  case llvm::ELF::R_HEX_GOT_32:
  case llvm::ELF::R_HEX_GOT_16:
  case llvm::ELF::R_HEX_GOT_32_6_X:
  case llvm::ELF::R_HEX_GOT_16_X:
  case llvm::ELF::R_HEX_GOT_11_X:
    CreateGOTAbsolute(Obj, pReloc);
    return;

  case llvm::ELF::R_HEX_GD_GOT_LO16:
  case llvm::ELF::R_HEX_GD_GOT_HI16:
  case llvm::ELF::R_HEX_GD_GOT_32:
  case llvm::ELF::R_HEX_GD_GOT_16:
  case llvm::ELF::R_HEX_GD_GOT_32_6_X:
  case llvm::ELF::R_HEX_GD_GOT_16_X:
  case llvm::ELF::R_HEX_GD_GOT_11_X:
    CreateGOTGD(Obj, pReloc, true);
    return;

  case llvm::ELF::R_HEX_LD_GOT_LO16:
  case llvm::ELF::R_HEX_LD_GOT_HI16:
  case llvm::ELF::R_HEX_LD_GOT_32:
  case llvm::ELF::R_HEX_LD_GOT_16:
  case llvm::ELF::R_HEX_LD_GOT_32_6_X:
  case llvm::ELF::R_HEX_LD_GOT_16_X:
  case llvm::ELF::R_HEX_LD_GOT_11_X:
    if (rsym->outSymbol()->type() != llvm::ELF::STT_TLS)
      config().raise(Diag::tls_non_tls_mix)
          << (int)pReloc.type() << pReloc.symInfo()->name();
    getTLSModuleID(rsym);
    return;

  case llvm::ELF::R_HEX_GD_PLT_B22_PCREL:
  case llvm::ELF::R_HEX_GD_PLT_B22_PCREL_X:
  case llvm::ELF::R_HEX_GD_PLT_B32_PCREL_X:
    CreateTLSPLT(Obj, pReloc, HexagonTLSStub::GDtoIE, HexagonTLSStub::GD);
    return;

  case llvm::ELF::R_HEX_LD_PLT_B22_PCREL:
  case llvm::ELF::R_HEX_LD_PLT_B22_PCREL_X:
  case llvm::ELF::R_HEX_LD_PLT_B32_PCREL_X:
    CreateTLSPLT(Obj, pReloc, HexagonTLSStub::LDtoLE, HexagonTLSStub::GD);
    return;

  case llvm::ELF::R_HEX_PLT_B22_PCREL:
  case llvm::ELF::R_HEX_B22_PCREL:
  case llvm::ELF::R_HEX_B15_PCREL:
  case llvm::ELF::R_HEX_B13_PCREL:
  case llvm::ELF::R_HEX_B9_PCREL:
  case llvm::ELF::R_HEX_B7_PCREL:
    isBranchReloc = true;
    LLVM_FALLTHROUGH;
  case llvm::ELF::R_HEX_B7_PCREL_X:
  case llvm::ELF::R_HEX_B9_PCREL_X:
  case llvm::ELF::R_HEX_B13_PCREL_X:
  case llvm::ELF::R_HEX_B15_PCREL_X:
  case llvm::ELF::R_HEX_B22_PCREL_X:
  case llvm::ELF::R_HEX_B32_PCREL_X:
    // By default, the code is assumed to be data. If the user needs an explicit
    // stub to be inserted by the linker, the user has to make the type of
    // symbol to be a function.
    if (rsym->type() == ResolveInfo::Function)
      isBranchReloc = true;
    LLVM_FALLTHROUGH;
  case llvm::ELF::R_HEX_32_PCREL:
  case llvm::ELF::R_HEX_6_PCREL_X: {
    if (config().isCodeStatic() && isBranchReloc &&
        !config().options().getDisableGuardForWeakUndefs() &&
        (rsym->isWeak() && rsym->isUndef())) {
      defineSymbolforGuard(pBuilder, rsym, getTarget());
      // There is really no need to create a PLT here as the symbols are
      // resolved internally to the linker defined symbol.
      return;
    }
    // Dont allocate PLT entries during static linking
    if (config().isCodeStatic() || !ld_backend.isSymbolPreemptible(*rsym))
      return;
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    CreatePLT(Obj, rsym);
    return;
  }

  case llvm::ELF::R_HEX_IE_LO16:
  case llvm::ELF::R_HEX_IE_HI16:
  case llvm::ELF::R_HEX_IE_32:
  case llvm::ELF::R_HEX_IE_32_6_X:
  case llvm::ELF::R_HEX_IE_16_X:
  // TODO: on code model static, convert IE to LE
  case llvm::ELF::R_HEX_IE_GOT_LO16:
  case llvm::ELF::R_HEX_IE_GOT_HI16:
  case llvm::ELF::R_HEX_IE_GOT_32:
  case llvm::ELF::R_HEX_IE_GOT_16:
  case llvm::ELF::R_HEX_IE_GOT_32_6_X:
  case llvm::ELF::R_HEX_IE_GOT_16_X:
  case llvm::ELF::R_HEX_IE_GOT_11_X:
    CreateGOTIE(Obj, pReloc);
    return;
  default:
    break;

  } // end of switch
}

void HexagonRelocator::defineSymbolforGuard(eld::IRBuilder &pBuilder,
                                            ResolveInfo *pSym,
                                            HexagonLDBackend &pTarget) {
  static const uint8_t jmprr31[] = {0x00, 0xc0, 0x9f, 0x52};
  auto SymbolName = "__linker_guard_weak_undef";
  std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
  // Create a fragment containing jumpr r31.
  if (!m_Guard) {
    LayoutInfo *layoutInfo = pBuilder.getModule().getLayoutInfo();
    ELFSection *guardSection = pTarget.getGuard();
    Fragment *frag = make<RegionFragment>(
        llvm::StringRef((const char *)jmprr31, sizeof(jmprr31)), guardSection,
        Fragment::Type::Region, 4);
    guardSection->addFragmentAndUpdateSize(frag);
    m_Guard = pBuilder.addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
        frag->getOwningSection()->getInputFile(), SymbolName,
        ResolveInfo::Function, ResolveInfo::Define, ResolveInfo::Global, 4, 0,
        make<FragmentRef>(*frag, 0), ResolveInfo::Default,
        true /* isPostLTOPhase */);
    m_Guard->setShouldIgnore(false);
    if (m_Module.getConfig().options().isSymbolTracingRequested() &&
        m_Module.getConfig().options().traceSymbol(SymbolName))
      config().raise(Diag::target_specific_symbol) << SymbolName;
    if (layoutInfo)
      layoutInfo->recordFragment(guardSection->getInputFile(), guardSection,
                                    frag);
  }
  config().raise(Diag::resolve_undef_weak_guard)
      << pSym->name() << pSym->resolvedOrigin()->getInput()->decoratedPath()
      << SymbolName;
  pSym->outSymbol()->setFragmentRef(m_Guard->fragRef());
  return;
}

void HexagonRelocator::partialScanRelocation(Relocation &pReloc,
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

HexagonGOT *HexagonRelocator::getTLSModuleID(ResolveInfo *R) {
  static HexagonGOT *G = nullptr;
  std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
  if (!G) {
    // Allocate 2 got entries and 1 dynamic reloc for R_HEX_LD_GOT*
    G = m_Target.createGOT(GOT::TLS_LD, nullptr, nullptr);

    helper_DynRel_init(m_Target.getDynamicSectionHeadersInputFile(), nullptr,
                       nullptr, G, 0x0, llvm::ELF::R_HEX_DTPMOD_32, m_Target);
  }

  m_Target.recordGOT(R, G);
  return G;
}

uint32_t HexagonRelocator::getNumRelocs() const { return HEXAGON_MAXRELOCS; }

namespace {

//=========================================//
// Relocation Verifier
//=========================================//
template <typename T>
Relocator::Result VerifyRelocAsNeededHelper(
    Relocation &pReloc, T Result, const RelocationDescription &pRelocDesc,
    DiagnosticEngine *DiagEngine, const GeneralOptions &options) {
  uint32_t RelocType = pReloc.type();
  auto RelocInfo = llvm::Hexagon::Relocs[RelocType];
  Relocator::Result R = Relocator::OK;

  if ((RelocInfo.VerifyAlignment || pRelocDesc.forceVerify) &&
      !llvm::Hexagon::verifyAlignment(RelocType, Result))
    DiagEngine->raise(Diag::not_aligned)
        << RelocInfo.Name << pReloc.symInfo()->name()
        << pReloc.getTargetPath(options) << pReloc.getSourcePath(options)
        << RelocInfo.Alignment;

  Result >>= llvm::Hexagon::Relocs[RelocType].Shift;

  if (RelocInfo.VerifyRange) {
    if (!llvm::Hexagon::verifyRange(RelocType, Result))
      R = Relocator::Overflow;
  }

  if ((pRelocDesc.forceVerify) &&
      (llvm::Hexagon::isTruncated(RelocType, Result))) {
    DiagEngine->raise(Diag::reloc_truncated)
        << RelocInfo.Name << pReloc.symInfo()->name()
        << pReloc.getTargetPath(options) << pReloc.getSourcePath(options);
  }
  return R;
}

Relocator::Result ApplyReloc(Relocation &pReloc, uint32_t Result,
                             const RelocationDescription &pRelocDesc,
                             DiagnosticEngine *DiagEngine,
                             const GeneralOptions &options) {
  auto RelocInfo = llvm::Hexagon::Relocs[pReloc.type()];

  // Verify the Relocation.
  Relocator::Result R = Relocator::OK;
  if (RelocInfo.IsSigned)
    R = VerifyRelocAsNeededHelper<int32_t>(pReloc, Result, pRelocDesc,
                                           DiagEngine, options);
  else
    R = VerifyRelocAsNeededHelper<uint32_t>(pReloc, Result, pRelocDesc,
                                            DiagEngine, options);
  if (R != Relocator::OK)
    return R;

  // Apply the relocation.
  pReloc.target() =
      llvm::Hexagon::doReloc(pReloc.type(), pReloc.target(), Result);
  return R;
}

//=========================================//
// Each relocation function implementation //
//=========================================//
// R_HEX_NONE
Relocator::Result none(Relocation &pReloc, HexagonRelocator &pParent,
                       RelocationDescription &pRelocDesc) {
  return Relocator::OK;
}

// R_HEX_B22_PCREL and its class of relocations, use
// S + A - P : Result is signed verify.
// Exception: R_HEX_B32_PCREL_X : signed truncate
// Another Exception: R_HEX_6_PCREL_X is unsigned truncate
Relocator::Result applyRel(Relocation &pReloc, uint32_t Result,
                           const RelocationDescription &pRelocDesc,
                           DiagnosticEngine *DiagEngine,
                           const GeneralOptions &options) {
  switch (pReloc.type()) {
  case llvm::ELF::R_HEX_B22_PCREL_X:
  case llvm::ELF::R_HEX_B15_PCREL_X:
  case llvm::ELF::R_HEX_B13_PCREL_X:
  case llvm::ELF::R_HEX_B9_PCREL_X:
  case llvm::ELF::R_HEX_B7_PCREL_X:
  case llvm::ELF::R_HEX_GD_PLT_B22_PCREL_X:
  case llvm::ELF::R_HEX_LD_PLT_B22_PCREL_X:
    Result &= 0x3f;
    break;
  default:
    break;
  }
  return ApplyReloc(pReloc, Result, pRelocDesc, DiagEngine, options);
}

Relocator::Result relocAbs(Relocation &pReloc, HexagonRelocator &pParent,
                           RelocationDescription &pRelocDesc) {
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  ResolveInfo *rsym = pReloc.symInfo();
  Relocator::Address S = pParent.getSymValue(&pReloc);
  Relocator::DWord A = pReloc.addend();

  const GeneralOptions &options = pParent.config().options();
  // For absolute relocations, and If we are building a static executable and if
  // the symbol is a weak undefined symbol, it should still use the undefined
  // symbol value which is 0. For non absolute relocations, the call is set to a
  // symbol defined by the linker which returns back to the caller.
  if (rsym && rsym->isWeakUndef() &&
      (pParent.config().codeGenType() == LinkerConfig::Exec)) {
    S = 0;
    return ApplyReloc(pReloc, S + A, pRelocDesc, DiagEngine, options);
  }

  // if the flag of target section is not ALLOC, we perform only static
  // relocation.
  if (!pReloc.targetRef()->getOutputELFSection()->isAlloc()) {
    return ApplyReloc(pReloc, S + A, pRelocDesc, DiagEngine, options);
  }

  if (rsym && rsym->reserved() & Relocator::ReservePLT)
    S = pParent.getTarget().findEntryInPLT(rsym)->getAddr(DiagEngine);

  return ApplyReloc(pReloc, S + A, pRelocDesc, DiagEngine, options);
}

Relocator::Result relocPCREL(Relocation &pReloc, HexagonRelocator &pParent,
                             RelocationDescription &pRelocDesc) {
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  ResolveInfo *rsym = pReloc.symInfo();
  uint32_t Result;

  Relocator::Address S = pParent.getSymValue(&pReloc);
  Relocator::DWord A = pReloc.addend();
  Relocator::DWord P = pReloc.place(pParent.module());

  FragmentRef *target_fragref = pReloc.targetRef();
  Fragment *target_frag = target_fragref->frag();
  ELFSection *target_sect = target_frag->getOutputELFSection();

  Result = S + A - P;

  // for relocs inside non ALLOC, just apply
  if (!target_sect->isAlloc()) {
    Relocator::Result R = applyRel(pReloc, Result, pRelocDesc, DiagEngine,
                                   pParent.config().options());
    if (R == Relocator::Overflow)
      DiagEngine->raise(Diag::pcrel_reloc_overflow)
          << llvm::utohexstr(S) << llvm::utohexstr(A) << llvm::utohexstr(P)
          << llvm::utohexstr(Result);
    return R;
  }

  if (!rsym->isLocal()) {
    if (rsym->reserved() & Relocator::ReservePLT) {
      S = pParent.getTarget().findEntryInPLT(rsym)->getAddr(
          pParent.config().getDiagEngine());
      Result = S + A - P;
      Relocator::Result R = applyRel(pReloc, Result, pRelocDesc, DiagEngine,
                                     pParent.config().options());
      if (R == Relocator::Overflow)
        DiagEngine->raise(Diag::pcrel_reloc_overflow)
            << llvm::utohexstr(S) << llvm::utohexstr(A) << llvm::utohexstr(P)
            << llvm::utohexstr(Result);
      return R;
    }
  }

  Relocator::Result R = applyRel(pReloc, Result, pRelocDesc, DiagEngine,
                                 pParent.config().options());
  if (R == Relocator::Overflow)
    DiagEngine->raise(Diag::pcrel_reloc_overflow)
        << llvm::utohexstr(S) << llvm::utohexstr(A) << llvm::utohexstr(P)
        << llvm::utohexstr(Result);
  return R;
}

// R_HEX_GPREL16_0 and its class : Unsigned Verify
Relocator::Result relocGPREL(Relocation &pReloc, HexagonRelocator &pParent,
                             RelocationDescription &pRelocDesc) {
  Relocator::Address S = pParent.getSymValue(&pReloc);
  Relocator::DWord A = pReloc.addend();
  Relocator::DWord GP = pParent.getTarget().getGP();
  uint32_t Result = (uint32_t)(S + A - GP);
  return ApplyReloc(pReloc, Result, pRelocDesc,
                    pParent.config().getDiagEngine(),
                    pParent.config().options());
}

// R_HEX_PLT_B22_PCREL: PLT(S) + A - P
Relocator::Result relocPLTB22PCREL(Relocation &pReloc,
                                   HexagonRelocator &pParent,
                                   RelocationDescription &pRelocDesc) {
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  // PLT_S depends on if there is a PLT entry.
  Relocator::Address PLT_S;
  if ((pReloc.symInfo()->reserved() & Relocator::ReservePLT))
    PLT_S = pParent.getTarget()
                .findEntryInPLT(pReloc.symInfo())
                ->getAddr(DiagEngine);
  else
    PLT_S = pParent.getSymValue(&pReloc);
  Relocator::Address P = pReloc.place(pParent.module());
  uint32_t Result = (PLT_S + pReloc.addend() - P);
  return ApplyReloc(pReloc, Result, pRelocDesc, DiagEngine,
                    pParent.config().options());
}

// R_HEX_GOT_LO16 and its class : (G) Signed Truncate
// Exception: R_HEX_GOT_11_X : unsigned truncate
Relocator::Result relocGOT(Relocation &pReloc, HexagonRelocator &pParent,
                           RelocationDescription &pRelocDesc) {
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  if (!(pReloc.symInfo()->reserved() & Relocator::ReserveGOT)) {
    return Relocator::BadReloc;
  }

  Relocator::Address GOT_S = pParent.getTarget()
                                 .findEntryInGOT(pReloc.symInfo())
                                 ->getAddr(pParent.config().getDiagEngine());
  Relocator::Address GOT = pParent.getTarget().getGOTSymbolAddr();
  uint32_t Result = (int32_t)(GOT_S - GOT);
  return ApplyReloc(pReloc, Result, pRelocDesc, DiagEngine,
                    pParent.config().options());
}

// R_HEX_GOTREL_LO16: and its class of relocs
// (S + A - GOT) : Signed Truncate
Relocator::Result relocGOTREL(Relocation &pReloc, HexagonRelocator &pParent,
                              RelocationDescription &pRelocDesc) {
  Relocator::Address S = pParent.getSymValue(&pReloc);
  Relocator::DWord A = pReloc.addend();
  Relocator::Address GOT = pParent.getTarget().getGOTSymbolAddr();

  uint32_t Result = (int32_t)(S + A - GOT);
  return ApplyReloc(pReloc, Result, pRelocDesc,
                    pParent.config().getDiagEngine(),
                    pParent.config().options());
}

// R_HEX_TPREL* : Signed truncate
// Exceptions: R_HEX_TPREL_16_X and R_HEX_TPREL_11_X : unsigned truncate
// Base TLS template is negative offset from the thread pointer
// ||||||||||||||||||||||||TP||||||||||||
// ^                         ^
// Static (initial) block    Dynamic loaded blocks
// ========================
// Initial TLS template
Relocator::Result relocTPREL(Relocation &pReloc, HexagonRelocator &pParent,
                             RelocationDescription &pRelocDesc) {
  Relocator::Address BaseSize = pParent.getTarget().getTLSTemplateSize();
  uint32_t S = pParent.getSymValue(&pReloc);
  Relocator::DWord A = pReloc.addend();

  uint32_t Result = (S + A - BaseSize);

  return ApplyReloc(pReloc, Result, pRelocDesc,
                    pParent.config().getDiagEngine(),
                    pParent.config().options());
}

// R_HEX_IE_GOT* : Signed Truncate
// Exception R_HEX_IE_GOT_16 : Signed verify
// R_HEX_IE_GOT_1*_X: Unsigned truncate
Relocator::Result relocIEGOT(Relocation &pReloc, HexagonRelocator &pParent,
                             RelocationDescription &pRelocDesc) {
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  ResolveInfo *rsym = pReloc.symInfo();
  if (!(rsym->reserved() & Relocator::ReserveGOT)) {
    return Relocator::BadReloc;
  }

  Relocator::Address GOT_S =
      pParent.getTarget().findEntryInGOT(rsym)->getAddr(DiagEngine);
  Relocator::Address GOT = pParent.getTarget().getGOTSymbolAddr();
  uint32_t Result = (int32_t)(GOT_S - GOT);
  return ApplyReloc(pReloc, Result, pRelocDesc, DiagEngine,
                    pParent.config().options());
}

// R_HEX_IE* : Signed Truncate
// Exception R_HEX_IE_16_X : Unsigned truncate
Relocator::Result relocIE(Relocation &pReloc, HexagonRelocator &pParent,
                          RelocationDescription &pRelocDesc) {
  ResolveInfo *rsym = pReloc.symInfo();
  if (!(rsym->reserved() & Relocator::ReserveGOT)) {
    return Relocator::BadReloc;
  }
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  // set up the got and dynamic relocation entries if not exist
  HexagonGOT *entry = pParent.getTarget().findEntryInGOT(rsym);
  Relocator::Address G =
      entry->getOffset(DiagEngine) + entry->getOutputELFSection()->addr();

  uint32_t Result = G;
  return ApplyReloc(pReloc, Result, pRelocDesc,
                    pParent.config().getDiagEngine(),
                    pParent.config().options());
}

// R_HEX_GD_GOT* : Signed Truncate
// Exception R_HEX_GD_GOT_16: Signed Verify
// R_HEX_GD_GOT_??_X Unsigned Truncate
Relocator::Result relocGDLDGOT(Relocation &pReloc, HexagonRelocator &pParent,
                               RelocationDescription &pRelocDesc) {
  ResolveInfo *rsym = pReloc.symInfo();
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  HexagonGOT *entry1 = pParent.getTarget().findEntryInGOT(rsym);
  Relocator::Address GOT_S =
      entry1->getOffset(DiagEngine) + entry1->getOutputELFSection()->addr();
  Relocator::Address GOT = pParent.getTarget().getGOTSymbolAddr();
  uint32_t Result = (GOT_S - GOT);
  return ApplyReloc(pReloc, Result, pRelocDesc,
                    pParent.config().getDiagEngine(),
                    pParent.config().options());
}

// R_HEX_GD_PLT_B22_PCREL
Relocator::Result relocGDLDPLT(Relocation &pReloc, HexagonRelocator &pParent,
                               RelocationDescription &pRelocDesc) {
  Relocator::DWord A = pReloc.addend();
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  Relocator::DWord P = pReloc.place(pParent.module());
  uint32_t Result = 0;

  if (pParent.config().isCodeStatic()) {
    HexagonRelocator::Address S = pParent.getSymValue(&pReloc);
    Result = S + A - P;
  } else {
    Relocator::Address L = 0;
    if ((pReloc.symInfo()->reserved() & Relocator::ReservePLT))
      L = pParent.getTarget()
              .findEntryInPLT(pReloc.symInfo())
              ->getAddr(DiagEngine);
    else
      L = pParent.getSymValue(&pReloc);
    Result = (L + A - P);
  }

  return applyRel(pReloc, Result, pRelocDesc, DiagEngine,
                  pParent.config().options());
}

Relocator::Result unsupport(Relocation &pReloc, HexagonRelocator &pParent,
                            RelocationDescription &pRelocDesc) {
  return HexagonRelocator::Unsupport;
}

// R_HEX_DTPREL* : Signed truncate
// Exceptions: R_HEX_DTPREL_16_X and R_HEX_DTPREL_11_X : unsigned truncate
// TLS local template relative relocation
// S + A - T where S is address (not offset in TLS)
Relocator::Result relocDTPREL(Relocation &pReloc, HexagonRelocator &pParent,
                              RelocationDescription &pRelocDesc) {
  Relocator::DWord A = pReloc.addend();
  Relocator::DWord S;
  if (pParent.config().isCodeStatic()) {
    // since this transformation, this is an address
    // of the stub symbol
    S = pParent.getSymValue(&pReloc) - pParent.getTarget().getTLSTemplateSize();
  } else {
    // S is already offset into TLS template. Hence we do not
    // need to subtract T from it.
    S = pParent.getSymValue(&pReloc);
  }
  uint32_t Result = (int32_t)(S + A);
  return ApplyReloc(pReloc, Result, pRelocDesc,
                    pParent.config().getDiagEngine(),
                    pParent.config().options());
}

Relocator::Result relocMsg(Relocation &pReloc, HexagonRelocator &pParent,
                           RelocationDescription &pRelocDesc) {
  Relocator::Address S = pParent.getSymValue(&pReloc);
  Relocator::DWord A = pReloc.addend();
  Relocator::DWord MB = pParent.getTarget().getMsgBase();

  uint32_t Result = (uint32_t)(S + A - MB);
  return ApplyReloc(pReloc, Result, pRelocDesc,
                    pParent.config().getDiagEngine(),
                    pParent.config().options());
}

} // anonymous namespace

} // namespace eld
