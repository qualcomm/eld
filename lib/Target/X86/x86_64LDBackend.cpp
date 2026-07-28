//===- x86_64LDBackend.cpp-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
#include "x86_64LDBackend.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Fragment/RegionFragment.h"
#include "eld/Fragment/Stub.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/Object/ObjectBuilder.h"
#include "eld/Object/ObjectLinker.h"
#include "eld/Support/MemoryArea.h"
#include "eld/Support/RegisterTimer.h"
#include "eld/Support/TargetRegistry.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "eld/Target/ELFDynamic.h"
#include "eld/Target/ELFSegmentFactory.h"
#include "x86_64.h"
#include "x86_64Relocator.h"
#include "x86_64StandaloneInfo.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/TargetParser/Host.h"
#include <limits>

using namespace eld;
using namespace llvm;

//===----------------------------------------------------------------------===//
// x86_64LDBackend
//===----------------------------------------------------------------------===//
x86_64LDBackend::x86_64LDBackend(Module &pModule, x86_64Info *pInfo)
    : GNULDBackend(pModule, pInfo), m_pRelocator(nullptr),
      m_pEndOfImage(nullptr) {}

x86_64LDBackend::~x86_64LDBackend() {}

bool x86_64LDBackend::initRelocator() {
  if (nullptr == m_pRelocator)
    m_pRelocator = make<x86_64Relocator>(*this, config(), m_Module);
  return true;
}

Relocator *x86_64LDBackend::getRelocator() const {
  assert(nullptr != m_pRelocator);
  return m_pRelocator;
}

Relocation::Type x86_64LDBackend::getCopyRelType() const {
  return llvm::ELF::R_X86_64_COPY;
}

unsigned int
x86_64LDBackend::getTargetSectionOrder(const ELFSection &pSectHdr) const {
  return SHO_UNDEFINED;
}

void x86_64LDBackend::initTargetSections(ObjectBuilder &pBuilder) {}

void x86_64LDBackend::initDynamicSections(ELFObjectFile &InputFile) {
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

void x86_64LDBackend::initTargetSymbols() {
  if (config().codeGenType() == LinkerConfig::Object)
    return;

  m_pEndOfImage =
      m_Module.getIRBuilder()->addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
          m_Module.getInternalInput(Module::Script), "___end",
          ResolveInfo::NoType, ResolveInfo::Define, ResolveInfo::Absolute,
          0x0, // size
          0x0, // value
          FragmentRef::null());
  if (m_pEndOfImage)
    m_pEndOfImage->setShouldIgnore(false);
}

bool x86_64LDBackend::initBRIslandFactory() { return true; }

bool x86_64LDBackend::initStubFactory() { return true; }

x86_64LDBackend::GOTPCRELXOpcode
x86_64LDBackend::getGOTPCRELXOpcode(uint8_t op, uint8_t modrm) const {
  if (op == 0x8b)
    return GOTPCRELXOpcode::MOV;
  if (op == 0xff && modrm == 0x15)
    return GOTPCRELXOpcode::CALL;
  if (op == 0xff && modrm == 0x25)
    return GOTPCRELXOpcode::JMP;
  return GOTPCRELXOpcode::Unknown;
}

bool x86_64LDBackend::isGOTPCRELXRelaxable(const Relocation *reloc) const {
  // Partial links have no final layout, so relaxation is not possible; keep
  // the GOT slot and let the final link decide.
  if (config().isLinkPartial())
    return false;
  if (reloc->type() != llvm::ELF::R_X86_64_GOTPCRELX)
    return false;
  // GNU as may emit GOTPCRELX with addend != -4. Such an instruction does not
  // load the full GOT entry (e.g. movl x@GOTPCREL+4(%rip) loads the high 32
  // bits), so it cannot be relaxed. Matches lld's adjustGotPcExpr.
  if (static_cast<int64_t>(reloc->addend()) != -4)
    return false;
  const ResolveInfo *rsym = reloc->symInfo();
  // IFUNC symbols must retain their GOT slot: the IRELATIVE resolver runs at
  // load time and deposits the selected implementation's address there.
  // Relaxing to a PC-relative access would bypass that resolution. Note a
  // hidden/local IFUNC is non-preemptible, so the isIFunc() check is not
  // redundant with isSymbolPreemptible().
  if (isSymbolPreemptible(*rsym) || rsym->isIFunc())
    return false;
  auto *RF = llvm::dyn_cast<RegionFragment>(reloc->targetRef()->frag());
  if (!RF)
    return false;
  uint32_t offset = reloc->targetRef()->offset();
  // The opcode and ModR/M byte sit at offset-2 and offset-1 relative to the
  // displacement field. A corrupt or hand-crafted object could place the
  // relocation at offset 0 or 1, making those reads out-of-bounds.
  if (offset < 2)
    return false;
  // The two bytes preceding the 4-byte displacement hold the opcode and,
  // for CALL/JMP, the ModR/M byte. Only MOV/CALL/JMP are relaxable.
  const uint8_t *loc =
      reinterpret_cast<const uint8_t *>(RF->getRegion().data()) + offset;
  uint8_t op = loc[-2];
  uint8_t modrm = loc[-1];
  return getGOTPCRELXOpcode(op, modrm) != GOTPCRELXOpcode::Unknown;
}

