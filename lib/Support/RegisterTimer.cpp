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
#include <fstream>

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

llvm::MapVector<
    llvm::StringRef,
    llvm::MapVector<llvm::StringRef, RegisterTimer::MemoryUsageInfo>>
    RegisterTimer::DiffMemoryInfo;
llvm::MapVector<
    llvm::StringRef,
    llvm::MapVector<llvm::StringRef, RegisterTimer::MemoryUsageInfo>>
    RegisterTimer::AbsMemoryInfo;

RegisterTimer::RegisterTimer(llvm::StringRef name, llvm::StringRef group,
                             bool enable)
    : NamedRegionTimer(name, name, group, group, enable), Name(name),
      Group(group) {
  if (!shouldRecordMemory())
    return;
  StartMemInfo = getMemoryUsageInfo();
  // Required to preserve the order!
  DiffMemoryInfo[Group];
}

RegisterTimer::~RegisterTimer() {
  if (!shouldRecordMemory())
    return;
  MemoryUsageInfo curMemInfo = getMemoryUsageInfo();
  DiffMemoryInfo[Group][Name] += curMemInfo - StartMemInfo;
  AbsMemoryInfo[Group][Name] = curMemInfo;
  // llvm::errs() << "Storing memory info for " << Group << ":" << Name << "\n";
}

#ifdef __linux__
bool RegisterTimer::ShouldRecordMemoryStats = false;

RegisterTimer::MemoryUsageInfo RegisterTimer::getMemoryUsageInfo() {
  std::ifstream statusFile("/proc/self/status");
  std::stringstream content;
  content << statusFile.rdbuf();
  std::string line;
  MemoryUsageInfo memInfo;
  while (std::getline(content, line)) {
    std::size_t pos = line.find(":");
    if (pos == std::string::npos)
      continue;
    std::string key = line.substr(0, pos);
    std::string valueStr = line.substr(pos + 1);
    llvm::StringRef valueStrRef = valueStr;
    valueStrRef = valueStrRef.trim();
    valueStrRef.consume_back_insensitive(" kb");
    if (key == "VmHWM") {
      valueStrRef.getAsInteger(/*Radix=*/10, memInfo.RSSPeak);
    } else if (key == "VmRSS") {
      valueStrRef.getAsInteger(/*Radix=*/10, memInfo.RSSCur);
    }
  }
  return memInfo;
}

void RegisterTimer::emitMemoryStats(llvm::raw_ostream &OS) {
  for (auto &group : DiffMemoryInfo) {
    llvm::StringRef groupName = group.first;
    // llvm::errs() << "Writing memory stats for group: " << groupName << "\n";
    const auto &groupMembersMemInfo = group.second;
    OS << "===" << std::string(73, '-') << "===" << "\n";
    unsigned padding = (80 - groupName.size()) / 2;
    if (padding > 80)
      padding = 0;
    OS.indent(padding);
    OS << groupName << "\n";
    OS << "===" << std::string(73, '-') << "===" << "\n";
    MemoryUsageInfo groupMemInfo = getGroupMemoryUsageInfo(groupName);
    OS << "Total resident set size change: "
       << groupMemInfo.getHumanReadableRSSCur() << "\n";
    OS << "\n";
    OS << "   ------RSS------";
    OS << "   -RSS peak-";
    OS << "   ---Name---";
    OS << "\n";
    for (const auto &elem : groupMembersMemInfo) {
      llvm::StringRef name = elem.first;
      const auto &memInfo = elem.second;
      const auto &absMemInfo = AbsMemoryInfo[groupName][name];
      OS << "   "
         << llvm::format("%6s(+%6s)",
                         absMemInfo.getHumanReadableRSSCur().c_str(),
                         memInfo.getHumanReadableRSSCur().c_str());
      OS << std::string(3, ' ')
         << llvm::format("%6s", absMemInfo.getHumanReadableRSSPeak().c_str());
      OS << std::string(7, ' ') << name << "\n";
    }
    OS << "\n\n";
  }
}

bool RegisterTimer::shouldRecordMemory() const {
  if (!ShouldRecordMemoryStats)
    return false;
  // All these timers are not suitable for memory tracking because they are
  // triggered large number of times.
  if (Group == "Symbol Resolution" || Name == "VisitSymbol" ||
      Name == "VisitSections" || Name == "Sort Sections" ||
      Name == "Evaluate Expressions")
    return false;
  return true;
}

RegisterTimer::MemoryUsageInfo
RegisterTimer::getGroupMemoryUsageInfo(llvm::StringRef groupName) {
  MemoryUsageInfo memInfo;
  for (const auto &elem : DiffMemoryInfo[groupName]) {
    memInfo += elem.second;
  }
  return memInfo;
}
#endif