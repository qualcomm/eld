//===- AArch64ErrataFactory.cpp--------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#include "AArch64ErrataFactory.h"
#include "AArch64ErrataIslandFactory.h"
#include "eld/BranchIsland/BranchIsland.h"
#include "eld/Diagnostics/DiagnosticPrinter.h"
#include "eld/Fragment/FragmentRef.h"
#include "eld/Fragment/Stub.h"
#include "eld/Readers/Relocation.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include <string>

using namespace eld;

//===----------------------------------------------------------------------===//
// AArch64ErrataFactory
//===----------------------------------------------------------------------===//
AArch64ErrataFactory::AArch64ErrataFactory(Stub *targetStub)
    : m_Stub(targetStub) {}

AArch64ErrataFactory::~AArch64ErrataFactory() {}

/// create - create a stub if needed, otherwise return nullptr
BranchIsland *
AArch64ErrataFactory::create(Fragment *frag, uint32_t pOffset,
                             eld::IRBuilder &pBuilder,
                             AArch64ErrataIslandFactory &pErrataIslandFactory) {
  // If the target has not registered a stub to check relocations
  // against, we cannot create a stub.
  if (!m_Stub) {
    assert(0 && "target is calling relaxation without a stub registered");
    return nullptr;
  }

  BranchIsland *errataIsland = pErrataIslandFactory.createAArch64ErrataIsland(
      frag, pOffset, m_Stub, pBuilder);

  if (errataIsland) {
    return errataIsland;
  } else {
    assert(0 && "cannot create errata island");
  }

  return nullptr;
}
