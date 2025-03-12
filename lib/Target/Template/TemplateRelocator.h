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
  ~TemplateRelocator();

  Result applyRelocation(Relocation &pRelocation);

  void scanRelocation(Relocation &pReloc, eld::IRBuilder &pBuilder,
                      ELFSection &pSection, Input &pInput, CopyRelocs &);

  // Handle partial linking
  void partialScanRelocation(Relocation &pReloc, const ELFSection &pSection);

  TemplateLDBackend &getTarget() { return m_Target; }

  const TemplateLDBackend &getTarget() const { return m_Target; }

  const char *getName(Relocation::Type pType) const;

  Size getSize(Relocation::Type pType) const;

private:
  virtual void scanLocalReloc(Input &pInput, Relocation &pReloc,
                              eld::IRBuilder &pBuilder, ELFSection &pSection);

  virtual void scanGlobalReloc(Input &pInput, Relocation &pReloc,
                               eld::IRBuilder &pBuilder, ELFSection &pSection);

private:
  TemplateLDBackend &m_Target;
  std::mutex m_RelocMutex;
};

} // namespace eld

#endif