bool x86_64LDBackend::shouldIgnoreRelocSync(Relocation *reloc) const {
  return isGOTPCRELXRelaxCandidate(reloc);
}

eld::Expected<void>
x86_64LDBackend::postProcessing(llvm::FileOutputBuffer &pOutput) {
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(GNULDBackend::postProcessing(pOutput));

  // Relaxation rewrites bytes in the laid-out output image; it is not
  // applicable to partial links, which have no final layout.
  if (config().options().getRelax() && !config().isLinkPartial())
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(doRelax(pOutput));

  return {};
}

eld::Expected<void> x86_64LDBackend::doRelax(llvm::FileOutputBuffer &pOutput) {
  uint8_t *buf = pOutput.getBufferStart();
  // The scan phase already identified every relaxation candidate (skipping
  // debug relocations and internal files, which never carry a relaxable
  // relocation). Iterate that cached set instead of re-walking all
  // relocations, and dispatch on the relocation type. Future relaxable types
  // (e.g. R_X86_64_REX_GOTPCRELX, TLS relaxations) add a case here.
  for (Relocation *reloc : m_GOTPCRELXRelaxCandidates) {
    switch (reloc->type()) {
    case llvm::ELF::R_X86_64_GOTPCRELX:
      ELDEXP_RETURN_DIAGENTRY_IF_ERROR(relaxGOTPCRELXReloc(reloc, buf));
      break;
    default:
      // Only relaxable types are recorded as candidates; skip anything else.
      break;
    }
  }
  return {};
}

