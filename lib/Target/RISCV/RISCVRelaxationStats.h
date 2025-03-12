//===- RISCVRelaxationStats.h----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef RISCV_RELAXATION_STATS_H
#define RISCV_RELAXATION_STATS_H

#include "eld/LayoutMap/LinkStats.h"

namespace eld {
class RISCVRelaxationStats : public LinkStats {
public:
  RISCVRelaxationStats()
      : LinkStats("RelaxationStats", LinkStats::Kind::Relaxation) {}

  static bool classof(const LinkStats *R) { return R->isRelaxationKind(); }

  void dumpStat(llvm::raw_ostream &OS) const override;

  void addBytesDeleted(size_t bytes) { numBytesDeleted += bytes; }
  void addBytesNotDeleted(size_t bytes) { numBytesNotDeleted += bytes; }

private:
  size_t numBytesDeleted = 0;
  size_t numBytesNotDeleted = 0;
};
} // namespace eld

#endif
