//===- X86_32LDBackend.cpp-----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "X86_32LDBackend.h"
#include "X86_32GOT.h"
#include "X86_32Relocator.h"
#include "X86_32StandaloneInfo.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "llvm/BinaryFormat/ELF.h"

namespace eld {
X86_32LDBackend::X86_32LDBackend(Module &pModule, X86_32Info *pInfo)
    : GNULDBackend(pModule, pInfo) {}

X86_32LDBackend::~X86_32LDBackend() = default;

Relocator *X86_32LDBackend::getRelocator() const {
  assert(m_pRelocator != nullptr);
  return m_pRelocator;
}

bool X86_32LDBackend::initRelocator() {
  if (m_pRelocator == nullptr)
    m_pRelocator = make<X86_32Relocator>(*this, config(), m_Module);
  return true;
}

void X86_32LDBackend::initTargetSections(ObjectBuilder &) {}

void X86_32LDBackend::initDynamicSections(InputFile &pInputFile) {
  // i386 dynamic relocations use 8-byte Elf32_Rel records with two 4-byte
  // fields.
  GNULDBackend::initDynamicSections(pInputFile,
                                    {llvm::ELF::SHT_REL, 4, 4, 4, 4});
}

void X86_32LDBackend::ensureGOTPLT0() {
  if (m_pGOTPLT0)
    return;

  if (!getGOTPLT()->hasFragments())
    m_pGOTPLT0 = X86_32GOTPLT0::Create(getGOTPLT(), &m_Module);
}

void X86_32LDBackend::ensureGOTBaseRelocations() {
  m_HasGOTBaseRelocations = true;
  ensureGOTPLT0();
  // R_386_GOTPC refers to this linker-defined symbol directly. Define it
  // while scanning, before scanRelocation checks for undefined symbols.
  if (!m_pGOTSymbol && !defineGOTSymbol())
    m_Module.setFailure(true);
}

void X86_32LDBackend::initTargetSymbols() {
  // _GLOBAL_OFFSET_TABLE_ is created by ensureGOTBaseRelocations() only after
  // scanning identifies a supported GOT-base relocation. Creating it here
  // would expose the symbol for links that do not use the i386 GOT base.
}

bool X86_32LDBackend::defineGOTSymbol() {
  if (m_pGOTSymbol)
    return true;
  if (!m_pGOTPLT0)
    return false;

  constexpr const char *GOTSymbolName = "_GLOBAL_OFFSET_TABLE_";
  InputFile *GOTInput = m_pGOTPLT0->getOwningSection()->getInputFile();
  FragmentRef *GOTRef = make<FragmentRef>(*m_pGOTPLT0, 0);
  m_pGOTSymbol =
      m_Module.getIRBuilder()->addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
          GOTInput, GOTSymbolName, ResolveInfo::Object, ResolveInfo::Define,
          ResolveInfo::Local, 0, 0, GOTRef, ResolveInfo::Hidden);

  if (!m_pGOTSymbol)
    return false;

  m_pGOTSymbol->setShouldIgnore(false);
  if (m_Module.getConfig().options().isSymbolTracingRequested() &&
      m_Module.getConfig().options().traceSymbol(GOTSymbolName))
    config().raise(Diag::target_specific_symbol) << GOTSymbolName;
  return true;
}

// Bind _GLOBAL_OFFSET_TABLE_ to the address of the GOT base once layout has
// assigned final section addresses. The minimal i386 backend uses the
// GOTPLT0 fragment as the GOT base for R_386_GOTOFF/R_386_GOTPC in both static
// and shared final links.
bool X86_32LDBackend::finalizeTargetSymbols() {
  if (config().codeGenType() == LinkerConfig::Object ||
      !m_HasGOTBaseRelocations)
    return true;

  if (!m_pGOTSymbol || !m_pGOTPLT0)
    return false;

  m_pGOTSymbol->setValue(m_pGOTPLT0->getAddr(config().getDiagEngine()));
  return true;
}

void X86_32LDBackend::initializeAttributes() {
  getInfo().initializeAttributes(m_Module.getIRBuilder()->getInputBuilder());
}

GNULDBackend *createX86_32LDBackend(Module &pModule) {
  return make<X86_32LDBackend>(pModule,
                               make<X86_32StandaloneInfo>(pModule.getConfig()));
}

} // namespace eld
