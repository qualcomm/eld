//===- TimingFragment.cpp--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#include "eld/Fragment/TimingFragment.h"
#include "eld/Core/Module.h"
#include "eld/Support/Memory.h"
#include "llvm/Support/BinaryStreamWriter.h"

using namespace eld;

TimingFragment::TimingFragment(uint64_t BeginningOfTime, uint64_t Duration,
                               llvm::StringRef ModuleName, ELFSection *O)
    : Fragment(Fragment::Type::Timing, O),
      TimeSlice(make<TimingSlice>(BeginningOfTime, Duration, ModuleName)) {}

TimingFragment::TimingFragment(llvm::StringRef Slice,
                               llvm::StringRef InputFileName, ELFSection *O,
                               DiagnosticEngine *DiagEngine)
    : Fragment(Fragment::Type::Timing, O),
      TimeSlice(make<TimingSlice>(Slice, InputFileName, DiagEngine)) {}

TimingFragment::~TimingFragment() {}

size_t TimingFragment::size() const {
  if (isNull())
    return 0;
  return 8 + 8 + TimeSlice->getModuleName().size() + 1;
}

void TimingFragment::setData(uint64_t BeginningOfTime, uint64_t Duration) {
  TimeSlice->setData(BeginningOfTime, Duration);
}

void TimingFragment::dump(llvm::raw_ostream &OS) {
  uint32_t FragmentSize = size();
  uint8_t *Buf = new uint8_t[FragmentSize];
  llvm::MutableArrayRef<uint8_t> Data =
      llvm::MutableArrayRef(Buf, FragmentSize);
  llvm::BinaryStreamWriter Writer(Data, llvm::endianness::little);

  llvm::Error E =
      Writer.writeInteger<uint64_t>(TimeSlice->getBeginningOfTime());
  if (!E)
    llvm::consumeError(std::move(E));
  E = Writer.writeInteger<uint64_t>(TimeSlice->getDuration());
  if (!E)
    llvm::consumeError(std::move(E));
  E = Writer.writeCString(TimeSlice->getModuleName());
  if (!E)
    llvm::consumeError(std::move(E));

  std::string LinkStats(reinterpret_cast<const char *>(Data.data()),
                        FragmentSize);
  OS << LinkStats;
  delete[] Buf;
}

eld::Expected<void> TimingFragment::emit(MemoryRegion &Mr, Module &M) {
  std::string S;
  llvm::raw_string_ostream OS(S);
  dump(OS);
  uint8_t *Out = Mr.begin() + getOffset(M.getConfig().getDiagEngine());
  memcpy(Out, OS.str().c_str(), size());
  return {};
}
