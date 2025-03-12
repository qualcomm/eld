//===- TimingSlice.cpp-----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Readers/TimingSlice.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Diagnostics/DiagnosticPrinter.h"
#include "eld/Support/MsgHandling.h"
#include "llvm/Support/Chrono.h"
#include "llvm/Support/raw_ostream.h"
#include <ctime>

using namespace llvm::sys;
using namespace std::chrono;

// returns start time as date string in local time
// YYYY-MM-DD HH:MM:SS
std::string eld::TimingSlice::getDate() const {
  TimePoint<> T = system_clock::time_point{microseconds(m_BeginningOfTime)};
  std::string S;
  llvm::raw_string_ostream OS(S);
  llvm::format_provider<TimePoint<>>::format(T, OS, "%F %H:%M:%S");
  return OS.str();
}

int64_t eld::TimingSlice::microsToSeconds(uint64_t T) const {
  microseconds m(T);
  return duration_cast<seconds>(m).count();
}

int64_t eld::TimingSlice::getDurationSeconds() const {
  return microsToSeconds(m_Duration);
}

eld::TimingSlice::TimingSlice(llvm::StringRef Slice,
                              llvm::StringRef InputFileName,
                              DiagnosticEngine *DiagEngine) {

  ReadSlice(Slice, InputFileName, DiagEngine);
}

eld::TimingSlice::TimingSlice(uint64_t beginningOfTime, uint64_t duration,
                              llvm::StringRef name)
    : m_BeginningOfTime(beginningOfTime), m_Duration(duration),
      m_ModuleName(name) {}

bool eld::TimingSlice::ReadSlice(llvm::StringRef Slice,
                                 llvm::StringRef InputFileName,
                                 DiagnosticEngine *DiagEngine) {
  // should hold at least two 8 byte integers plus module name
  if (Slice.size() <= 16) {
    DiagEngine->raise(diag::unexpected_section_size)
        << Slice.size() << getTimingSectionName() << InputFileName;
    return false;
  }

  uint64_t buf[2];
  std::memcpy(buf, Slice.data(), 16);
  if (buf[0] > 0 && buf[1] > 0) {
    m_BeginningOfTime = buf[0];
    m_Duration = buf[1];
  } else {
    DiagEngine->raise(diag::invalid_timing_value)
        << buf[0] << getTimingSectionName() << InputFileName;
    return false;
  }

  // remaining bytes hold module name
  m_ModuleName = Slice.slice(16, Slice.size());
  return true;
}

void eld::TimingSlice::setData(uint64_t beginningOfTime, uint64_t duration) {
  m_BeginningOfTime = beginningOfTime;
  m_Duration = duration;
}
