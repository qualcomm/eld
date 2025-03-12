//===- CommonELFSection.h--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#ifndef ELD_READERS_COMMONSECTIONH
#define ELD_READERS_COMMONSECTIONH

#include "eld/Input/InputFile.h"
#include "eld/Readers/ELFSection.h"
#include "llvm/BinaryFormat/ELF.h"

namespace eld {
/// Common section contains a common symbol.
///
class CommonELFSection : public ELFSection {
public:
  CommonELFSection(const std::string &Name, InputFile *I, uint32_t Align)
      : ELFSection(Section::Kind::CommonELF, LDFileFormat::Common, Name,
                   DefaultFlags, /*EntSize=*/0, Align, DefaultType, /*Info=*/0,
                   /*Link=*/nullptr,
                   /*SectionSize*/ 0, /*PAddr=*/0),
        Origin(I) {}

  static bool classof(const Section *S) {
    return S->getSectionKind() == Section::Kind::CommonELF;
  }

  InputFile *getOrigin() const { return Origin; }

  static constexpr uint32_t DefaultType = llvm::ELF::SHT_NOBITS;
  static constexpr uint32_t DefaultFlags =
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE;

private:
  eld::InputFile *Origin;
};
} // namespace eld
#endif
