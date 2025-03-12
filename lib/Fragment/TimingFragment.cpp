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

TimingFragment::TimingFragment(uint64_t beginningOfTime, uint64_t duration,
                               llvm::StringRef moduleName, ELFSection *O)
    : Fragment(Fragment::Type::Timing, O),
      m_Slice(make<TimingSlice>(beginningOfTime, duration, moduleName)) {}

TimingFragment::TimingFragment(llvm::StringRef Slice,
                               llvm::StringRef InputFileName, ELFSection *O,
                               DiagnosticEngine *DiagEngine)
    : Fragment(Fragment::Type::Timing, O),
      m_Slice(make<TimingSlice>(Slice, InputFileName, DiagEngine)) {}

TimingFragment::~TimingFragment() {}

size_t TimingFragment::size() const {
  if (isNull())
    return 0;
  return 8 + 8 + m_Slice->getModuleName().size() + 1;
}

void TimingFragment::setData(uint64_t beginningOfTime, uint64_t duration) {
  m_Slice->setData(beginningOfTime, duration);
}

void TimingFragment::dump(llvm::raw_ostream &OS) {
  uint32_t fragmentSize = size();
  uint8_t *buf = new uint8_t[fragmentSize];
  llvm::MutableArrayRef<uint8_t> Data =
      llvm::MutableArrayRef(buf, fragmentSize);
  llvm::BinaryStreamWriter Writer(Data, llvm::endianness::little);

  llvm::Error E = Writer.writeInteger<uint64_t>(m_Slice->getBeginningOfTime());
  if (!E)
    llvm::consumeError(std::move(E));
  E = Writer.writeInteger<uint64_t>(m_Slice->getDuration());
  if (!E)
    llvm::consumeError(std::move(E));
  E = Writer.writeCString(m_Slice->getModuleName());
  if (!E)
    llvm::consumeError(std::move(E));

  std::string linkStats(reinterpret_cast<const char *>(Data.data()),
                        fragmentSize);
  OS << linkStats;
  delete[] buf;
}

eld::Expected<void> TimingFragment::emit(MemoryRegion &mr, Module &M) {
  std::string S;
  llvm::raw_string_ostream OS(S);
  dump(OS);
  uint8_t *out = mr.begin() + getOffset(M.getConfig().getDiagEngine());
  memcpy(out, OS.str().c_str(), size());
  return {};
}
