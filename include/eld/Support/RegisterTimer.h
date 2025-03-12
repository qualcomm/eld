//===- RegisterTimer.h-----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SUPPORT_REGISTERTIMER_H
#define ELD_SUPPORT_REGISTERTIMER_H

#include "llvm/Support/Timer.h"

namespace eld {

class Input;

class RegisterTimer : public llvm::NamedRegionTimer {

public:
  // Params: Name -> Stats Description, Group -> Name of Sub-section in Linker
  // timing stats and string to "Group-by",
  // Enable -> Turn Timer On/Off.
  RegisterTimer(llvm::StringRef Name, llvm::StringRef Group, bool Enable)
      : NamedRegionTimer(Name, Name, Group, Group, Enable) {}

  ~RegisterTimer() {}
};

class Timer {
public:
  // Generic timer.
  Timer(std::string Name, std::string Description = "", bool Enable = true);
  // Timer for inputs.
  Timer(const Input *in, std::string Description = "", bool Enable = true);
  // Timer for Compilation time.
  Timer(std::string Name, uint64_t startTime, int64_t duration,
        std::string Description = "", bool Enable = true);

  ~Timer();

  bool start();

  bool stop();

  bool clear();

  void print(llvm::raw_ostream &os);

  void printVal(double val, llvm::raw_ostream &os);

  bool isRunning() const { return isStarted; }

  std::string getName() const { return Name; }

  std::string getDescription() const { return Description; }

  const llvm::TimeRecord &getTimerTotal() const { return Total; }

  const llvm::TimeRecord &getStartTime() const { return StartTime; }

  const uint64_t &getCompilationStartTime() const {
    return m_CompilationStartTime;
  }

  const int64_t &getCompilationDuration() const {
    return m_CompilationDuration;
  }

  const Input *getInput() const { return m_Input; }

  bool hasInput() const { return m_Input != nullptr; }

  void setThreadCount(int val) { threadCount = val; }

  int getThreadCount() const { return threadCount; }

private:
  bool isStarted = false;
  int threadCount;
  std::string Name;
  std::string Description;
  llvm::TimeRecord Total;
  llvm::TimeRecord StartTime;
  const Input *m_Input = nullptr;
  // The below attributes store compilation time
  const uint64_t m_CompilationStartTime;
  const int64_t m_CompilationDuration;
};

} // namespace eld

#endif
