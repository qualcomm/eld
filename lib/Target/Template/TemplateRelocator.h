//===- TemplateRelocator.h-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
#ifndef TEMPLATE_RELOCATION_FACTORY_H
#define TEMPLATE_RELOCATION_FACTORY_H

#include "TemplateLDBackend.h"
#include "eld/Target/Relocator.h"
#include <mutex>

namespace eld {

class ResolveInfo;
class LinkerConfig;

/** \class TemplateRelocator
 *  \brief TemplateRelocator creates and destroys the Template relocations.
 *
 */
class TemplateRelocator : public Relocator {
public:
  TemplateRelocator(TemplateLDBackend &pParent, LinkerConfig &pConfig,
                    Module &pModule);

  Result applyRelocation(Relocation &pRelocation) override;

  void scanRelocation(Relocation &pReloc, eld::IRBuilder &pBuilder,
                      ELFSection &pSection, InputFile &pInput,
                      CopyRelocs &) override;

  // Handle partial linking
  void partialScanRelocation(Relocation &pReloc,
                             const ELFSection &pSection) override;

  TemplateLDBackend &getTarget() override { return m_Target; }

  const TemplateLDBackend &getTarget() const override { return m_Target; }

  const char *getName(Relocation::Type pType) const override;

  Size getSize(Relocation::Type pType) const override;

  uint32_t getNumRelocs() const override;

private:
  virtual void scanLocalReloc(InputFile &pInput, Relocation &pReloc,
                              eld::IRBuilder &pBuilder, ELFSection &pSection);

  virtual void scanGlobalReloc(InputFile &pInput, Relocation &pReloc,
                               eld::IRBuilder &pBuilder, ELFSection &pSection);

private:
  TemplateLDBackend &m_Target;
  std::mutex m_RelocMutex;
};

} // namespace eld

#endif
