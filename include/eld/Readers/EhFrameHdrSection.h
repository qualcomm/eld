//===- EhFrameHdrSection.h-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#ifndef ELD_READERS_EHFRAMEHDRSECTION_H
#define ELD_READERS_EHFRAMEHDRSECTION_H

#include "eld/Fragment/EhFrameHdrFragment.h"
#include "eld/Readers/ELFSection.h"

namespace eld {

class DiagnosticEngine;
class ELFSection;
class CIEFragment;
class EhFrameHdrFragment;

class EhFrameHdrSection : public ELFSection {
public:
  EhFrameHdrSection(std::string Name, uint32_t pType, uint32_t pFlag,
                    uint32_t entSize, uint64_t pSize);

  static bool classof(const Section *S) {
    return S->getSectionKind() == Section::Kind::EhFrameHdr;
  }

  void addCIE(std::vector<CIEFragment *> &C);

  size_t getNumCIE() const { return NumCIE; }

  size_t getNumFDE() const { return NumFDE; }

  size_t sizeOfHeader() const { return 12; }

  std::vector<CIEFragment *> &getCIEs() { return m_CIEs; }

private:
  std::vector<CIEFragment *> m_CIEs;
  size_t NumCIE = 0;
  size_t NumFDE = 0;
};
} // namespace eld

#endif
