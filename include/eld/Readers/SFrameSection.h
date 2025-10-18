//===- SFrameSection.h----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_READERS_SFRAMESECTION_H
#define ELD_READERS_SFRAMESECTION_H

#include "eld/Fragment/SFrameFragment.h"
#include "eld/Readers/ELFSection.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"

namespace eld {

class DiagnosticEngine;
class DiagnosticPrinter;
class Module;
class Relocation;
class RegionFragment;

/// \class SFrameSection
/// Represents an SFrame section (.sframe) containing stack unwinding
/// information
class SFrameSection : public ELFSection {
public:
  SFrameSection(std::string Name, DiagnosticEngine *diagEngine, uint32_t pType,
                uint32_t pFlag, uint32_t entSize, uint64_t pSize,
                int8_t cfaFixedFPOffset = 0, int8_t cfaFixedRAOffset = 0);

  static bool classof(const Section *S) {
    return S->getSectionKind() == Section::Kind::SFrame;
  }

  /// Process the raw SFrame section data and create fragments
  bool processSFrameSection();

  /// Create SFrame fragment containing the processed data
  bool createSFrameFragment();

  /// Get the main SFrame fragment
  SFrameFragment *getSFrameFragment() const { return m_SFrameFragment; }

  /// Get the raw section data
  llvm::ArrayRef<uint8_t> getData() const { return Data; }

  /// Set the raw section data
  void setData(llvm::ArrayRef<uint8_t> data);

  /// Validate SFrame section format
  bool validateSFrameFormat();

  /// Parse SFrame header from raw data
  bool parseSFrameHeader();

  /// Finish adding fragments to this section
  void finishAddingFragments(Module &M);

protected:
  /// The processed SFrame fragment
  SFrameFragment *m_SFrameFragment = nullptr;

  /// Raw section data
  std::vector<uint8_t> Data;

  /// Diagnostic engine for error reporting
  DiagnosticEngine *m_DiagEngine;

  /// Architecture-specific CFA fixed offsets
  int8_t m_CFAFixedFPOffset;
  int8_t m_CFAFixedRAOffset;

  /// Parse function descriptors from raw data
  bool parseFunctionDescriptors();

  /// Parse frame row entries from raw data
  bool parseFrameRowEntries();
};

} // namespace eld

#endif // ELD_READERS_SFRAMESECTION_H
