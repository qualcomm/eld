//===- x86_64LDBackend.cpp-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
#include "x86_64LDBackend.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Fragment/Stub.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/Object/ObjectBuilder.h"
#include "eld/Object/ObjectLinker.h"
#include "eld/Support/MemoryArea.h"
#include "eld/Support/RegisterTimer.h"
#include "eld/Support/TargetRegistry.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "eld/Target/ELFFileFormat.h"
#include "eld/Target/ELFSegmentFactory.h"
#include "x86_64.h"
#include "x86_64ELFDynamic.h"
#include "x86_64Relocator.h"
#include "x86_64StandaloneInfo.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/Casting.h"
#include "llvm/TargetParser/Host.h"

using namespace eld;
using namespace llvm;

//===----------------------------------------------------------------------===//
// x86_64LDBackend
//===----------------------------------------------------------------------===//
x86_64LDBackend::x86_64LDBackend(Module &pModule, x86_64Info *pInfo)
    : GNULDBackend(pModule, pInfo), m_pRelocator(nullptr), m_pDynamic(nullptr),
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
  // Create a dynamic section handler for non-static or forced-dynamic outputs.
  if (!config().isCodeStatic() || config().options().forceDynamic())
    m_pDynamic = make<x86_64ELFDynamic>(*this, config());

  if (LinkerConfig::Object != config().codeGenType()) {
    getRelaPLT()->setSize(getRelaPLT()->getRelocations().size() *
                          getRelaEntrySize());
    m_Module.addOutputSection(getRelaPLT());
  }
}

x86_64ELFDynamic *x86_64LDBackend::dynamic() { return m_pDynamic; }

// Create GOT entry.
x86_64GOT *x86_64LDBackend::createGOT(GOT::GOTType T, ELFObjectFile *Obj,
                                      ResolveInfo *R) {

  if (R != nullptr && ((config().options().isSymbolTracingRequested() &&
                        config().options().traceSymbol(*R)) ||
                       m_Module.getPrinter()->traceDynamicLinking()))
    config().raise(Diag::create_got_entry) << R->name();
  // If we are creating a GOT, always create a .got.plt.
  if (!getGOTPLT()->getFragmentList().size()) {
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
    assert(0);
    break;
  case GOT::TLS_IE:
    G = x86_64IEGOT::Create(Obj->getGOT(), R);
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
x86_64PLT *x86_64LDBackend::createPLT(ELFObjectFile *Obj, ResolveInfo *R) {
  bool hasNow = config().options().hasNow();
  if (R != nullptr && ((config().options().isSymbolTracingRequested() &&
                        config().options().traceSymbol(*R)) ||
                       m_Module.getPrinter()->traceDynamicLinking()))
    config().raise(Diag::create_plt_entry) << R->name();

  // Create PLT0 if this is the first PLT entry. PLT0 is the common
  // trampoline that all PLTN entries jump to for symbol resolution.
  if (!hasNow && !getPLT()->getFragmentList().size()) {
    x86_64PLT0::Create(*m_Module.getIRBuilder(),
                       createGOT(GOT::GOTPLT0, nullptr, nullptr), getPLT(),
                       nullptr, hasNow);
  }
  x86_64PLT *P =
      x86_64PLTN::Create(*m_Module.getIRBuilder(),
                         createGOT(GOT::GOTPLTN, Obj, R), getPLT(), R, hasNow);

  // The relocation index is set before getContent() is called during
  // layout, as it is embedded directly in the PLT entry's instruction bytes.
  x86_64PLTN *pltn = llvm::cast<x86_64PLTN>(P);
  pltn->setRelocIndex(m_RelaPLTIndex);

  // Create JUMP_SLOT relocation entry in .rela.plt section for dynamic linker
  // The dynamic linker uses this to resolve the function address on first call
  Relocation &rela_entry = *Obj->getRelaPLT()->createOneReloc();
  rela_entry.setType(llvm::ELF::R_X86_64_JUMP_SLOT);
  Fragment *F = P->getGOT();
  rela_entry.setTargetRef(make<FragmentRef>(*F, 0));
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
extern "C" void ELDInitializex86_64LDBackend() {
  // Register the linker backend
  eld::TargetRegistry::RegisterGNULDBackend(Thex86_64Target,
                                            createx86_64LDBackend);
}
