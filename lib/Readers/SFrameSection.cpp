//===- SFrameSection.cpp---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Readers/SFrameSection.h"
#include "eld/Diagnostics/MsgHandler.h"
#include "eld/Fragment/RegionFragment.h"
#include "eld/Fragment/SFrameFragment.h"
#include "llvm/Support/Endian.h"

using namespace eld;

SFrameSection::SFrameSection(std::string Name, DiagnosticEngine *E,
                             uint32_t Type, uint32_t Flags, uint32_t EntSize,
                             uint64_t Size)
    : ELFSection(Section::Kind::SFrame, LDFileFormat::SFrame, Name, Flags,
                 EntSize, /*AddrAlign=*/0, Type, /*Info=*/0, /*Link=*/nullptr,
                 Size, /*PAddr=*/0),
      TheDiagEngine(E) {}

bool SFrameSection::parseSFrameSection() {
  if (!size())
    return true;

  // Get data from the first fragment (RegionFragment).
  if (getFragmentList().empty())
    return true;

  auto *RF = llvm::dyn_cast<eld::RegionFragment>(getFragmentList().front());
  if (!RF)
    return false;

  Data = llvm::ArrayRef(reinterpret_cast<const uint8_t *>(RF->getRegion().data()),
                        RF->size());

  // Validate minimum header size.
  if (Data.size() < SFrameHeaderSize) {
    TheDiagEngine->raise(Diag::sframe_read_error)
        << "section too small for SFrame header"
        << getInputFile()->getInput()->decoratedPath();
    return false;
  }

  // Validate magic number (bytes 0-1, little-endian).
  uint16_t Magic =
      llvm::support::endian::read16le(Data.data());
  if (Magic != SFrameMagic) {
    TheDiagEngine->raise(Diag::sframe_read_error)
        << "invalid SFrame magic number"
        << getInputFile()->getInput()->decoratedPath();
    return false;
  }

  // Validate version (byte 2).
  uint8_t Version = Data[2];
  if (Version != SFrameVersionCurrent) {
    TheDiagEngine->raise(Diag::sframe_read_error)
        << "unsupported SFrame version"
        << getInputFile()->getInput()->decoratedPath();
    return false;
  }

  // Read ABI/arch (byte 4).
  ABI = Data[4];

  // Read number of FDEs (bytes 8-11, little-endian).
  NumFDEs = llvm::support::endian::read32le(Data.data() + 8);

  // Read number of FREs (bytes 12-15, little-endian).
  NumFREs = llvm::support::endian::read32le(Data.data() + 12);

  return true;
}
