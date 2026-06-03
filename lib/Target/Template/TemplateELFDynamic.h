//===- TemplateELFDynamic.h------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
// Refer to eld/Target/ELFDynamic.h for additional hooks.
//===----------------------------------------------------------------------===//

#ifndef ELD_TEMPLATE_ELFDYNAMIC_SECTION_H
#define ELD_TEMPLATE_ELFDYNAMIC_SECTION_H

#include "eld/Target/ELFDynamic.h"

namespace eld {

class TemplateELFDynamic : public ELFDynamic {
public:
  TemplateELFDynamic(GNULDBackend &pParent, LinkerConfig &pConfig);
  ~TemplateELFDynamic();

private:
  void reserveTargetEntries() override;
  void applyTargetEntries() override;
};

} // namespace eld

#endif
