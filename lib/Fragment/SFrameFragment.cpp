//===- SFrameFragment.cpp--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Fragment/SFrameFragment.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Support/MemoryRegion.h"
#include <cstring>

using namespace eld;

SFrameFragment::SFrameFragment(llvm::ArrayRef<uint8_t> Data,
                               ELFSection *Owner)
    : Fragment(Fragment::SFrame, Owner, /*Align=*/4), SectionData(Data) {}

SFrameFragment::~SFrameFragment() {}

eld::Expected<void> SFrameFragment::emit(MemoryRegion &Mr, Module &M) {
  uint32_t OutOffset = getOffset();
  if (SectionData.empty())
    return {};
  uint8_t *Out = Mr.begin() + OutOffset;
  std::memcpy(Out, SectionData.data(), SectionData.size());
  return {};
}

void SFrameFragment::dump(llvm::raw_ostream &OS) {
  OS << "SFrameFragment: size=" << size();
}
