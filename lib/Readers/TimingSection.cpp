//===- TimingSection.cpp---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Readers/TimingSection.h"
#include "eld/Diagnostics/MsgHandler.h"
#include "eld/Fragment/TimingFragment.h"
#include "eld/Input/InputFile.h"
#include "eld/Readers/TimingSlice.h"

bool eld::TimingSection::readContents(llvm::StringRef SectionData,
                                      DiagnosticEngine *DiagEngine) {
  uint32_t currStart = 0;
  uint32_t bytesRead = 0;
  uint32_t size = SectionData.size();
  const std::string &InputFileName =
      getInputFile()->getInput()->decoratedPath();
  while (bytesRead < size) {
    auto end = SectionData.find('\0', currStart + 16);
    // should always end with null terminated string
    if (end == std::string::npos) {
      DiagEngine->raise(diag::no_string_in_timing)
          << getTimingSectionName() << InputFileName;
      return false;
    }
    addFragmentAndUpdateSize(eld::make<TimingFragment>(
        SectionData.slice(currStart, end), InputFileName, this, DiagEngine));
    bytesRead += end - currStart + 1;
    currStart = end + 1;
  }
  ASSERT(bytesRead == size, "Failed to read timing slice");
  return true;
}

eld::TimingSection::TimingSection(DiagnosticEngine *E,
                                  llvm::StringRef SectionData,
                                  uint32_t SectionSize, uint32_t Flags,
                                  InputFile *File)
    : ELFSection(LDFileFormat::Timing, ".note.qc.timing", Flags, /*EntSize=*/1,
                 /*AddrAlign=*/0, /*Type=*/0, /*Info=*/0, /*Link=*/nullptr,
                 /*SectionSize=*/0, /*PAddr=*/0) {
  // should hold at least two 8 byte integers plus module name
  setInputFile(File);
  const std::string &InputFileName = File->getInput()->decoratedPath();
  if (SectionSize <= 16) {
    E->raise(diag::unexpected_section_size)
        << SectionSize << getTimingSectionName() << InputFileName;
    return;
  }
  if (Flags) {
    E->raise(diag::invalid_section_flag)
        << Flags << getTimingSectionName() << InputFileName;
    return;
  }
  readContents(SectionData, E);
}
