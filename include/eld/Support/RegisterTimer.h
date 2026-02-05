//===- RegisterTimer.h-----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SUPPORT_REGISTERTIMER_H
#define ELD_SUPPORT_REGISTERTIMER_H

#include "llvm/ADT/MapVector.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/raw_ostream.h"
#include <iostream>

namespace eld {

class Input;

class RegisterTimer : public llvm::NamedRegionTimer {

public:
  // Params: Name -> Stats Description, Group -> Name of Sub-section in Linker
  // timing stats and string to "Group-by",
  // Enable -> Turn Timer On/Off.
  RegisterTimer(llvm::StringRef name, llvm::StringRef group, bool enable);

  ~RegisterTimer();

#ifdef __linux__
  static void setShouldRecordMemoryStats(bool shouldRecordMemoryStats) {
    ShouldRecordMemoryStats = shouldRecordMemoryStats;
  }

  /// A simple data structure to track memory usage information.
  struct MemoryUsageInfo {
    /// Current resident set size in kilobytes. Resident set size is the amount of virtual
    /// memory that is actually residing in memory.
    int64_t RSSCur = 0;
    /// Peak resident set size in kilobytes seen so far
    int64_t RSSPeak = 0;

    std::string getHumanReadableRSSCur() const {
      if (RSSCur > 1024)
        return std::to_string(RSSCur / 1024) + "MB";
      else
        return std::to_string(RSSCur) + "KB";
    }

    std::string getHumanReadableRSSPeak() const {
      if (RSSPeak > 1024)
        return std::to_string(RSSPeak / 1024) + "MB";
      else
        return std::to_string(RSSPeak) + "KB";
    }

    MemoryUsageInfo operator-(const MemoryUsageInfo &rhs) const {
      MemoryUsageInfo res;
      res.RSSCur = RSSCur - rhs.RSSCur;
      res.RSSPeak = RSSPeak - rhs.RSSPeak;
      return res;
    }

    MemoryUsageInfo &operator+=(const MemoryUsageInfo &rhs) {
      RSSCur += rhs.RSSCur;
      RSSPeak += rhs.RSSPeak;
      return *this;
    }

    MemoryUsageInfo operator+(const MemoryUsageInfo &rhs) {
      MemoryUsageInfo res = *this;
      res += rhs;
      return res;
    }
  };

  static void emitMemoryStats(llvm::raw_ostream &OS);
#endif
private:
  llvm::StringRef Name;
  llvm::StringRef Group;

#ifdef __linux__
  static bool ShouldRecordMemoryStats;
  /// Stores the memory usage information for each timer.
  ///
  /// MapVector is used here to preserve the insertion order. The order is
  /// important for the memory report to be easy to read and understand.
  static llvm::MapVector<llvm::StringRef,
                         llvm::MapVector<llvm::StringRef, MemoryUsageInfo>>
      AbsMemoryInfo;
  /// Stores the diff of memory usage information at the start and end timer for
  /// each timer
  static llvm::MapVector<llvm::StringRef,
                         llvm::MapVector<llvm::StringRef, MemoryUsageInfo>>
      DiffMemoryInfo;

  /// Stores the memory usage information at the start of the timer.
  MemoryUsageInfo StartMemInfo;

  /// Computes MemoryUsageInfo by parsing the virtual file /proc/self/status
  static MemoryUsageInfo getMemoryUsageInfo();

  /// Computes diff MemoryUsageInfo for the group by summing up DiffMemoryInfo
  /// for each member of the group. It is used to determine total amount of
  /// memory used by the group.
  static MemoryUsageInfo getGroupMemoryUsageInfo(llvm::StringRef groupName);

  /// Returns true if we should record memory for this timer.
  /// It returns false for all timers if link time memory report is not
  /// requested.
  /// It also returns false for timers that are called large number of times.
  /// This is because reading the virtual file /proc/self/status large number of
  /// times can be time-consuming.
  bool shouldRecordMemory() const;
#endif
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
