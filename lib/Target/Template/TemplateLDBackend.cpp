//===- TemplateLDBackend.cpp-----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "TemplateLDBackend.h"
#include "Template.h"
#include "TemplateRelocator.h"
#include "TemplateStandaloneInfo.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Fragment/FillFragment.h"
#include "eld/Fragment/RegionFragment.h"
#include "eld/Fragment/Stub.h"
#include "eld/Object/ObjectBuilder.h"
#include "eld/Object/ObjectLinker.h"
#include "eld/Support/MemoryArea.h"
#include "eld/Support/MsgHandling.h"
#include "eld/Support/RegisterTimer.h"
#include "eld/Support/TargetRegistry.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "eld/Target/ELFFileFormat.h"
#include "eld/Target/ELFSegmentFactory.h"
#include "llvm/ADT/Hashing.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/Memory.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Program.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/TargetParser/Triple.h"
#include <string>

using namespace eld;
using namespace llvm;

//===----------------------------------------------------------------------===//
// TemplateLDBackend
//===----------------------------------------------------------------------===//
TemplateLDBackend::TemplateLDBackend(Module &pModule, TemplateInfo *pInfo)
    : GNULDBackend(pModule, pInfo), m_pRelocator(nullptr),
      m_pEndOfImage(nullptr) {}

TemplateLDBackend::~TemplateLDBackend() { delete m_pRelocator; }

bool TemplateLDBackend::initRelocator() {
  if (nullptr == m_pRelocator) {
    m_pRelocator = new TemplateRelocator(*this, config(), m_Module);
  }
  return true;
}

Relocator *TemplateLDBackend::getRelocator() const {
  assert(nullptr != m_pRelocator);
  return m_pRelocator;
}

unsigned int
TemplateLDBackend::getTargetSectionOrder(const ELFSection &pSectHdr) const {
  return SHO_UNDEFINED;
}

void TemplateLDBackend::initTargetSections(ObjectBuilder &pBuilder) {}

void TemplateLDBackend::initTargetSymbols() {
  if (config().codeGenType() == LinkerConfig::Object)
    return;

  m_pEndOfImage =
      m_Module.getIRBuilder()->addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
          m_Module.getInternalInput(Module::Script), "__end",
          ResolveInfo::NoType, ResolveInfo::Define, ResolveInfo::Absolute,
          0x0, // size
          0x0, // value
          FragmentRef::null());
  if (m_pEndOfImage)
    m_pEndOfImage->setShouldIgnore(false);
}

bool TemplateLDBackend::initBRIslandFactory() { return true; }

bool TemplateLDBackend::initStubFactory() { return true; }

/// finalizeSymbol - finalize the symbol value
bool TemplateLDBackend::finalizeTargetSymbols() {
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

uint64_t
TemplateLDBackend::getValueForDiscardedRelocations(const Relocation *R) const {
  if (!m_pEndOfImage)
    return GNULDBackend::getValueForDiscardedRelocations(R);
  return m_pEndOfImage->value();
}

void TemplateLDBackend::initializeAttributes() {
  getInfo().initializeAttributes(*m_Module.getIRBuilder());
}

namespace eld {

//===----------------------------------------------------------------------===//
/// createTemplateLDBackend - the help funtion to create corresponding
/// TemplateLDBackend
GNULDBackend *createTemplateLDBackend(Module &pModule) {
  return new TemplateLDBackend(pModule,
                               new TemplateStandaloneInfo(pModule.getConfig()));
}

} // namespace eld

//===----------------------------------------------------------------------===//
// Force static initialization.
//===----------------------------------------------------------------------===//
extern "C" void ELDInitializeTemplateLDBackend() {
  // Register the linker backend
  eld::TargetRegistry::RegisterGNULDBackend(TheTemplateTarget,
                                            createTemplateLDBackend);
}
