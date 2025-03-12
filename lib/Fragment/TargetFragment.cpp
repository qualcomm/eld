//===- TargetFragment.cpp--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

//===- TargetFragment.cpp -------------------------------------------------===//
//===----------------------------------------------------------------------===//

#include "eld/Fragment/TargetFragment.h"
#include "eld/Core/Module.h"
#include "eld/SymbolResolver/ResolveInfo.h"

using namespace eld;

TargetFragment::TargetFragment(Kind K, ELFSection *O, ResolveInfo *R,
                               uint32_t Align, uint32_t Size)
    : Fragment(Fragment::Target, O, Align), m_pSymInfo(R), m_TargetKind(K),
      m_Size(Size) {}

TargetFragment::~TargetFragment() {}

void TargetFragment::setSymInfo(ResolveInfo *pSymInfo) {
  m_pSymInfo = pSymInfo;
}

size_t TargetFragment::size() const { return m_Size; }

eld::Expected<void> TargetFragment::emit(MemoryRegion &mr, Module &M) {
  if (getContent().data())
    memcpy(mr.begin() + getOffset(M.getConfig().getDiagEngine()),
           getContent().data(), size());
  return {};
}

const std::string TargetFragment::name() const {
  if (!m_pSymInfo)
    return "";
  return std::string(m_pSymInfo->name());
}
