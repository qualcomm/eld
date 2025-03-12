//===- TimingSection.h-----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_READERS_TIMINGSECTION_H
#define ELD_READERS_TIMINGSECTION_H

#include "eld/Readers/ELFSection.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/DataTypes.h"
#include <string>
#include <vector>

namespace eld {

class DiagnosticEngine;
class TimingSlice;
class InputFile;

class TimingSection : public ELFSection {
public:
  explicit TimingSection(DiagnosticEngine *diagEngine,
                         llvm::StringRef SectionData, uint32_t sh_size,
                         uint32_t sh_flag, InputFile *iptFile);

private:
  bool readContents(llvm::StringRef SectionData, DiagnosticEngine *DiagEngine);

  // return name of the timing section
  llvm::StringRef getTimingSectionName() const { return ".note.qc.timing"; }
};

} // namespace eld

#endif
