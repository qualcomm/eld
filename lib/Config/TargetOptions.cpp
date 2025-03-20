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
TargetOptions::TargetOptions() : Endian(Unknown), BitClass(0) {}

TargetOptions::TargetOptions(const std::string &PTriple)
    : Endian(Unknown), BitClass(0) {
  setTriple(PTriple);
}

TargetOptions::~TargetOptions() {}

void TargetOptions::setTriple(const llvm::Triple &PTriple) { Triple = PTriple; }

void TargetOptions::setTriple(const std::string &PTriple) {
  llvm::Triple triple(PTriple);
  Triple = triple;
}

void TargetOptions::setArch(const std::string &PArchName) {
  ArchName = PArchName;
}

void TargetOptions::setTargetCPU(const std::string &PCpu) { TargetCPU = PCpu; }

void TargetOptions::addEntrySection(LinkerScript &PScript,
                                    llvm::StringRef PPattern) {
  WildcardPattern *Pat = make<WildcardPattern>(PPattern);
  EntrySections.emplace_back(Pat);
  PScript.registerWildCardPattern(Pat);
}

TargetOptions::WildCardVecTy &TargetOptions::getEntrySections() {
  return EntrySections;
}
