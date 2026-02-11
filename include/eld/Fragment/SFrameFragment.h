//===- SFrameFragment.h----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_FRAGMENT_SFRAMEFRAGMENT_H
#define ELD_FRAGMENT_SFRAMEFRAGMENT_H

#include "eld/Fragment/Fragment.h"
#include "llvm/ADT/ArrayRef.h"
#include <cstdint>

namespace eld {

class ELFSection;
class Module;

/// SFrame header constants (SFrame v2)
constexpr uint16_t SFrameMagic = 0xdee2;
constexpr uint8_t SFrameVersionCurrent = 2;

/// SFrame header size (SFrame v2)
constexpr size_t SFrameHeaderSize = 32;

/// SFrame ABI/arch identifiers as defined in the SFrame specification.
/// See https://sourceware.org/binutils/docs/sframe-spec.html
enum SFrameABI : uint8_t {
  SFRAME_ABI_NONE = 0,
  SFRAME_ABI_AARCH64_ENDIAN_BIG = 1,
  SFRAME_ABI_AARCH64_ENDIAN_LITTLE = 2,
  SFRAME_ABI_AMD64_ENDIAN_LITTLE = 3,
};

/// SFrameFragment holds the raw data from one input .sframe section.
/// When emitted, it writes the raw sframe data to the output.
class SFrameFragment : public Fragment {
public:
  SFrameFragment(llvm::ArrayRef<uint8_t> Data, ELFSection *Owner);

  virtual ~SFrameFragment();

  virtual size_t size() const override { return SectionData.size(); }

  static bool classof(const Fragment *F) {
    return F->getKind() == Fragment::Type::SFrame;
  }

  virtual eld::Expected<void> emit(MemoryRegion &Mr, Module &M) override;

  virtual void dump(llvm::raw_ostream &OS) override;

  llvm::ArrayRef<uint8_t> getData() const { return SectionData; }

private:
  llvm::ArrayRef<uint8_t> SectionData;
};

} // namespace eld

#endif
