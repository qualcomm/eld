//===- TargetOptions.cpp---------------------------------------------------===//
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
#include "eld/Config/TargetOptions.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Support/Memory.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// TargetOptions
//===----------------------------------------------------------------------===//
TargetOptions::TargetOptions() : m_Endian(Unknown), m_BitClass(0) {}

TargetOptions::TargetOptions(const std::string &pTriple)
    : m_Endian(Unknown), m_BitClass(0) {
  setTriple(pTriple);
}

TargetOptions::~TargetOptions() {}

void TargetOptions::setTriple(const llvm::Triple &pTriple) {
  m_Triple = pTriple;
}

void TargetOptions::setTriple(const std::string &pTriple) {
  llvm::Triple Triple(pTriple);
  m_Triple = Triple;
}

void TargetOptions::setArch(const std::string &pArchName) {
  m_ArchName = pArchName;
}

void TargetOptions::setTargetCPU(const std::string &pCPU) {
  m_TargetCPU = pCPU;
}

void TargetOptions::addEntrySection(LinkerScript &pScript,
                                    llvm::StringRef pPattern) {
  WildcardPattern *Pat = make<WildcardPattern>(pPattern);
  m_EntrySections.emplace_back(Pat);
  pScript.registerWildCardPattern(Pat);
}

TargetOptions::WildCardVecTy &TargetOptions::getEntrySections() {
  return m_EntrySections;
}
