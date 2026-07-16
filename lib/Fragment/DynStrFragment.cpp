//===- DynStrFragment.cpp--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Fragment/DynStrFragment.h"
#include "eld/Core/Module.h"

using namespace eld;

DynStrFragment::DynStrFragment(ELFSection *S)
    : Fragment(Fragment::Type::DynStr, S), Contents(1, '\0') {}

std::size_t DynStrFragment::addString(const std::string &S) {
  auto It = Offsets.find(S);
  if (It != Offsets.end())
    return It->second;
  std::size_t Offset = Contents.size();
  Contents += S;
  Contents += '\0';
  return Offsets[S] = Offset;
}

std::optional<std::size_t>
DynStrFragment::getStringOffset(const std::string &S) const {
  auto It = Offsets.find(S);
  if (It != Offsets.end())
    return It->second;
  return std::nullopt;
}

size_t DynStrFragment::size() const { return Contents.size(); }

eld::Expected<void> DynStrFragment::emit(MemoryRegion &Mr, Module &M) {
  memcpy(Mr.begin() + getOffset(M.getConfig().getDiagEngine()), Contents.data(),
         Contents.size());
  return {};
}
