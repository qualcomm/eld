//===- AArch64ELFDynamic.h-------------------------------------------------===//
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
#ifndef TARGET_AARCH64_AARCH64ELFDYNAMIC_H
#define TARGET_AARCH64_AARCH64ELFDYNAMIC_H

#include "eld/Target/ELFDynamic.h"

namespace eld {

class AArch64ELFDynamic : public ELFDynamic {
public:
  AArch64ELFDynamic(GNULDBackend &pParent, LinkerConfig &pConfig);
  ~AArch64ELFDynamic();

private:
  void reserveTargetEntries() override;
  void applyTargetEntries() override;
};

} // namespace eld

#endif
