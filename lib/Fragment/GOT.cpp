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
    : Fragment(Fragment::Got, O, Align), ThisSymInfo(R), ThisSize(Size),
      ThisValueType(Default), GotType(T) {}

GOT::~GOT() {}

void GOT::setSymInfo(ResolveInfo *PSymInfo) { ThisSymInfo = PSymInfo; }

size_t GOT::size() const { return ThisSize; }

eld::Expected<void> GOT::emit(MemoryRegion &Mr, Module &M) {
  std::memcpy(Mr.begin() + getOffset(M.getConfig().getDiagEngine()),
              getContent().data(), size());
  return {};
}

const std::string GOT::name() const {
  if (!ThisSymInfo)
    return "";
  return (llvm::Twine("GOT entry for ") + ThisSymInfo->name()).str();
}
