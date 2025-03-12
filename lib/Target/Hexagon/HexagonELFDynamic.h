//===- HexagonELFDynamic.h-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_HEXAGON_ELFDYNAMIC_SECTION_H
#define ELD_HEXAGON_ELFDYNAMIC_SECTION_H

#include "eld/Target/ELFDynamic.h"

namespace eld {

enum {
  DT_HEXAGON_SYMSZ = 0x70000000,
  DT_HEXAGON_VER = 0x70000001,
  DT_HEXAGON_PLT = 0x70000002
};

class HexagonELFDynamic : public ELFDynamic {
public:
  HexagonELFDynamic(GNULDBackend &pParent, LinkerConfig &pConfig);
  ~HexagonELFDynamic();

private:
  void reserveTargetEntries() override;
  void applyTargetEntries() override;
};

} // namespace eld

#endif
