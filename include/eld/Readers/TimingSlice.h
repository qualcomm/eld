//===- TimingSlice.h-------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_READERS_TIMINGSLICE_H
#define ELD_READERS_TIMINGSLICE_H

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/DataTypes.h"
#include <chrono>
#include <string>

namespace eld {

class DiagnosticEngine;

// Represents the data of a single module in timing section
class TimingSlice {
public:
  explicit TimingSlice(llvm::StringRef Slice, llvm::StringRef InputFileName,
                       DiagnosticEngine *DiagEngine);

  explicit TimingSlice(uint64_t beginningOfTime, uint64_t duration,
                       llvm::StringRef name);

  bool ReadSlice(llvm::StringRef Slice, llvm::StringRef InputFileName,
                 DiagnosticEngine *DiagEngine);

  void setData(uint64_t beginningOfTime, uint64_t duration);

  std::string getDate() const;

  int64_t getDurationSeconds() const;

  // duration in mircoseconds
  uint64_t getDuration() const { return m_Duration; }

  // start time unix epoch microseconds
  uint64_t getBeginningOfTime() const { return m_BeginningOfTime; }

  // name of input file to which this timing data belongs
  llvm::StringRef getModuleName() const { return m_ModuleName; }

  // return name of the timing section
  llvm::StringRef getTimingSectionName() const { return ".note.qc.timing"; }

private:
  int64_t microsToSeconds(uint64_t micros) const;

private:
  uint64_t m_BeginningOfTime;
  uint64_t m_Duration;
  llvm::StringRef m_ModuleName;
};

} // namespace eld
#endif
