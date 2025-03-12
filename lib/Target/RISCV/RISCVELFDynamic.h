//===- RISCVELFDynamic.h---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_RISCV_ELFDYNAMIC_SECTION_H
#define ELD_RISCV_ELFDYNAMIC_SECTION_H

#include "eld/Target/ELFDynamic.h"

namespace eld {

class RISCVELFDynamic : public ELFDynamic {
public:
  RISCVELFDynamic(GNULDBackend &pParent, LinkerConfig &pConfig);
  ~RISCVELFDynamic();

private:
  void reserveTargetEntries() override;
  void applyTargetEntries() override;
};

} // namespace eld

#endif
