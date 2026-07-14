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
#include <mutex>
#include <unordered_map>

using namespace eld;

//===--------------------------------------------------------------------===//
// ScanFlagMap — two-phase scan flag accumulator.
// Defined inside namespace eld to match the forward declaration in the header.
//===--------------------------------------------------------------------===//
namespace eld {

/// Bit flags set during parallel scan phase 1.
enum ScanFlagBit : uint8_t {
  MayNeedGOT = 1 << 0,
  MayNeedPLT = 1 << 1,
  MayNeedTLSIE = 1 << 2,
  MayNeedTLSGD = 1 << 3,
};

/// One slot per global symbol, holding the routing decision made during the
/// parallel scan (phase 1). `Flags` is atomic so phase 1 can OR bits into it
/// lock-free from multiple threads. The *referencing* input file is NOT stored
/// here: phase 2 re-walks the relocations in encounter order and recovers the
/// owning ELFObjectFile from the walk (see allocateDynEntries).
struct ScanEntry {
  std::atomic<uint8_t> Flags{0};
};

struct ScanFlagMap {
  // Pre-populated by init() with every global, single-threaded, so no
  // concurrent insertion ever happens. Phase 1 only *updates* existing slots.
  std::unordered_map<ResolveInfo *, ScanEntry> Entries;

  void init(Module &M) {
    auto &Globals = M.getNamePool().getGlobals();
    Entries.reserve(Globals.size() * 2);
    for (auto &G : Globals)
      Entries.try_emplace(G.getValue());
  }

  // Only called for global symbols pre-inserted by init(). Local symbols take
  // direct code paths (e.g. TLSLD/local GOTTPOFF handled inline in
  // scanOneReloc) and must never call set() — concurrent insertion into
  // unordered_map is not safe.
  void set(ResolveInfo *rsym, uint8_t bit) {
    auto it = Entries.find(rsym);
    if (it == Entries.end())
      return;
    it->second.Flags.fetch_or(bit, std::memory_order_relaxed);
  }

  uint8_t get(ResolveInfo *rsym) const {
    auto it = Entries.find(rsym);
    return it != Entries.end()
               ? it->second.Flags.load(std::memory_order_relaxed)
               : 0;
  }

  bool empty() const { return Entries.empty(); }
};

} // namespace eld

//===--------------------------------------------------------------------===//
// x86_64Relocator
//===--------------------------------------------------------------------===//
x86_64Relocator::x86_64Relocator(x86_64LDBackend &pParent,
                                 LinkerConfig &pConfig, Module &pModule)
    : Relocator(pConfig, pModule), m_Target(pParent),
      m_ScanFlags(std::make_unique<ScanFlagMap>()) {
  // NOTE: m_ScanFlags is NOT populated here. The constructor runs from
  // initRelocator() on the first input file, before symbol resolution has
  // filled getGlobals(). Population happens in initScanRelocations(), called
  // single-threaded just before the parallel scan.
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

// Out-of-line destructor: ScanFlagMap must be complete for unique_ptr.
x86_64Relocator::~x86_64Relocator() = default;

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

bool x86_64Relocator::isRelocSupported(const Relocation &pReloc) const {

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
  case llvm::ELF::R_X86_64_TLSLD:
    return true;
  default:
    return false;
  }
}

void x86_64Relocator::scanRelocation(Relocation &pReloc,
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

  if (!checkPICRelocSupported(pReloc))
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

  bool isLocal = rsym->isLocal();
  scanOneReloc(pInputFile, pReloc, pLinker, *section, CopyRelocs, isLocal);
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
    if (R && R->type() == llvm::ELF::R_X86_64_64) {
      // Non-preemptible R_X86_64_64 → preserve original addend
      // Writer will compute final: S + A (see emitRela RELATIVE case)
      rela_entry->setAddend(R->addend());
    } else {
      // Phase-2 GOT RELATIVE (no original relocation) or GOTPCREL — addend = 0.
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

  if (R && (pType == llvm::ELF::R_X86_64_RELATIVE ||
            pType == llvm::ELF::R_X86_64_IRELATIVE)) {
    B.recordRelativeReloc(rela_entry, R);
  }
  return rela_entry;
}

} // namespace

