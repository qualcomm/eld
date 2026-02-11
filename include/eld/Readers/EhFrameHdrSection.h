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
class EhFrameFragment;
class EhFrameHdrFragment;

class EhFrameHdrSection : public ELFSection {
public:
  EhFrameHdrSection(std::string Name, uint32_t pType, uint32_t pFlag,
                    uint32_t entSize, uint64_t pSize);

  static bool classof(const Section *S) {
    return S->getSectionKind() == Section::Kind::EhFrameHdr;
  }

  void addEhFrame(EhFrameFragment &F);

  size_t getNumCIE() const { return NumCIE; }

  size_t getNumFDE() const { return NumFDE; }

  size_t sizeOfHeader() const { return 12; }

  const std::vector<EhFrameFragment *> &getEhFrames() const { return EhFrames; }

private:
  std::vector<EhFrameFragment *> EhFrames;
  size_t NumCIE = 0;
  size_t NumFDE = 0;
};
} // namespace eld

#endif
