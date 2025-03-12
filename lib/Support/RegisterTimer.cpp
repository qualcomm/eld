//===- RegisterTimer.cpp---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Support/RegisterTimer.h"
#include "eld/Input/Input.h"
#include "eld/Support/MsgHandling.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"

using namespace eld;

eld::Timer::Timer(std::string Name, std::string Description, bool IsEnabled)
    : Name(Name), Description(Description), m_CompilationStartTime(0),
      m_CompilationDuration(0) {
  threadCount = 1;
}

eld::Timer::Timer(const Input *In, std::string Description, bool IsEnabled)
    : Name(In->decoratedPath()), Description(Description), m_Input(In),
      m_CompilationStartTime(0), m_CompilationDuration(0) {
  threadCount = 1;
}

eld::Timer::Timer(std::string Name, uint64_t StartTime, int64_t Duration,
                  std::string Description, bool IsEnabled)
    : Name(Name), Description(Description), m_CompilationStartTime(StartTime),
      m_CompilationDuration(Duration) {
  threadCount = 1;
}

bool eld::Timer::start() {
  if (isRunning())
    return true;
  StartTime = llvm::TimeRecord::getCurrentTime(false);
  isStarted = true;
  return true;
}

bool eld::Timer::stop() {
  if (!isRunning())
    return true;
  llvm::TimeRecord TR = llvm::TimeRecord::getCurrentTime(false);
  TR -= StartTime;
  Total += TR;
  isStarted = false;
  return true;
}

eld::Timer::~Timer() { stop(); }

void Timer::print(llvm::raw_ostream &Os) {
  ASSERT(!isRunning(), "Timer " + Name + " is already running");
  printVal(Total.getUserTime(), Os);
  printVal(Total.getSystemTime(), Os);
  printVal(Total.getProcessTime(), Os);
  printVal(Total.getWallTime(), Os);
  Os << Name;
  if (!Description.empty())
    Os << " ( " << Description << " ) ";
  Os << "\n";
}

void Timer::printVal(double Val, llvm::raw_ostream &Os) {
  Os << llvm::format("  %7.4f ", Val);
}
