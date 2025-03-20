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
StubFactory::StubFactory(Stub *TargetStub) { Stubs.emplace_back(TargetStub); }

StubFactory::StubFactory() {}

StubFactory::~StubFactory() {}

void StubFactory::registerStub(Stub *PStub) { Stubs.emplace_back(PStub); }

/// create - create a stub if needed, otherwise return nullptr
std::pair<BranchIsland *, bool>
StubFactory::create(Relocation &PReloc, eld::IRBuilder &PBuilder,
                    BranchIslandFactory &PBrIslandFactory,
                    GNULDBackend &PBackend) {
  DiagnosticEngine *DiagEngine = PBackend.config().getDiagEngine();
  // If the target has not registered a stub to check relocations
  // against, we cannot create a stub.
  if (Stubs.empty()) {
    assert(0 && "target is calling relaxation without a stub registered");
    return std::make_pair(nullptr, false);
  }

  int64_t TargetSymbolValue = 0;

  LDSymbol *Symbol = PReloc.symInfo()->outSymbol();
  if (Symbol->hasFragRef()) {
    uint64_t Value = Symbol->fragRef()->getOutputOffset(PBuilder.getModule());
    uint64_t Addr = Symbol->fragRef()->getOutputELFSection()->addr();
    TargetSymbolValue = Addr + Value;
  } else
    TargetSymbolValue = Symbol->value();

  Stub *Stub = PBackend.getBranchIslandStub(&PReloc, TargetSymbolValue);

  if (!Stub)
    return std::make_pair(nullptr, false);

  // We need to explicitly check the range for PLT slot if the stub supports
  // PIC code and we have a slot for it in PLT
  if (Stub->supportsPIC() && PReloc.shouldUsePLTAddr())
    TargetSymbolValue = PBackend.getPLTAddr(PReloc.symInfo());

  int64_t Offset = 0;
  if (!Stub->isNeeded(&PReloc, TargetSymbolValue, PBuilder.getModule()) &&
      Stub->isRelocInRange(&PReloc, TargetSymbolValue, Offset,
                           PBuilder.getModule()))
    return std::make_pair(nullptr, false);

  std::pair<BranchIsland *, bool> BranchIsland =
      PBrIslandFactory.createBranchIsland(PReloc, Stub, PBuilder,
                                          PBackend.getRelocator());

  const LinkerConfig &Config = PBuilder.getModule().getConfig();
  if (BranchIsland.first) {
    if (PBuilder.getModule().getPrinter()->traceTrampolines()) {
      std::lock_guard<std::mutex> Guard(Mutex);
      DiagEngine->raise(Diag::trampoline_symbol) << PReloc.getSymbolName(
          PReloc.symInfo(), Config.options().shouldDemangle());
      llvm::outs() << " ";
      if (BranchIsland.second) {
        DiagEngine->raise(Diag::reuse_stub) << PReloc.getSymbolName(
            BranchIsland.first->symInfo(), Config.options().shouldDemangle());
      } else {
        DiagEngine->raise(Diag::create_stub) << PReloc.getSymbolName(
            BranchIsland.first->symInfo(), Config.options().shouldDemangle());
      }
      DiagEngine->raise(Diag::set_call_from_stub)
          << PReloc.getFragmentPath(nullptr, PReloc.targetRef()->frag(),
                                    Config.options())
          << PReloc.getSymbolName(BranchIsland.first->symInfo(),
                                  Config.options().shouldDemangle());
      DiagEngine->raise(Diag::destination_stub) << llvm::utohexstr(
          PReloc.place(PBuilder.getModule()), /*LowerCase*/ true);
    }
    PReloc.setSymInfo(BranchIsland.first->symInfo());
    return BranchIsland;
  }

  return std::make_pair(nullptr, false);
}
