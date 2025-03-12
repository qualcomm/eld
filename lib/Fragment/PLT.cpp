//===- PLT.cpp-------------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#include "eld/Fragment/PLT.h"
#include "eld/Core/Module.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "llvm/ADT/Twine.h"

using namespace eld;

PLT::PLT(PLTType T, GOT *G, ELFSection *O, ResolveInfo *R, uint32_t Align,
         uint32_t Size)
    : Fragment(Fragment::Plt, O, Align), m_GOT(G), m_pSymInfo(R), m_Size(Size),
      m_PLTType(T) {}

PLT::~PLT() {}

void PLT::setSymInfo(ResolveInfo *pSymInfo) { m_pSymInfo = pSymInfo; }

size_t PLT::size() const { return m_Size; }

eld::Expected<void> PLT::emit(MemoryRegion &mr, Module &M) {
  memcpy(mr.begin() + getOffset(M.getConfig().getDiagEngine()),
         getContent().data(), size());
  return {};
}

const std::string PLT::name() const {
  if (!m_pSymInfo)
    return "";
  return (llvm::Twine("PLT entry for ") + m_pSymInfo->name()).str();
}
