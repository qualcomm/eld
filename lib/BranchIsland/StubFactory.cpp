//===- StubFactory.cpp-----------------------------------------------------===//
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
#include "eld/BranchIsland/StubFactory.h"
#include "eld/BranchIsland/BranchIsland.h"
#include "eld/BranchIsland/BranchIslandFactory.h"
#include "eld/Diagnostics/DiagnosticPrinter.h"
#include "eld/Fragment/FragmentRef.h"
#include "eld/Fragment/Stub.h"
#include "eld/Readers/Relocation.h"
#include "eld/Support/MsgHandling.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "eld/Target/Relocator.h"
#include <string>

using namespace eld;
//===----------------------------------------------------------------------===//
// StubFactory
//===----------------------------------------------------------------------===//
StubFactory::StubFactory(Stub *targetStub) { m_Stubs.emplace_back(targetStub); }

StubFactory::StubFactory() {}

StubFactory::~StubFactory() {}

void StubFactory::registerStub(Stub *pStub) { m_Stubs.emplace_back(pStub); }

/// create - create a stub if needed, otherwise return nullptr
std::pair<BranchIsland *, bool>
StubFactory::create(Relocation &pReloc, eld::IRBuilder &pBuilder,
                    BranchIslandFactory &pBRIslandFactory,
                    GNULDBackend &pBackend) {
  DiagnosticEngine *DiagEngine = pBackend.config().getDiagEngine();
  // If the target has not registered a stub to check relocations
  // against, we cannot create a stub.
  if (m_Stubs.empty()) {
    assert(0 && "target is calling relaxation without a stub registered");
    return std::make_pair(nullptr, false);
  }

  int64_t targetSymbolValue = 0;

  LDSymbol *symbol = pReloc.symInfo()->outSymbol();
  if (symbol->hasFragRef()) {
    uint64_t value = symbol->fragRef()->getOutputOffset(pBuilder.getModule());
    uint64_t addr = symbol->fragRef()->getOutputELFSection()->addr();
    targetSymbolValue = addr + value;
  } else
    targetSymbolValue = symbol->value();

  Stub *Stub = pBackend.getBranchIslandStub(&pReloc, targetSymbolValue);

  if (!Stub)
    return std::make_pair(nullptr, false);

  // We need to explicitly check the range for PLT slot if the stub supports
  // PIC code and we have a slot for it in PLT
  if (Stub->supportsPIC() && pReloc.shouldUsePLTAddr())
    targetSymbolValue = pBackend.getPLTAddr(pReloc.symInfo());

  int64_t Offset = 0;
  if (!Stub->isNeeded(&pReloc, targetSymbolValue, pBuilder.getModule()) &&
      Stub->isRelocInRange(&pReloc, targetSymbolValue, Offset,
                           pBuilder.getModule()))
    return std::make_pair(nullptr, false);

  std::pair<BranchIsland *, bool> branchIsland =
      pBRIslandFactory.createBranchIsland(pReloc, Stub, pBuilder,
                                          pBackend.getRelocator());

  const LinkerConfig &Config = pBuilder.getModule().getConfig();
  if (branchIsland.first) {
    if (pBuilder.getModule().getPrinter()->traceTrampolines()) {
      std::lock_guard<std::mutex> Guard(Mutex);
      DiagEngine->raise(diag::trampoline_symbol) << pReloc.getSymbolName(
          pReloc.symInfo(), Config.options().shouldDemangle());
      llvm::outs() << " ";
      if (branchIsland.second) {
        DiagEngine->raise(diag::reuse_stub) << pReloc.getSymbolName(
            branchIsland.first->symInfo(), Config.options().shouldDemangle());
      } else {
        DiagEngine->raise(diag::create_stub) << pReloc.getSymbolName(
            branchIsland.first->symInfo(), Config.options().shouldDemangle());
      }
      DiagEngine->raise(diag::set_call_from_stub)
          << pReloc.getFragmentPath(nullptr, pReloc.targetRef()->frag(),
                                    Config.options())
          << pReloc.getSymbolName(branchIsland.first->symInfo(),
                                  Config.options().shouldDemangle());
      DiagEngine->raise(diag::destination_stub) << llvm::utohexstr(
          pReloc.place(pBuilder.getModule()), /*LowerCase*/ true);
    }
    pReloc.setSymInfo(branchIsland.first->symInfo());
    return branchIsland;
  }

  return std::make_pair(nullptr, false);
}
