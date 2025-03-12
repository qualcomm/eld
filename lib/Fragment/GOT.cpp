//===- GOT.cpp-------------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#include "eld/Fragment/GOT.h"
#include "eld/Core/Module.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "llvm/ADT/Twine.h"

using namespace eld;

GOT::GOT(GOTType T, ELFSection *O, ResolveInfo *R, uint32_t Align,
         uint32_t Size)
    : Fragment(Fragment::Got, O, Align), m_pSymInfo(R), m_Size(Size),
      m_ValueType(Default), m_GOTType(T) {}

GOT::~GOT() {}

void GOT::setSymInfo(ResolveInfo *pSymInfo) { m_pSymInfo = pSymInfo; }

size_t GOT::size() const { return m_Size; }

eld::Expected<void> GOT::emit(MemoryRegion &mr, Module &M) {
  std::memcpy(mr.begin() + getOffset(M.getConfig().getDiagEngine()),
              getContent().data(), size());
  return {};
}

const std::string GOT::name() const {
  if (!m_pSymInfo)
    return "";
  return (llvm::Twine("GOT entry for ") + m_pSymInfo->name()).str();
}