x86_64GOT *x86_64Relocator::getTLSModuleID(ResolveInfo *R, bool isStatic) {
  if (m_TLSModuleIDGOT != nullptr) {
    m_Target.recordGOT(R, m_TLSModuleIDGOT);
    return m_TLSModuleIDGOT;
  }

  m_TLSModuleIDGOT = m_Target.createGOT(GOT::TLS_LD, nullptr, nullptr);

  ASSERT(!isStatic,
         "We always need to relax if -static because libc.a doesn't "
         "contain__tls_get_addr(). Relaxations are currently unsupported");

  if (!isStatic)
    helper_DynRel_init(m_Target.getDynamicSectionHeadersInputFile(), nullptr,
                       nullptr, m_TLSModuleIDGOT, 0x0,
                       llvm::ELF::R_X86_64_DTPMOD64, m_Target);

  m_Target.recordGOT(R, m_TLSModuleIDGOT);
  return m_TLSModuleIDGOT;
}
void x86_64Relocator::scanOneReloc(InputFile &pInputFile, Relocation &pReloc,
                                   eld::IRBuilder &pBuilder,
                                   ELFSection &pSection, CopyRelocs &copyRelocs,
                                   bool isLocal) {
  ResolveInfo *rsym = pReloc.symInfo();

  // Phase 1 is lock-free: it performs NO shared-state mutation (no createGOT/
  // createPLT/helper_DynRel_init/setReserved). It only (a) sets per-symbol
  // routing flags for globals via the atomic flag map, and (b) makes the
  // read-only copy-relocation / non-PIC / diagnostic decisions for absolute
  // relocations. All actual allocation happens in phase 2 (allocateDynEntries).
  //
  // Why the copy-reloc decision must stay here (not phase 2): it feeds
  // ObjectLinker::createCopyRelocation(), which runs after the parallel scan
  // and BEFORE phase 2, converting the symbol to a local BSS define. That
  // conversion is what makes phase 2's symbolNeedsDynRel() correctly skip the
  // symbol. Deferring the decision would break that ordering.
  //
  // Diagnostics are safe without m_RelocMutex: DiagnosticEngine::raise() locks
  // internally, and symbolNeedsCopyReloc/symbolNeedsDynRel/isSymbolPreemptible
  // are const reads. copyRelocs is a per-file (thread-local) set.

  switch (pReloc.type()) {
  case llvm::ELF::R_X86_64_64: {
    if (isLocal)
      return; // local RELATIVE dynrel is created per-site in phase 2
    // Global: only the copy-relocation decision is made here. The dynamic
    // relocation itself (if needed and not a copy reloc) is created in phase 2.
    if (getTarget().symbolNeedsDynRel(*rsym, (rsym->reserved() & ReservePLT),
                                      true) &&
        getTarget().symbolNeedsCopyReloc(pReloc, *rsym)) {
      if (config().options().hasNoCopyReloc()) {
        config().raise(Diag::copyrelocs_is_error)
            << rsym->name() << pInputFile.getInput()->decoratedPath()
            << rsym->resolvedOrigin()->getInput()->decoratedPath();
        return;
      }
      copyRelocs.insert(rsym);
    }
    return;
  }

  case llvm::ELF::R_X86_64_32:
  case llvm::ELF::R_X86_64_32S:
  case llvm::ELF::R_X86_64_16:
  case llvm::ELF::R_X86_64_8: {
    // Decision-only case: these never allocate a GOT/PLT; they either need a
    // copy reloc (collected here) or are a hard non-PIC error. No phase-2 work.
    if (isLocal) {
      if (config().isCodeIndep())
        checkDynamicRelocAllowed(pReloc, pSection, true);
      return;
    }
    if (getTarget().symbolNeedsDynRel(*rsym, (rsym->reserved() & ReservePLT),
                                      true)) {
      if (getTarget().symbolNeedsCopyReloc(pReloc, *rsym)) {
        if (config().options().hasNoCopyReloc()) {
          config().raise(Diag::copyrelocs_is_error)
              << rsym->name() << pInputFile.getInput()->decoratedPath()
              << rsym->resolvedOrigin()->getInput()->decoratedPath();
          return;
        }
        copyRelocs.insert(rsym);
        return;
      }
      config().raise(Diag::non_pic_relocation)
          << (int)pReloc.type() << pReloc.symInfo()->name()
          << pReloc.getSourcePath(config().options());
      m_Target.getModule().setFailure(true);
    }
    return;
  }

  case llvm::ELF::R_X86_64_PLT32: {
    if (isLocal || rsym->reserved() & ReservePLT)
      return;
    // Static IFUNC PLT and preemptible PLT are both allocated in phase 2; here
    // we only record the routing decision for the preemptible case. (IFUNC is
    // re-derived by type in phase 2, so it needs no flag.)
    if (rsym->type() == ResolveInfo::IndirectFunc && config().isCodeStatic())
      return;
    if (!getTarget().isSymbolPreemptible(*rsym))
      return;
    m_ScanFlags->set(rsym, MayNeedPLT);
    return;
  }

  case llvm::ELF::R_X86_64_GOTPCREL:
  case llvm::ELF::R_X86_64_GOTPCRELX:
  case llvm::ELF::R_X86_64_REX_GOTPCRELX: {
    if (isLocal || rsym->reserved() & ReserveGOT)
      return;
    m_ScanFlags->set(rsym, MayNeedGOT);
    return;
  }

  case llvm::ELF::R_X86_64_GOTTPOFF: {
    if (rsym->reserved() & ReserveGOT)
      return;
    // Global TLS-IE is routed via the flag map; local TLS-IE is re-derived by
    // type in phase 2 (locals are not in the flag map). Both allocate in
    // phase 2.
    if (!isLocal)
      m_ScanFlags->set(rsym, MayNeedTLSIE);
    return;
  }

  case llvm::ELF::R_X86_64_TLSGD: {
    if (isLocal || rsym->reserved() & ReserveGOT)
      return;
    m_ScanFlags->set(rsym, MayNeedTLSGD);
    return;
  }

  case llvm::ELF::R_X86_64_TLSLD:
    // TLSLD creates one shared module-ID GOT entry; allocated in phase 2.
    return;

  default:
    break;
  }
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

// Phase 0: populate the per-global flag map. Runs single-threaded from
// ObjectLinker::scanRelocations(), after symbol resolution has filled the
// name pool but before the parallel scan begins. Populating here (not in the
// constructor) is required: the constructor runs on the first input file,
// long before getGlobals() is complete.
void x86_64Relocator::initScanRelocations() { m_ScanFlags->init(module()); }

// Phase 2: the single allocation pass. Runs sequentially after Pool->wait(),
// so NO mutex is needed anywhere here.
//
// Two kinds of work happen in the same encounter-order re-walk:
//   (1) Flag-driven, per-symbol, allocate-once (global GOT/PLT/TLS-IE/TLS-GD):
//       the routing decision was cached as a flag bit in phase 1; here we
//       allocate at the symbol's first encounter, guarded by reserved().
//   (2) Type-driven, re-derived here (things that cannot use the global flag
//       map): local RELATIVE dynrels and global R_X86_64_64 dynrels (per site),
//       local GOTTPOFF (TLS-IE), the TLSLD module-ID singleton, and static
//       IFUNC PLT. These were previously done inline under m_RelocMutex during
//       the parallel scan; moving them here makes phase 1 fully lock-free.
//
// Encounter order (object-list → section → within-section) reproduces pristine
// GOT/PLT slot offsets and .rela.plt indices, which are a test/ABI contract.
// The walk hands us the referencing ELFObjectFile for free (each file owns its
// own .got/.rela.dyn — see initDynamicSections).
void x86_64Relocator::allocateDynEntries() {
  // Partial links (-r / LinkerConfig::Object) never scan for GOT/PLT/dynrels
  // (scanRelocation early-returns for them, and per-file .got/.rela.dyn are not
  // created by initDynamicSections). finalizeScanRelocations still runs for
  // them, so guard here too — otherwise the re-walk would deref a null
  // getRelaDyn()/getGOT().
  if (LinkerConfig::Object == config().codeGenType())
    return;

  for (InputFile *Input : module().getObjectList()) {
    ELFObjectFile *Obj = llvm::dyn_cast<ELFObjectFile>(Input);
    if (!Obj)
      continue;
    for (ELFSection *Rs : Obj->getRelocationSections()) {
      if (Rs->isIgnore() || Rs->isDiscard())
        continue;
      for (Relocation *pReloc : Rs->getLink()->getRelocations()) {
        if (m_Target.maySkipRelocProcessing(pReloc))
          continue;
        ResolveInfo *rsym = pReloc->symInfo();
        if (!rsym)
          continue;

        // Replicate scanRelocation's section filter: only allocate for
        // relocations whose target section is allocatable. This also ensures
        // flag-driven allocation attaches to the correct first *alloc*
        // encounter (matching pristine order and Obj selection).
        ELFSection *section =
            Rs->getLink() ? Rs->getLink()
                          : pReloc->targetRef()->frag()->getOwningSection();
        if (!section || !section->isAlloc())
          continue;

        bool isLocal = rsym->isLocal();
        // Locals are never pre-inserted in the flag map (it's populated only
        // from getGlobals()), so get() would always return 0 and waste a hash
        // lookup. Skip it for locals — they are handled by the type-driven
        // switch below.
        uint8_t f = isLocal ? 0 : m_ScanFlags->get(rsym);

        // ---- (1) flag-driven, per-symbol, allocate-once ----
        if ((f & MayNeedGOT) && !(rsym->reserved() & ReserveGOT)) {
          x86_64GOT *Gslot = m_Target.createGOT(GOT::Regular, Obj, rsym);
          if (!config().isCodeStatic()) {
            bool useRelative = !m_Target.isSymbolPreemptible(*rsym);
            helper_DynRel_init(Obj, nullptr, rsym, Gslot, 0,
                               useRelative ? llvm::ELF::R_X86_64_RELATIVE
                                           : llvm::ELF::R_X86_64_GLOB_DAT,
                               m_Target);
          } else {
            Gslot->setValueType(GOT::SymbolValue);
          }
          rsym->setReserved(rsym->reserved() | ReserveGOT);
        }

        if ((f & MayNeedPLT) && !(rsym->reserved() & ReservePLT) &&
            m_Target.isSymbolPreemptible(*rsym) &&
            rsym->type() != ResolveInfo::IndirectFunc) {
          m_Target.createPLT(Obj, rsym);
          rsym->setReserved(rsym->reserved() | ReservePLT);
        }

        if ((f & MayNeedTLSIE) && !(rsym->reserved() & ReserveGOT)) {
          x86_64GOT *Gtls = m_Target.createGOT(GOT::TLS_IE, Obj, rsym);
          if (config().isBuildingExecutable() &&
              !m_Target.isSymbolPreemptible(*rsym)) {
            Gtls->setValueType(GOT::TLSStaticSymbolValue);
          } else {
            helper_DynRel_init(Obj, nullptr, rsym, Gtls, 0,
                               llvm::ELF::R_X86_64_TPOFF64, m_Target);
            m_Target.setHasStaticTLS();
          }
          rsym->setReserved(rsym->reserved() | ReserveGOT);
        }

        if ((f & MayNeedTLSGD) && !(rsym->reserved() & ReserveGOT)) {
          x86_64GOT *Ggd = m_Target.createGOT(GOT::TLS_GD, Obj, rsym);
          helper_DynRel_init(Obj, nullptr, rsym, Ggd->getFirst(), 0,
                             llvm::ELF::R_X86_64_DTPMOD64, m_Target);
          if (m_Target.isSymbolPreemptible(*rsym))
            helper_DynRel_init(Obj, nullptr, rsym, Ggd->getNext(), 0,
                               llvm::ELF::R_X86_64_DTPOFF64, m_Target);
          else
            Ggd->getNext()->setValueType(GOT::TLSStaticSymbolValue);
          rsym->setReserved(rsym->reserved() | ReserveGOT);
        }

        // ---- (2) type-driven, re-derived here ----
        switch (pReloc->type()) {
        case llvm::ELF::R_X86_64_64: {
          if (isLocal) {
            // Local absolute in PIC output: one RELATIVE dynrel per site.
            if (config().isCodeIndep() &&
                checkDynamicRelocAllowed(*pReloc, *section, true)) {
              rsym->setReserved(rsym->reserved() | ReserveRel);
              m_Target.checkAndSetHasTextRel(*section);
              helper_DynRel_init(Obj, pReloc, rsym, pReloc->targetRef()->frag(),
                                 pReloc->targetRef()->offset(),
                                 llvm::ELF::R_X86_64_RELATIVE, m_Target);
            }
            break;
          }
          // Global absolute. Copy-relocated symbols were converted to local
          // BSS defines by createCopyRelocation (before this pass), so
          // symbolNeedsDynRel() returns false for them here — no double work.
          if (m_Target.symbolNeedsDynRel(*rsym, (rsym->reserved() & ReservePLT),
                                         true) &&
              !m_Target.symbolNeedsCopyReloc(*pReloc, *rsym) &&
              checkDynamicRelocAllowed(*pReloc, *section, true)) {
            bool isPreemptible = m_Target.isSymbolPreemptible(*rsym);
            rsym->setReserved(rsym->reserved() | ReserveRel);
            m_Target.checkAndSetHasTextRel(*section);
            helper_DynRel_init(Obj, pReloc, rsym, pReloc->targetRef()->frag(),
                               pReloc->targetRef()->offset(),
                               isPreemptible ? llvm::ELF::R_X86_64_64
                                             : llvm::ELF::R_X86_64_RELATIVE,
                               m_Target);
          }
          break;
        }

        case llvm::ELF::R_X86_64_PLT32: {
          // Static IFUNC: emit IRELATIVE PLT. Preemptible PLT is flag-driven
          // above; this branch only fires for the IFUNC-in-static case, which
          // set no flag in phase 1.
          if (!isLocal && !(rsym->reserved() & ReservePLT) &&
              rsym->type() == ResolveInfo::IndirectFunc &&
              config().isCodeStatic()) {
            m_Target.createPLT(Obj, rsym, /*isIRelative=*/true);
            rsym->setReserved(rsym->reserved() | ReservePLT);
            m_Target.defineIRelativeRange(*rsym);
          }
          break;
        }

        case llvm::ELF::R_X86_64_GOTTPOFF: {
          // Local __thread IE: not in the flag map, re-derived by type here.
          if (isLocal && !(rsym->reserved() & ReserveGOT)) {
            x86_64GOT *G = m_Target.createGOT(GOT::TLS_IE, Obj, rsym);
            if (config().isBuildingExecutable()) {
              G->setValueType(GOT::TLSStaticSymbolValue);
            } else {
              helper_DynRel_init(Obj, pReloc, rsym, G, 0x0,
                                 llvm::ELF::R_X86_64_TPOFF64, m_Target);
              m_Target.setHasStaticTLS();
            }
            rsym->setReserved(rsym->reserved() | ReserveGOT);
          }
          break;
        }

        case llvm::ELF::R_X86_64_TLSLD:
          // One shared module-ID GOT entry per module (singleton). In this
          // single-threaded pass no mutex is needed.
          getTLSModuleID(rsym, config().isCodeStatic());
          break;

        default:
          break;
        }
      }
    }
  }

  m_ScanFlags.reset();
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
