//===- TimingFragment.h----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#ifndef ELD_FRAGMENT_TIMINGFRAGMENT_H
#define ELD_FRAGMENT_TIMINGFRAGMENT_H

#include "eld/Fragment/Fragment.h"
#include "eld/Readers/TimingSlice.h"
#include "llvm/Support/raw_ostream.h"
namespace eld {
class LinkerConfig;

class TimingFragment : public Fragment {
public:
  TimingFragment(uint64_t BeginningOfTime, uint64_t Duration,
                 llvm::StringRef ModuleName, ELFSection *O);

  TimingFragment(llvm::StringRef Slice, llvm::StringRef InputFileName,
                 ELFSection *O, DiagnosticEngine *DiagEngine);

  ~TimingFragment();
  static bool classof(const Fragment *F) {
    return F->getKind() == Fragment::Timing;
  }

  size_t size() const override;

  eld::Expected<void> emit(MemoryRegion &Mr, Module &M) override;

  void dump(llvm::raw_ostream &OS) override;

  // set the actual timing info after linking is done
  void setData(uint64_t BeginningOfTime, uint64_t Duration);

  const TimingSlice *getTimingSlice() const { return TimeSlice; }

private:
  TimingSlice *TimeSlice;
};

} // namespace eld

#endif
