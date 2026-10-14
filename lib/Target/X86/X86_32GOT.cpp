//===- X86_32GOT.cpp------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "X86_32GOT.h"
#include "eld/Readers/ELFSection.h"
#include <cstring>

using namespace eld;

X86_32GOTPLT0 *X86_32GOTPLT0::Create(ELFSection *pOutput, Module *pModule) {
  return make<X86_32GOTPLT0>(pOutput, pModule);
}

llvm::ArrayRef<uint8_t> X86_32GOTPLT0::getContent() const {
  std::memset(m_Value, 0, sizeof(m_Value));
  if (m_Module) {
    if (ELFSection *Dynamic = m_Module->getSection(".dynamic")) {
      const uint32_t DynamicAddress = static_cast<uint32_t>(Dynamic->addr());
      std::memcpy(m_Value, &DynamicAddress, sizeof(DynamicAddress));
    }
  }
  return llvm::ArrayRef(m_Value);
}
