//===- ProgressBar.h-------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SUPPORT_PROGRESSBAR_H
#define ELD_SUPPORT_PROGRESSBAR_H

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <cassert>
#include <iomanip>
#include <sstream>
#include <string>

namespace eld {
class ProgressBar {
public:
  ProgressBar(unsigned int Total, unsigned int Width, bool Enabled,
              llvm::raw_ostream &OutputStream = llvm::errs())
      : TotalTicks{Total}, MaxBarWidth{Width}, BarWidth{getBarWidth(Width, {})},
        Enabled(Enabled), OutputStream(OutputStream),
        Interactive(OutputStream.is_displayed()) {}

  unsigned int operator++() { return ++Ticks; }

  void display(bool IsSpin) {
    if (!Enabled)
      return;
    const char SpinChars[] = "/-\\|";
    assert(Ticks <= TotalTicks && "Progress bar exceeded its total tick count");
    BarWidth = getBarWidth(MaxBarWidth, CurrentStep);
    float Progress = TotalTicks ? (float)Ticks / TotalTicks : 1.0f;
    uint32_t Position = (int)(BarWidth * Progress);

    const long long TimeElapsed = static_cast<long long>(
        (llvm::TimeRecord::getCurrentTime() - StartTime).getWallTime() * 1000);

    OutputStream << "Linking : ";
    OutputStream << "[";

    for (uint32_t Index = 0; Index < BarWidth; ++Index) {
      if (Index < Position)
        OutputStream << CompleteChar;
      else if (Index == Position) {
        if (!IsSpin) {
          OutputStream << ">";
        } else {
          OutputStream << SpinChars[SpinCount++ % sizeof(SpinChars)];
        }
      } else
        OutputStream << IncompleteChar;
    }
    OutputStream << "] " << int(Progress * 100.0) << "% "
                 << formatElapsed(TimeElapsed);
    if (!CurrentStep.empty())
      OutputStream << "  " << CurrentStep;
    if (Interactive)
      OutputStream << '\r';
    else
      OutputStream << '\n';
    OutputStream.flush();
  }

  void displaySpin() { display(true); }

  void incrementAndDisplayProgress(llvm::StringRef Step = {}) {
    if (!Enabled)
      return;
    clear();
    CurrentStep = Step.str();
    SpinCount = 0;
    this->operator++();
    display(false);
  }

  void updateTicks(uint32_t Total) { TotalTicks = Total; }

  void addMoreTicks(uint32_t Count) { TotalTicks += Count; }

  void clear() {
    if (!Enabled || !Interactive)
      return;
    clearLine();
    OutputStream.flush();
  }

  void finish() {
    if (!Enabled)
      return;
    clear();
    CurrentStep = "Completed";
    if (Ticks != TotalTicks)
      Ticks = TotalTicks;
    display(false);
    if (Interactive)
      OutputStream << '\n';
    OutputStream.flush();
    Enabled = false;
  }

  void stop() {
    if (!Enabled)
      return;
    done();
    Enabled = false;
  }

  void done() {
    if (!Enabled)
      return;
    if (Interactive) {
      clearLine();
      OutputStream << '\n';
    }
    OutputStream.flush();
  }

  ~ProgressBar() {
    if (Enabled)
      done();
  }

private:
  uint32_t Ticks = 0;
  uint32_t TotalTicks = 0;
  uint32_t MaxBarWidth = 0;
  uint32_t BarWidth = 0;
  const char CompleteChar = '=';
  const char IncompleteChar = ' ';
  const llvm::TimeRecord StartTime = llvm::TimeRecord::getCurrentTime();
  bool Enabled = false;
  uint32_t SpinCount = 0;
  llvm::raw_ostream &OutputStream;
  bool Interactive = false;
  std::string CurrentStep;

  static std::string formatElapsed(long long Milliseconds) {
    if (Milliseconds < 1000)
      return std::to_string(Milliseconds) + "ms";

    const auto Seconds = Milliseconds / 1000;
    if (Seconds < 60) {
      std::ostringstream Stream;
      Stream << std::fixed << std::setprecision(2)
             << static_cast<double>(Milliseconds) / 1000.0 << "s";
      return Stream.str();
    }

    const auto Minutes = Seconds / 60;
    const auto RemainingSeconds = Seconds % 60;
    if (Minutes < 60) {
      std::ostringstream Stream;
      Stream << Minutes << "m " << std::setfill('0') << std::setw(2)
             << RemainingSeconds << "s";
      return Stream.str();
    }

    const auto Hours = Minutes / 60;
    const auto RemainingMinutes = Minutes % 60;
    std::ostringstream Stream;
    Stream << Hours << "h " << std::setfill('0') << std::setw(2)
           << RemainingMinutes << "m " << std::setw(2) << RemainingSeconds
           << "s";
    return Stream.str();
  }

  static unsigned int getBarWidth(unsigned int MaximumWidth,
                                  llvm::StringRef Step) {
    constexpr unsigned int FixedWidth = 30;
    const unsigned int StepWidth = Step.empty() ? 0 : Step.size() + 2;
    const unsigned int TerminalWidth = llvm::sys::Process::StandardErrColumns();
    if (!TerminalWidth)
      return MaximumWidth;
    if (TerminalWidth <= FixedWidth + StepWidth)
      return 1;
    return std::min(MaximumWidth, TerminalWidth - FixedWidth - StepWidth);
  }

  void clearLine() {
    if (OutputStream.has_colors())
      OutputStream << "\033[2K\r";
    else
      OutputStream << '\r'
                   << std::string(BarWidth + 34 + CurrentStep.size(), ' ')
                   << '\r';
  }
};

} // namespace eld
#endif // ELD_SUPPORT_PROGRESSBAR_H
