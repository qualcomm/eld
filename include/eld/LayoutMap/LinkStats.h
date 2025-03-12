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

class LinkStats {
public:
  enum Kind : uint8_t { None, Relaxation };

  LinkStats(llvm::StringRef Name, Kind K) : m_Name(Name), m_Kind(K) {}

  virtual llvm::StringRef getStatName() const { return m_Name; }

  virtual void dumpStat(llvm::raw_ostream &OS) const = 0;

  virtual ~LinkStats() {}

  Kind getKind() const { return m_Kind; }

  bool isRelaxationKind() const { return m_Kind == Kind::Relaxation; }

private:
  llvm::StringRef m_Name;
  Kind m_Kind = None;
};

} // namespace eld

#endif
