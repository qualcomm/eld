//===- TemplateLDBackend.cpp-----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "TemplateLDBackend.h"
#include "Template.h"
#include "TemplateRelocator.h"
#include "TemplateStandaloneInfo.h"
#include "eld/Support/TargetRegistry.h"
#include <string>

using namespace eld;
using namespace llvm;

//===----------------------------------------------------------------------===//
// TemplateLDBackend
//===----------------------------------------------------------------------===//
TemplateLDBackend::TemplateLDBackend(Module &pModule, TemplateInfo *pInfo)
    : GNULDBackend(pModule, pInfo) {}

TemplateLDBackend::~TemplateLDBackend() {}

/// finalizeSymbol - finalize the symbol value
bool TemplateLDBackend::finalizeTargetSymbols() {
  if (config().codeGenType() == LinkerConfig::Object)
    return true;

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

bool TemplateLDBackend::initRelocator() {
  if (nullptr == m_pRelocator)
    m_pRelocator = make<TemplateRelocator>(*this, config(), m_Module);

  return true;
}

void TemplateLDBackend::initTargetSections(ObjectBuilder &pBuilder) {}

void TemplateLDBackend::initTargetSymbols() {
  if (config().codeGenType() == LinkerConfig::Object)
    return;
}

void TemplateLDBackend::initializeAttributes() {
  getInfo().initializeAttributes(m_Module.getIRBuilder()->getInputBuilder());
}

Stub *TemplateLDBackend::getBranchIslandStub(Relocation *pReloc,
                                             int64_t targetValue) const {
  return nullptr;
}

TemplateGOT *TemplateLDBackend::createGOT(GOT::GOTType T, ELFObjectFile *Obj,
                                          ResolveInfo *R) {
  return nullptr;
}

// Record GOT entry.
void TemplateLDBackend::recordGOT(ResolveInfo *I, TemplateGOT *G) {
  m_GOTMap[I] = G;
}

// Record GOTPLT entry.
void TemplateLDBackend::recordGOTPLT(ResolveInfo *I, TemplateGOT *G) {
  m_GOTPLTMap[I] = G;
}

// Find an entry in the GOT.
TemplateGOT *TemplateLDBackend::findEntryInGOT(ResolveInfo *I) const {
  auto Entry = m_GOTMap.find(I);
  if (Entry == m_GOTMap.end())
    return nullptr;
  return Entry->second;
}

// Create PLT entry.
TemplatePLT *TemplateLDBackend::createPLT(ELFObjectFile *Obj, ResolveInfo *R) {
  return nullptr;
}

// Record PLT entry
void TemplateLDBackend::recordPLT(ResolveInfo *I, TemplatePLT *P) {
  m_PLTMap[I] = P;
}

// Find an entry in the PLT
TemplatePLT *TemplateLDBackend::findEntryInPLT(ResolveInfo *I) const {
  auto Entry = m_PLTMap.find(I);
  if (Entry == m_PLTMap.end())
    return nullptr;
  return Entry->second;
}

namespace eld {

//===----------------------------------------------------------------------===//
/// createTemplateLDBackend - the help function to create corresponding
/// TemplateLDBackend
GNULDBackend *createTemplateLDBackend(eld::Module &pModule) {
  return make<TemplateLDBackend>(
      pModule, make<TemplateStandaloneInfo>(pModule.getConfig()));
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
