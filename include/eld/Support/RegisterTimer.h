//===- RegisterTimer.h-----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SUPPORT_REGISTERTIMER_H
#define ELD_SUPPORT_REGISTERTIMER_H

#include "llvm/Support/Timer.h"
#include "llvm/Support/raw_ostream.h"
#include <optional>

namespace eld {

class Input;

class RegisterTimer {
public:
  // Params:
  // - Name: timer name (also used as description by default)
  // - Phase: top-level linker phase
  // (Initialize/Read/Resolve/Layout/Emit/Plugins)
  // - Enable: if false, the timer is a no-op
  explicit RegisterTimer(llvm::StringRef Name, llvm::StringRef Phase,
                         bool Enable);

  // Like the 3-arg ctor, but allows custom timer/group descriptions.
  RegisterTimer(llvm::StringRef Name, llvm::StringRef Description,
                llvm::StringRef Phase, llvm::StringRef PhaseDescription,
                bool Enable);

  llvm::Timer *getTimer() const { return T; }

  // Print a hierarchical timing report to OS, grouping timers under their
  // phases in pipeline order (Initialize -> Read -> Resolve -> Layout -> Emit).
  static void printTimingReport(llvm::raw_ostream &OS);

private:
  llvm::Timer *T = nullptr;
  std::optional<llvm::TimeRegion> Region;
};

} // namespace eld

#endif
