//===- ARMELFDynamic.h-----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

//===- ARMELFDynamic.h ----------------------------------------------------===//
//===----------------------------------------------------------------------===//
#ifndef ELD_ARM_ELFDYNAMIC_SECTION_H
#define ELD_ARM_ELFDYNAMIC_SECTION_H

#include "eld/Target/ELFDynamic.h"

namespace eld {

class ARMELFDynamic : public ELFDynamic {
public:
  ARMELFDynamic(GNULDBackend &pParent, LinkerConfig &pConfig);
  ~ARMELFDynamic();

private:
  void reserveTargetEntries() override;
  void applyTargetEntries() override;
};

} // namespace eld

#endif