eld::Expected<void> x86_64LDBackend::relaxGOTPCRELXReloc(Relocation *reloc,
                                                         uint8_t *buf) {
  // Only RegionFragment relocations are recorded as relaxation candidates
  // (isGOTPCRELXRelaxable requires a RegionFragment), so this cast must
  // succeed.
  auto *RF = llvm::dyn_cast<RegionFragment>(reloc->targetRef()->frag());
  assert(RF && "GOTPCRELX relax candidate must reference a RegionFragment");

  uint32_t offset = reloc->targetRef()->offset();
  // isGOTPCRELXRelaxable checks offset >= 2 before recording a candidate,
  // so this must hold for every relocation that reaches here.
  assert(offset >= 2 && "GOTPCRELX relax candidate must have offset >= 2");
  // Opcode/ModR/M bytes live just before the 4-byte displacement field.
  const uint8_t *loc =
      reinterpret_cast<const uint8_t *>(RF->getRegion().data()) + offset;

  ResolveInfo *rsym = reloc->symInfo();
  Relocator::Address S = reloc->symValue(m_Module);
  Relocator::DWord A = reloc->addend();
  Relocator::DWord P = reloc->place(m_Module);
  // Relaxed form is a direct PC-relative access: displacement = S + A - P.
  int64_t val = static_cast<int64_t>(S) + static_cast<int64_t>(A) -
                static_cast<int64_t>(P);

  if (!llvm::isInt<32>(val)) {
    // Unlike lld (which relayouts and can fall back to the GOT slot), eld
    // decides relaxability at scan time and has already dropped the GOT slot,
    // so an out-of-range displacement here is a hard error. Report it with the
    // source location and abort the link (matching GNU ld's --no-relax hint).
    std::string location;
    ELFSection *overflowSect = RF->getOwningSection();
    assert(overflowSect &&
           "RegionFragment in relax candidate must have an owning section");
    location = overflowSect->getLocation(offset, config().options());
    return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
        Diag::error_x86_gotpcrelx_relax_overflow,
        {std::move(location), "R_X86_64_GOTPCRELX", llvm::itostr(val),
         llvm::itostr(std::numeric_limits<int32_t>::min()),
         llvm::itostr(std::numeric_limits<int32_t>::max()),
         std::string(rsym->name())}));
  }

  FragmentRef::Offset off = reloc->targetRef()->getOutputOffset(m_Module);
  if (off == (FragmentRef::Offset)-1)
    return {};
  size_t out_off = reloc->targetRef()->getOutputELFSection()->offset() + off;

  uint8_t op = loc[-2];
  uint8_t modrm = loc[-1];
  const char *kind = nullptr;
  switch (getGOTPCRELXOpcode(op, modrm)) {
  case GOTPCRELXOpcode::MOV:
    // mov foo@GOTPCREL(%rip), reg  ->  lea foo(%rip), reg
    // The GOT-load opcode 0x8b becomes the address-load opcode 0x8d; the
    // ModR/M and displacement encoding are otherwise identical.
    buf[out_off - 2] = 0x8d;
    llvm::support::endian::write32le(buf + out_off, static_cast<uint32_t>(val));
    kind = "mov->lea";
    break;
  case GOTPCRELXOpcode::CALL:
    // call *foo@GOTPCREL(%rip)  ->  addr32 call foo
    // The 2-byte "ff 15" indirect call is 6 bytes; the direct "e8 rel32" is
    // 5 bytes, so a 0x67 (addr32) prefix pads it back to 6 bytes.
    buf[out_off - 2] = 0x67;
    buf[out_off - 1] = 0xe8;
    llvm::support::endian::write32le(buf + out_off, static_cast<uint32_t>(val));
    kind = "call->direct";
    break;
  case GOTPCRELXOpcode::JMP:
    // jmp *foo@GOTPCREL(%rip)  ->  jmp foo; nop
    // "ff 25" indirect jmp (6 bytes) becomes "e9 rel32" direct jmp (5 bytes)
    // followed by a 0x90 nop for padding. The displacement shifts one byte
    // earlier (no ModR/M), so it is written at out_off-1 with val+1 to account
    // for the one-byte-shorter instruction prefix.
    buf[out_off - 2] = 0xe9;
    llvm::support::endian::write32le(buf + out_off - 1,
                                     static_cast<uint32_t>(val + 1));
    buf[out_off + 3] = 0x90;
    kind = "jmp->direct";
    break;
  case GOTPCRELXOpcode::Unknown:
    // isGOTPCRELXRelaxable only records MOV/CALL/JMP candidates, so this is
    // not expected; leave the instruction untouched if it is ever reached.
    break;
  }

  if (m_Module.getPrinter()->traceRelax()) {
    std::string location;
    ELFSection *traceSect = RF->getOwningSection();
    assert(traceSect &&
           "RegionFragment in relax candidate must have an owning section");
    location = traceSect->getLocation(offset, config().options());
    config().raise(Diag::trace_relax_gotpcrelx)
        << location << kind << rsym->name();
  }

  return {};
}

/// finalizeSymbol - finalize the symbol value
bool x86_64LDBackend::finalizeTargetSymbols() {
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

  return true;
}

void x86_64LDBackend::doPreLayout() {
  if (LinkerConfig::Object != config().codeGenType()) {
    getRelaPLT()->setSize(getRelaPLT()->getRelocationCount() *
                          getRelaEntrySize());
    getRelaDyn()->setSize(getRelaDyn()->getRelocationCount() *
                          getRelaEntrySize());
    m_Module.addOutputSection(getRelaPLT());
    m_Module.addOutputSection(getRelaDyn());
  }
}

void x86_64LDBackend::reserveTargetDynamicEntries() {
  m_pDynamic->reserveOne(llvm::ELF::DT_RELACOUNT);
}

