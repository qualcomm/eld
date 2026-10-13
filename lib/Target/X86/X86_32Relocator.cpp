//===- X86_32Relocator.cpp-----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "X86_32Relocator.h"

namespace eld {

X86_32Relocator::X86_32Relocator(X86_32LDBackend &pParent,
                                 LinkerConfig &pConfig, Module &pModule)
    : Relocator(pConfig, pModule), m_Target(pParent) {}

Relocator::Result X86_32Relocator::applyRelocation(Relocation &) {
  return Relocator::Unsupport;
}

void X86_32Relocator::scanRelocation(Relocation &, eld::IRBuilder &,
                                     ELFSection &, InputFile &, CopyRelocs &) {}

const char *X86_32Relocator::getName(Relocation::Type) const {
  return "<unsupported i386 relocation>";
}

Relocation::Size X86_32Relocator::getSize(Relocation::Type) const { return 0; }

uint32_t X86_32Relocator::getNumRelocs() const { return 0; }

} // namespace eld
