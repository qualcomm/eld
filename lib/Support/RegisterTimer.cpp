//===- RegisterTimer.cpp---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Support/RegisterTimer.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Format.h"
#include <cassert>
#include <memory>
#include <vector>

namespace {

// Pipeline order for the hierarchical report.
static const llvm::StringRef PipelineOrder[] = {
    "Initialize", "Read", "Resolve", "Optimize", "Layout", "Emit", "Plugins",
};

class RegisterTimerRegistry {
public:
  static RegisterTimerRegistry &instance() {
    static RegisterTimerRegistry Registry;
    return Registry;
  }

  llvm::Timer &getOrCreateTimer(llvm::StringRef Name, llvm::StringRef Phase) {
    GroupTimers &GroupEntry = Groups[Phase];
    if (!GroupEntry.Group)
      GroupEntry.Group = std::make_unique<llvm::TimerGroup>(
          Phase, Phase, /*PrintOnExit=*/false);

    if (GroupEntry.Timers.find(Name) == GroupEntry.Timers.end())
      GroupEntry.Order.push_back(Name.str());

    llvm::Timer &T = GroupEntry.Timers[Name];
    if (!T.isInitialized())
      T.init(Name, Name, *GroupEntry.Group);

    return T;
  }

  void startTimer(llvm::Timer &Timer) {
    assert(!ActiveTimer && "Timers must not overlap");
    Timer.startTimer();
    ActiveTimer = &Timer;
  }

  void stopTimer(llvm::Timer &Timer) {
    assert(ActiveTimer == &Timer && "Stopping an inactive timer");
    Timer.stopTimer();
    ActiveTimer = nullptr;
  }

  struct TimerData {
    std::string Name;
    double Wall = 0.0;
    double User = 0.0;
    double Sys = 0.0;
  };

  struct PhaseData {
    std::string Name;
    double Wall = 0.0;
    double User = 0.0;
    double Sys = 0.0;
    std::vector<TimerData> Timers; // in first-seen (linker flow) order
  };

  // Snapshot timer data after linker work completes and return phases in
  // pipeline order with timers in first-seen order.
  std::vector<PhaseData> snapshot() {
    assert(!ActiveTimer && "Timing report requires completed timer scopes");
    std::vector<PhaseData> Result;
    for (llvm::StringRef Phase : PipelineOrder) {
      auto It = Groups.find(Phase);
      if (It == Groups.end())
        continue;
      PhaseData PD;
      PD.Name = Phase;
      for (const std::string &Name : It->second.Order) {
        llvm::TimeRecord TR = It->second.Timers[Name].getTotalTime();
        TimerData TD;
        TD.Name = Name;
        TD.Wall = TR.getWallTime();
        TD.User = TR.getUserTime();
        TD.Sys = TR.getSystemTime();
        PD.Wall += TD.Wall;
        PD.User += TD.User;
        PD.Sys += TD.Sys;
        PD.Timers.push_back(std::move(TD));
      }
      Result.push_back(std::move(PD));
    }
    return Result;
  }

private:
  struct GroupTimers {
    std::unique_ptr<llvm::TimerGroup> Group;
    llvm::StringMap<llvm::Timer> Timers;
    std::vector<std::string> Order; // insertion order
  };

  llvm::StringMap<GroupTimers> Groups;
  llvm::Timer *ActiveTimer = nullptr;
};

} // namespace

namespace eld {

RegisterTimer::RegisterTimer(llvm::StringRef Name, llvm::StringRef Phase,
                             bool Enable) {
  if (!Enable)
    return;

  T = &RegisterTimerRegistry::instance().getOrCreateTimer(Name, Phase);
  RegisterTimerRegistry::instance().startTimer(*T);
}

RegisterTimer::~RegisterTimer() {
  if (T)
    RegisterTimerRegistry::instance().stopTimer(*T);
}

void RegisterTimer::printTimingReport(llvm::raw_ostream &OS) {
  auto Phases = RegisterTimerRegistry::instance().snapshot();
  if (Phases.empty())
    return;

  double TotalWall = 0.0;
  double TotalUser = 0.0;
  double TotalSys = 0.0;
  for (auto &P : Phases) {
    TotalWall += P.Wall;
    TotalUser += P.User;
    TotalSys += P.Sys;
  }

  // Compute name field width from the longest name actually present.
  // Phase rows have no indent; sub-timer rows have 2-space indent, so their
  // effective text budget is NW-2.  We size NW so all names fit untruncated.
  int NW = (int)llvm::StringRef("Phase / Timer").size();
  for (auto &P : Phases) {
    NW = std::max(NW, (int)P.Name.size());
    for (auto &T : P.Timers)
      NW = std::max(NW, (int)T.Name.size() + 2); // +2 for sub-timer indent
  }
  NW += 2; // a little breathing room

  // Separator line: "  " + NW dashes + "  " + 4*(8+2) number columns + "  " + 5
  // pct
  int SepLen = 2 + NW + 2 + 8 + 2 + 8 + 2 + 8 + 2 + 5;
  std::string Sep = "  " + std::string(SepLen - 2, '-') + "\n";

  // Banner: "===---------===", same width as SepLen.
  std::string Banner = "===" + std::string(SepLen - 6, '-') + "===";
  std::string Title = "ELD Link Timing Report";
  int TitlePad = (SepLen - (int)Title.size()) / 2;
  std::string TitleLine(TitlePad, ' ');
  TitleLine += Title;

  OS << "\n" << Banner << "\n";
  OS << TitleLine << "\n";
  OS << Banner << "\n";
  OS << llvm::format("  %-*s  %8s  %8s  %8s  %5s\n", NW, "Phase / Timer",
                     "Wall(s)", "User(s)", "Sys(s)", "%");
  OS << Sep;

  for (auto &P : Phases) {
    double Pct = (TotalWall > 0.0) ? 100.0 * P.Wall / TotalWall : 0.0;
    OS << llvm::format("  %-*s  %8.4f  %8.4f  %8.4f  %5.1f%%\n", NW,
                       P.Name.c_str(), P.Wall, P.User, P.Sys, Pct);
    for (auto &T : P.Timers) {
      double TPct = (TotalWall > 0.0) ? 100.0 * T.Wall / TotalWall : 0.0;
      std::string Indented = "  " + T.Name;
      OS << llvm::format("  %-*s  %8.4f  %8.4f  %8.4f  %5.1f%%\n", NW,
                         Indented.c_str(), T.Wall, T.User, T.Sys, TPct);
    }
  }

  OS << Sep;
  OS << llvm::format("  %-*s  %8.4f  %8.4f  %8.4f\n", NW, "Total", TotalWall,
                     TotalUser, TotalSys);
  OS << "\n";
}

} // namespace eld
