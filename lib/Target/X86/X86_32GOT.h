//===- X86_32GOT.h--------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_TARGET_X86_32_GOT_H
#define ELD_TARGET_X86_32_GOT_H

#include "eld/Core/Module.h"
#include "eld/Fragment/GOT.h"

namespace eld {

class X86_32GOTPLT0 : public GOT {
public:
  X86_32GOTPLT0(ELFSection *pOutput, Module *pModule)
      : GOT(GOT::GOTPLT0, pOutput, nullptr, 4, 12), m_Module(pModule),
        m_Value{} {
    if (pOutput)
      pOutput->addFragmentAndUpdateSize(this);
  }

  llvm::ArrayRef<uint8_t> getContent() const override;

  static X86_32GOTPLT0 *Create(ELFSection *pOutput, Module *pModule);

private:
  Module *m_Module;
  mutable uint8_t m_Value[12];
};

} // namespace eld

#endif
