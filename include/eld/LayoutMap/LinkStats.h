//===- LinkStats.h---------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_LAYOUTMAP_LINKSTATS_H
#define ELD_LAYOUTMAP_LINKSTATS_H

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"
#include <optional>

namespace eld {

// FIXME: perhaps it should be named relaxation stats?
class LinkStats {
public:
  enum Kind : uint8_t { None, Relaxation };

  LinkStats(llvm::StringRef Name, Kind K) : StatsName(Name), StatsKind(K) {}

  // FIXME: Why virtual?
  virtual llvm::StringRef getStatName() const { return StatsName; }

  // FIXME: Why virtual?
  virtual void dumpStat(llvm::raw_ostream &OS) const = 0;

  virtual ~LinkStats() {}

  Kind getKind() const { return StatsKind; }

  bool isRelaxationKind() const { return StatsKind == Kind::Relaxation; }

private:
  llvm::StringRef StatsName;
  Kind StatsKind = None;
};

} // namespace eld

#endif
