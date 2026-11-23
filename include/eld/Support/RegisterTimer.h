//===- RegisterTimer.h-----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SUPPORT_REGISTERTIMER_H
#define ELD_SUPPORT_REGISTERTIMER_H

#include "llvm/Support/Timer.h"
#include "llvm/Support/raw_ostream.h"

namespace eld {

class Input;

class RegisterTimer {
public:
  // Params:
  // - Name: timer name (also used as description)
  // - Phase: top-level linker phase
  // (Initialize/Read/Resolve/Optimize/Layout/Emit/Plugins)
  // - Enable: if false, the timer is a no-op
  explicit RegisterTimer(llvm::StringRef Name, llvm::StringRef Phase,
                         bool Enable);

  ~RegisterTimer();

  RegisterTimer(const RegisterTimer &) = delete;
  RegisterTimer &operator=(const RegisterTimer &) = delete;

  llvm::Timer *getTimer() const { return T; }

  // Print a hierarchical timing report to OS, grouping timers under their
  // phases in pipeline order
  // (Initialize -> Read -> Resolve -> Optimize -> Layout -> Emit -> Plugins).
  static void printTimingReport(llvm::raw_ostream &OS);

private:
  llvm::Timer *T = nullptr;
};

} // namespace eld

#endif
