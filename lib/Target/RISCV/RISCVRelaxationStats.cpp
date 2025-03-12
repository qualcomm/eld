//===- RISCVRelaxationStats.cpp--------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "RISCVRelaxationStats.h"

using namespace eld;

void RISCVRelaxationStats::dumpStat(llvm::raw_ostream &OS) const {
  if (numBytesDeleted)
    OS << "# "
       << "RelaxationBytesDeleted"
       << " : " << numBytesDeleted << "\n";
  if (numBytesNotDeleted)
    OS << "# "
       << "RelaxationBytesMissed"
       << " : " << numBytesNotDeleted << "\n";
}
