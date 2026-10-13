//===- X86_32LDBackend.cpp-----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "X86_32LDBackend.h"
#include "X86_32Relocator.h"
#include "X86_32StandaloneInfo.h"

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

void X86_32LDBackend::initializeAttributes() {
  getInfo().initializeAttributes(m_Module.getIRBuilder()->getInputBuilder());
}

GNULDBackend *createX86_32LDBackend(Module &pModule) {
  return make<X86_32LDBackend>(pModule,
                               make<X86_32StandaloneInfo>(pModule.getConfig()));
}

} // namespace eld
