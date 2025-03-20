//===- BranchIsland.cpp----------------------------------------------------===//
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
#include "eld/BranchIsland/BranchIsland.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Fragment/Stub.h"
#include "eld/Readers/ELFSection.h"
#include "eld/SymbolResolver/ResolveInfo.h"

using namespace eld;

//==========================
// BranchIsland

BranchIsland::BranchIsland(Stub *Stub) : S(Stub), Reloc(nullptr) {}

BranchIsland::~BranchIsland() {}

int64_t BranchIsland::branchIslandAddr(Module &M) {
  ResolveInfo *BranchIslandInfo = S->symInfo();
  if (BranchIslandInfo->outSymbol()->hasFragRef()) {
    const FragmentRef *FragRef = BranchIslandInfo->outSymbol()->fragRef();
    return FragRef->getOutputELFSection()->addr() + FragRef->getOutputOffset(M);
  }
  return BranchIslandInfo->outSymbol()->value();
}

ResolveInfo *BranchIsland::symInfo() const { return S->symInfo(); }
