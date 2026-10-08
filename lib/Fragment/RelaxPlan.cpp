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
  assert(E.Reloc && "RelaxPlan edits must be keyed by a relocation");
#ifndef NDEBUG
  for (const auto &Entry : EditsByReloc) {
    const RelaxEdit &Existing = Entry.second;
    if (Existing.DeleteOffset + Existing.DeleteBytes <= E.DeleteOffset ||
        E.DeleteOffset + E.DeleteBytes <= Existing.DeleteOffset)
      continue;
    assert(false && "RelaxPlan edits must not delete overlapping ranges");
  }
#endif

  auto Existing = EditsByReloc.find(E.Reloc);
  if (Existing != EditsByReloc.end() && Existing->second.Active)
    TotalShift -= Existing->second.DeleteBytes;
  if (E.Active)
    TotalShift += E.DeleteBytes;
  EditsByReloc[E.Reloc] = std::move(E);
  invalidateDerived();
}

void RelaxPlan::removeEdit(const Relocation *R) {
  auto It = EditsByReloc.find(R);
  if (It == EditsByReloc.end())
    return;
  if (It->second.Active)
    TotalShift -= It->second.DeleteBytes;
  EditsByReloc.erase(It);
  invalidateDerived();
}

void RelaxPlan::clear() {
  EditsByReloc.clear();
  OrderedEdits.clear();
  PrefixShift.clear();
  DerivedValid = true;
  TotalShift = 0;
}

void RelaxPlan::rebuildDerived() {
  OrderedEdits.clear();
  OrderedEdits.reserve(EditsByReloc.size());
  for (const auto &Entry : EditsByReloc)
    if (Entry.second.Active)
      OrderedEdits.push_back(Entry.second);

  std::sort(OrderedEdits.begin(), OrderedEdits.end(),
            [](const RelaxEdit &L, const RelaxEdit &R) {
              return L.DeleteOffset < R.DeleteOffset;
            });

  PrefixShift.clear();
  PrefixShift.reserve(OrderedEdits.size());
  uint32_t Shift = 0;
  for (const RelaxEdit &E : OrderedEdits) {
    PrefixShift.push_back(Shift);
    Shift += E.DeleteBytes;
  }
  TotalShift = Shift;
  DerivedValid = true;
}

uint32_t RelaxPlan::shiftAt(uint32_t OrigOff) const {
  if (!DerivedValid) {
    uint32_t Shift = 0;
    for (const auto &Entry : EditsByReloc) {
      const RelaxEdit &E = Entry.second;
      if (E.Active && E.DeleteOffset < OrigOff)
        Shift += E.DeleteBytes;
    }
    return Shift;
  }

  auto It = std::lower_bound(
      OrderedEdits.begin(), OrderedEdits.end(), OrigOff,
      [](const RelaxEdit &E, uint32_t Off) { return E.DeleteOffset < Off; });
  if (It == OrderedEdits.begin())
    return 0;
  return PrefixShift[std::distance(OrderedEdits.begin(), It) - 1] +
         (It - 1)->DeleteBytes;
}

uint32_t RelaxPlan::totalShift() const { return TotalShift; }

bool RelaxPlan::wouldOverlap(uint32_t DeleteOffset,
                             uint32_t DeleteBytes) const {
  if (!DerivedValid) {
    for (const auto &Entry : EditsByReloc) {
      const RelaxEdit &E = Entry.second;
      if (E.Active && E.DeleteOffset < DeleteOffset + DeleteBytes &&
          DeleteOffset < E.DeleteOffset + E.DeleteBytes)
        return true;
    }
    return false;
  }

  auto It = std::lower_bound(
      OrderedEdits.begin(), OrderedEdits.end(), DeleteOffset,
      [](const RelaxEdit &E, uint32_t Off) { return E.DeleteOffset < Off; });
  if (It != OrderedEdits.begin() &&
      (It - 1)->DeleteOffset + (It - 1)->DeleteBytes > DeleteOffset)
    return true;
  return It != OrderedEdits.end() &&
         DeleteOffset + DeleteBytes > It->DeleteOffset;
}

RelaxEdit *RelaxPlan::find(const Relocation *R) {
  auto It = EditsByReloc.find(R);
  if (It == EditsByReloc.end())
    return nullptr;
  invalidateDerived();
  return &It->second;
}

llvm::ArrayRef<RelaxEdit> RelaxPlan::edits() {
  if (!DerivedValid)
    rebuildDerived();
  return OrderedEdits;
}
