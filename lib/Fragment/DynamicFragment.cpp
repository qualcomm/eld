//===- DynamicFragment.cpp-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Fragment/DynamicFragment.h"
#include "eld/Core/Module.h"
#include "eld/Target/ELFDynamic.h"

using namespace eld;

DynamicFragment::DynamicFragment(ELFSection *S, ELFDynamic &Dynamic)
    : Fragment(Fragment::Type::Dynamic, S), m_Dynamic(Dynamic) {}

size_t DynamicFragment::size() const { return m_Dynamic.numOfBytes(); }

eld::Expected<void> DynamicFragment::emit(MemoryRegion &Mr, Module &M) {
  uint8_t *Base = Mr.begin() + getOffset(M.getConfig().getDiagEngine());
  MemoryRegion SubRegion(Base, m_Dynamic.numOfBytes());
  m_Dynamic.emit(*getOwningSection(), SubRegion);
  return {};
}
