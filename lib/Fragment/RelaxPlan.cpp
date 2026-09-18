//===-RelaxPlan.cpp--------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Fragment/RelaxPlan.h"
#include <algorithm>
#include <cassert>

using namespace eld;

void RelaxPlan::addEdit(RelaxEdit E) {
  auto It = std::upper_bound(Edits.begin(), Edits.end(), E.DeleteOffset,
                             [](uint32_t Off, const RelaxEdit &Existing) {
                               return Off < Existing.DeleteOffset;
                             });
#ifndef NDEBUG
  for (auto Prev = It; Prev != Edits.begin();) {
    --Prev;
    if (Prev->Active) {
      assert(Prev->DeleteOffset + Prev->DeleteBytes <= E.DeleteOffset &&
             "RelaxPlan edits must not delete overlapping ranges");
      break;
    }
  }
  for (auto Next = It; Next != Edits.end(); ++Next) {
    if (Next->Active) {
      assert(E.DeleteOffset + E.DeleteBytes <= Next->DeleteOffset &&
             "RelaxPlan edits must not delete overlapping ranges");
      break;
    }
  }
#endif
  Edits.insert(It, std::move(E));
  rebuildDerived();
}

void RelaxPlan::removeEdit(const Relocation *R) {
  auto It = ByReloc.find(R);
  if (It == ByReloc.end())
    return;
  Edits[It->second].Active = false;
  rebuildDerived();
}

void RelaxPlan::clear() {
  Edits.clear();
  PrefixShift.clear();
  ByReloc.clear();
  TotalShift = 0;
}

void RelaxPlan::rebuildDerived() {
  PrefixShift.clear();
  PrefixShift.reserve(Edits.size());
  ByReloc.clear();
  uint32_t Shift = 0;
  for (unsigned I = 0; I < Edits.size(); ++I) {
    PrefixShift.push_back(Shift);
    if (Edits[I].Active) {
      Shift += Edits[I].DeleteBytes;
      if (Edits[I].Reloc)
        ByReloc[Edits[I].Reloc] = I;
    }
  }
  TotalShift = Shift;
}

uint32_t RelaxPlan::shiftAt(uint32_t OrigOff) const {
  // First edit with DeleteOffset >= OrigOff; everything before it has
  // already fired.
  auto It = std::upper_bound(
      Edits.begin(), Edits.end(), OrigOff,
      [](uint32_t Off, const RelaxEdit &E) { return Off <= E.DeleteOffset; });
  if (It == Edits.begin())
    return 0;
  size_t Idx = std::distance(Edits.begin(), It);
  while (Idx != 0) {
    --Idx;
    if (Edits[Idx].Active)
      return PrefixShift[Idx] + Edits[Idx].DeleteBytes;
  }
  return 0;
}

bool RelaxPlan::wouldOverlap(uint32_t DeleteOffset,
                             uint32_t DeleteBytes) const {
  auto It = std::upper_bound(Edits.begin(), Edits.end(), DeleteOffset,
                             [](uint32_t Off, const RelaxEdit &Existing) {
                               return Off < Existing.DeleteOffset;
                             });
  for (auto Prev = It; Prev != Edits.begin();) {
    --Prev;
    if (Prev->Active) {
      if (Prev->DeleteOffset + Prev->DeleteBytes > DeleteOffset)
        return true;
      break;
    }
  }
  for (auto Next = It; Next != Edits.end(); ++Next) {
    if (Next->Active)
      return DeleteOffset + DeleteBytes > Next->DeleteOffset;
  }
  return false;
}

RelaxEdit *RelaxPlan::find(const Relocation *R) {
  auto It = ByReloc.find(R);
  return It == ByReloc.end() ? nullptr : &Edits[It->second];
}
