//===- X86_32Relocator.h-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
// Refer to eld/Target/Relocator.h for additional hooks.
//===----------------------------------------------------------------------===//

#ifndef ELD_TARGET_X86_32_RELOCATOR_H
#define ELD_TARGET_X86_32_RELOCATOR_H

#include "X86_32LDBackend.h"
#include "eld/Target/Relocator.h"

namespace eld {

class X86_32Relocator : public Relocator {
public:
  X86_32Relocator(X86_32LDBackend &pParent, LinkerConfig &pConfig,
                  Module &pModule);

  Result applyRelocation(Relocation &) override;

  void scanRelocation(Relocation &, eld::IRBuilder &, ELFSection &, InputFile &,
                      CopyRelocs &) override;

  X86_32LDBackend &getTarget() override { return m_Target; }
  const X86_32LDBackend &getTarget() const override { return m_Target; }

  const char *getName(Relocation::Type) const override;

  Size getSize(Relocation::Type) const override;
  uint32_t getNumRelocs() const override;

private:
  X86_32LDBackend &m_Target;
};

} // namespace eld

#endif
