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
    : Fragment(Fragment::Plt, O, Align), ThisGot(G), ThisSymInfo(R),
      ThisSize(Size), ThisPltType(T) {}

PLT::~PLT() {}

void PLT::setSymInfo(ResolveInfo *PSymInfo) { ThisSymInfo = PSymInfo; }

size_t PLT::size() const { return ThisSize; }

eld::Expected<void> PLT::emit(MemoryRegion &Mr, Module &M) {
  memcpy(Mr.begin() + getOffset(M.getConfig().getDiagEngine()),
         getContent().data(), size());
  return {};
}

const std::string PLT::name() const {
  if (!ThisSymInfo)
    return "";
  return (llvm::Twine("PLT entry for ") + ThisSymInfo->name()).str();
}
