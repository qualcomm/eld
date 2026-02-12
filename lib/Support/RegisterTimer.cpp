//===- RegisterTimer.cpp---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Support/RegisterTimer.h"
#include "llvm/ADT/StringMap.h"
#include <memory>
#include <mutex>

namespace {

class RegisterTimerRegistry {
public:
  static RegisterTimerRegistry &instance() {
    static RegisterTimerRegistry Registry;
    return Registry;
  }

  llvm::Timer &getOrCreateTimer(llvm::StringRef Name,
                                llvm::StringRef Description,
                                llvm::StringRef GroupName,
                                llvm::StringRef GroupDescription) {
    std::lock_guard<std::mutex> Lock(Mu);

    GroupTimers &GroupEntry = Groups[GroupName];
    if (!GroupEntry.Group)
      GroupEntry.Group = std::make_unique<llvm::TimerGroup>(
          GroupName, GroupDescription, /*PrintOnExit=*/true);

    llvm::Timer &T = GroupEntry.Timers[Name];
    if (!T.isInitialized())
      T.init(Name, Description, *GroupEntry.Group);

    return T;
  }

private:
  struct GroupTimers {
    std::unique_ptr<llvm::TimerGroup> Group;
    llvm::StringMap<llvm::Timer> Timers;
  };

  std::mutex Mu;
  llvm::StringMap<GroupTimers> Groups;
};

} // namespace

namespace eld {

RegisterTimer::RegisterTimer(llvm::StringRef Name, llvm::StringRef Group,
                             bool Enable)
    : RegisterTimer(Name, Name, Group, Group, Enable) {}

RegisterTimer::RegisterTimer(llvm::StringRef Name, llvm::StringRef Description,
                             llvm::StringRef Group,
                             llvm::StringRef GroupDescription, bool Enable) {
  if (!Enable)
    return;

  T = &RegisterTimerRegistry::instance().getOrCreateTimer(
      Name, Description, Group, GroupDescription);
  Region.emplace(*T);
}
} // namespace eld
