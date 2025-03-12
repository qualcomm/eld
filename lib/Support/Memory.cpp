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