void x86_64LDBackend::applyTargetDynamicEntries() {
  uint32_t relaCount = 0;
  for (auto &R : getRelaDyn()->getRelocations()) {
    if (R->type() == llvm::ELF::R_X86_64_RELATIVE)
      relaCount++;
  }
  m_pDynamic->applyOne(llvm::ELF::DT_RELACOUNT, relaCount);
}

// Create GOT entry.
x86_64GOT *x86_64LDBackend::createGOT(GOT::GOTType T, ELFObjectFile *Obj,
                                      ResolveInfo *R) {

  traceGOTCreation(T, R);
  // If we are creating a GOT, always create a .got.plt.
  if (!getGOTPLT()->hasFragments()) {
    // GOTPLT0 will populate its first 8 bytes with .dynamic address.
    x86_64GOTPLT0::Create(getGOTPLT(), &m_Module);
  }

  x86_64GOT *G = nullptr;
  bool GOT = true;
  switch (T) {
  case GOT::Regular:
    G = x86_64GOT::Create(Obj->getGOT(), R);
    break;
  case GOT::GOTPLT0:
    G = llvm::dyn_cast<x86_64GOT>(*getGOTPLT()->getFragmentList().begin());
    GOT = false;
    break;
  case GOT::GOTPLTN:
    G = x86_64GOTPLTN::Create(Obj->getGOTPLT(), R);
    GOT = false;
    break;
  case GOT::TLS_GD:
    G = x86_64GDGOT::Create(Obj->getGOT(), R);
    break;
  case GOT::TLS_LD:
    G = x86_64LDGOT::Create(getGOT(), R);
    break;
  case GOT::TLS_IE:
    G = x86_64IEGOT::Create(Obj->getGOT(), R);
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
void x86_64LDBackend::recordGOT(ResolveInfo *I, x86_64GOT *G) {
  m_GOTMap[I] = G;
}

// Record GOTPLT entry.
void x86_64LDBackend::recordGOTPLT(ResolveInfo *I, x86_64GOT *G) {
  m_GOTPLTMap[I] = G;
}

// Find an entry in the GOT.
x86_64GOT *x86_64LDBackend::findEntryInGOT(ResolveInfo *I) const {
  auto Entry = m_GOTMap.find(I);
  if (Entry == m_GOTMap.end())
    return nullptr;
  return Entry->second;
}

// Create PLT entry.
x86_64PLT *x86_64LDBackend::createPLT(ELFObjectFile *Obj, ResolveInfo *R,
                                      bool isIRelative) {
  bool hasNow = config().options().hasNow();
  tracePLTCreation(R);

  reportErrorIfPLTIsDiscarded(R);

  // Create PLT0 if this is the first PLT entry. PLT0 is the common
  // trampoline that all PLTN entries jump to for symbol resolution.
  if (!hasNow && !isIRelative && !getPLT()->hasFragments()) {
    x86_64PLT0::Create(*m_Module.getIRBuilder(),
                       createGOT(GOT::GOTPLT0, nullptr, nullptr), getPLT(),
                       nullptr, hasNow);
  }
  x86_64PLT *P = x86_64PLTN::Create(
      *m_Module.getIRBuilder(), createGOT(GOT::GOTPLTN, Obj, R), getPLT(), R,
      /*BindNow*/ (hasNow || isIRelative));

  // The relocation index is set before getContent() is called during
  // layout, as it is embedded directly in the PLT entry's instruction bytes.
  x86_64PLTN *pltn = llvm::cast<x86_64PLTN>(P);
  pltn->setRelocIndex(m_RelaPLTIndex);

  Relocation &rela_entry = *Obj->getRelaPLT()->createOneReloc();
  rela_entry.setType(isIRelative ? llvm::ELF::R_X86_64_IRELATIVE
                                 : llvm::ELF::R_X86_64_JUMP_SLOT);
  rela_entry.setTargetRef(make<FragmentRef>(*P->getGOT(), 0));
  if (isIRelative) {
    P->getGOT()->setValueType(GOT::SymbolValue);
    if (auto *gpltn = llvm::dyn_cast<x86_64GOTPLTN>(P->getGOT()))
      gpltn->setNoInit(true);
  }
  rela_entry.setSymInfo(R);

  m_RelaPLTIndex++;

  if (R)
    recordPLT(R, P);
  return P;
}

// Record GOT entry.
void x86_64LDBackend::recordPLT(ResolveInfo *I, x86_64PLT *P) {
  m_PLTMap[I] = P;
}

// Find an entry in the GOT.
x86_64PLT *x86_64LDBackend::findEntryInPLT(ResolveInfo *I) const {
  auto Entry = m_PLTMap.find(I);
  if (Entry == m_PLTMap.end())
    return nullptr;
  return Entry->second;
}

uint64_t
x86_64LDBackend::getValueForDiscardedRelocations(const Relocation *R) const {
  if (!m_pEndOfImage)
    return GNULDBackend::getValueForDiscardedRelocations(R);
  return m_pEndOfImage->value();
}

void x86_64LDBackend::initializeAttributes() {
  getInfo().initializeAttributes(m_Module.getIRBuilder()->getInputBuilder());
}

void x86_64LDBackend::setDefaultConfigs() {
  if (config().options().threadsEnabled() &&
      !config().isGlobalThreadingEnabled()) {
    config().disableThreadOptions(LinkerConfig::EnableThreadsOpt::AllThreads);
  }
}

/// sortRelocation - Override to handle TLS Local Dynamic (TLSLD) relocations.
///
/// The base implementation in GNULDBackend assumes that relocations without
/// symbol info (!hasSymInfo) are RELATIVE relocations. However, TLSLD emits
/// R_X86_64_DTPMOD64 relocations without symbol info (they reference the
/// module, not a specific symbol). This causes DTPMOD64 to be incorrectly
/// sorted among RELATIVE relocations.
///
/// The dynamic loader expects the first N relocations to be RELATIVE, but
/// mixing DTPMOD64 among them causes loader errors. This override ensures
/// proper sorting by explicitly checking relocation types rather than relying
/// solely on hasSymInfo().
void x86_64LDBackend::sortRelocation(ELFSection &pSection) {
  if (!config().options().hasCombReloc())
    return;

  if (pSection.getKind() != LDFileFormat::DynamicRelocation)
    return;

  if ((pSection.name() != ".rel.dyn") && (pSection.name() != ".rela.dyn"))
    return;

  std::sort(pSection.getRelocations().begin(), pSection.getRelocations().end(),
            [this](Relocation *X, Relocation *Y) {
              // 1. RELATIVE relocations always come first
              bool xIsRelative = (X->type() == llvm::ELF::R_X86_64_RELATIVE);
              bool yIsRelative = (Y->type() == llvm::ELF::R_X86_64_RELATIVE);

              if (xIsRelative && !yIsRelative)
                return true;
              if (!xIsRelative && yIsRelative)
                return false;

              // 2. Among non-RELATIVE relocations, compare if relocation has
              // symbol info
              if (!hasSymInfo(X)) {
                if (hasSymInfo(Y))
                  return true;
              } else if (!hasSymInfo(Y)) {
                return false;
              } else {
                // 2. compare the symbol index
                size_t symIdxX = getDynSymbolIdx(X->symInfo()->outSymbol());
                size_t symIdxY = getDynSymbolIdx(Y->symInfo()->outSymbol());
                if (symIdxX < symIdxY)
                  return true;
                if (symIdxX > symIdxY)
                  return false;
              }

              // 3. compare the relocation address
              if (X->place(m_Module) < Y->place(m_Module))
                return true;
              if (X->place(m_Module) > Y->place(m_Module))
                return false;

              // 4. compare the relocation type
              if (X->type() < Y->type())
                return true;
              if (X->type() > Y->type())
                return false;

              // 5. compare the addend
              if (X->addend() < Y->addend())
                return true;
              if (X->addend() > Y->addend())
                return false;

              return false;
            });
}

namespace eld {

//===----------------------------------------------------------------------===//
/// createx86_64LDBackend - the help function to create corresponding
/// x86_64LDBackend
GNULDBackend *createx86_64LDBackend(Module &pModule) {
  return make<x86_64LDBackend>(pModule,
                               make<x86_64StandaloneInfo>(pModule.getConfig()));
}

} // namespace eld

//===----------------------------------------------------------------------===//
// Force static initialization.
//===----------------------------------------------------------------------===//
extern "C" void ELDInitializeX86LDBackend() {
  // Register the linker backend
  eld::TargetRegistry::RegisterGNULDBackend(Thex86_64Target,
                                            createx86_64LDBackend);
}
