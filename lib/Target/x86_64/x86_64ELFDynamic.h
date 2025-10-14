//===- x86_64ELFDynamic.h----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_TARGET_X86_64_ELFDYNAMIC_H
#define ELD_TARGET_X86_64_ELFDYNAMIC_H

#include "eld/Target/ELFDynamic.h"

namespace eld {

class x86_64ELFDynamic : public ELFDynamic {
public:
  x86_64ELFDynamic(GNULDBackend &pParent, LinkerConfig &pConfig);

private:
  void reserveTargetEntries() override;
  void applyTargetEntries() override;
};
} // namespace eld
#endif
