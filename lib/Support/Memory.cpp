//===- Memory.cpp----------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Support/Memory.h"
#include "llvm/ADT/STLExtras.h"

using namespace llvm;
using namespace eld;

BumpPtrAllocator eld::BAlloc;
StringSaver eld::Saver{BAlloc};
std::vector<SpecificAllocBase *> eld::SpecificAllocBase::Instances;

void eld::freeArena() {
  for (SpecificAllocBase *Alloc : llvm::reverse(SpecificAllocBase::Instances))
    Alloc->reset();
  BAlloc.Reset();
}

const char *eld::getUninitBuffer(uint32_t Sz) {
  return BAlloc.Allocate<char>(Sz);
}

// Format bytes using 1024-based units with a short suffix (B, KB, MB, GB, TB)
std::string eld::formatBytes(size_t bytes) {
  static constexpr const char *kUnits[] = {"B", "KB", "MB", "GB", "TB"};
  double value = static_cast<double>(bytes);
  size_t unit = 0;
  while (value >= 1024.0 && unit < std::size(kUnits) - 1) {
    value /= 1024.0;
    ++unit;
  }
  char buf[64];
  // Use 0 decimals for large values, 1 decimal otherwise
  if (value >= 100.0 || unit == 0) {
    std::snprintf(buf, sizeof(buf), "%.0f %s", value, kUnits[unit]);
  } else {
    std::snprintf(buf, sizeof(buf), "%.1f %s", value, kUnits[unit]);
  }
  return std::string(buf);
}

void eld::dumpArenaReport(llvm::raw_ostream &OS) {
  // Copy and sort instances by total bytes (descending)
  std::vector<SpecificAllocBase *> items = SpecificAllocBase::Instances;
  std::sort(items.begin(), items.end(),
            [](const SpecificAllocBase *a, const SpecificAllocBase *b) {
              return a->totalBytes() > b->totalBytes();
            });

  size_t grandTotal = 0;
  OS << "[eld] Arena allocation report by type (sorted by total bytes):\n";
  for (auto *I : items) {
    const size_t total = I->totalBytes();
    OS << "  - " << I->typeName() << ": count=" << I->count()
       << ", sizeof(T)=" << I->sizeOfT() << "B"
       << ", total=" << total << "B"
       << " (" << formatBytes(total) << ")\n";
    grandTotal += total;
  }
  OS << "Grand total: " << grandTotal << "B"
     << " (" << formatBytes(grandTotal) << ")\n";
}
