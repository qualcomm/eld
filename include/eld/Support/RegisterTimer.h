//===- RegisterTimer.h-----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SUPPORT_REGISTERTIMER_H
#define ELD_SUPPORT_REGISTERTIMER_H

#include "llvm/Support/Timer.h"
#include <optional>

namespace eld {

class Input;

class RegisterTimer {
public:
  // Params:
  // - Name: timer name (also used as description by default)
  // - Group: TimerGroup name (also used as group description by default)
  // - Enable: if false, the timer is a no-op
  explicit RegisterTimer(llvm::StringRef Name, llvm::StringRef Group,
                         bool Enable);

  // Like the 3-arg ctor, but allows custom timer/group descriptions.
  RegisterTimer(llvm::StringRef Name, llvm::StringRef Description,
                llvm::StringRef Group, llvm::StringRef GroupDescription,
                bool Enable);

  llvm::Timer *getTimer() const { return T; }

private:
  llvm::Timer *T = nullptr;
  std::optional<llvm::TimeRegion> Region;
};

} // namespace eld

#endif
