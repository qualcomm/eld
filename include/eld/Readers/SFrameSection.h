//===- SFrameSection.h-----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_READERS_SFRAMESECTION_H
#define ELD_READERS_SFRAMESECTION_H

#include "eld/Readers/ELFSection.h"

namespace eld {

class DiagnosticEngine;
class SFrameFragment;

/// SFrameSection represents an input .sframe section.
/// It validates the SFrame header and creates SFrameFragment instances
/// for merging into the output.
class SFrameSection : public ELFSection {
public:
  SFrameSection(std::string Name, DiagnosticEngine *DiagEngine, uint32_t Type,
                uint32_t Flags, uint32_t EntSize, uint64_t Size);

  static bool classof(const Section *S) {
    return S->getSectionKind() == Section::Kind::SFrame;
  }

  /// Parse and validate the .sframe section header.
  /// Returns true on success, false on error.
  bool parseSFrameSection();

  /// Get the raw section data.
  llvm::ArrayRef<uint8_t> getData() const { return Data; }

  /// Get the number of FDEs parsed from the header.
  uint32_t getNumFDEs() const { return NumFDEs; }

  /// Get the number of FREs parsed from the header.
  uint32_t getNumFREs() const { return NumFREs; }

  /// Get the ABI identifier from the header.
  uint8_t getABI() const { return ABI; }

private:
  llvm::ArrayRef<uint8_t> Data;
  DiagnosticEngine *TheDiagEngine = nullptr;
  uint32_t NumFDEs = 0;
  uint32_t NumFREs = 0;
  uint8_t ABI = 0;
};

} // namespace eld

#endif
