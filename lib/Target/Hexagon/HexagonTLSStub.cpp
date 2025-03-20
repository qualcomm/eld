//===- HexagonTLSStub.cpp--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "HexagonTLSStub.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/LayoutMap/LayoutPrinter.h"
#include "eld/Support/Memory.h"
#include "eld/Support/MsgHandling.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include <string>

using namespace eld;

HexagonTLSStub::HexagonTLSStub(StubType T, ELFSection *O, ResolveInfo *R,
                               uint32_t Align, uint32_t size)
    : TargetFragment(TargetFragment::TargetSpecific, O, R, Align, size),
      m_StubType(T) {
  if (O)
    O->addFragmentAndUpdateSize(this);
}

HexagonTLSStub *HexagonGDStub::Create(Module &pModule, ELFSection *O) {
  DiagnosticEngine *DiagEngine = pModule.getConfig().getDiagEngine();
  std::string name = HexagonTLSStub::stubName(HexagonTLSStub::GD).str();
  LDSymbol *symbol =
      pModule.getIRBuilder()->addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
          O->getInputFile(), name, ResolveInfo::Function,
          ResolveInfo::Undefined, ResolveInfo::Global,
          0, // size
          0, // value
          FragmentRef::null(), ResolveInfo::Default, true /* isPostLTOPhase */);
  if (pModule.getConfig().options().isSymbolTracingRequested() &&
      pModule.getConfig().options().traceSymbol(name))
    DiagEngine->raise(Diag::target_specific_symbol) << name;
  // GC is already complete, mark this stub as it is needed
  symbol->setShouldIgnore(false);
  HexagonTLSStub *T = make<HexagonGDStub>(O, symbol->resolveInfo());
  return T;
}

HexagonTLSStub *HexagonGDIEStub::Create(Module &pModule, ELFSection *O) {
  DiagnosticEngine *DiagEngine = pModule.getConfig().getDiagEngine();
  HexagonTLSStub *T = make<HexagonGDIEStub>(O, nullptr);
  LDSymbol *symbol =
      pModule.getIRBuilder()->addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
          O->getInputFile(), T->name(), ResolveInfo::Function,
          ResolveInfo::Define, ResolveInfo::Global,
          T->size(), // size
          0,         // value
          make<FragmentRef>(*T, 0), ResolveInfo::Default,
          true /* isPostLTOPhase */);
  if (pModule.getConfig().options().isSymbolTracingRequested() &&
      pModule.getConfig().options().traceSymbol(T->name()))
    DiagEngine->raise(Diag::target_specific_symbol) << T->name();
  // GC is already complete, mark this stub as it is needed
  symbol->setShouldIgnore(false);
  symbol->resolveInfo()->setResolvedOrigin(O->getInputFile());
  T->setSymInfo(symbol->resolveInfo());
  LayoutPrinter *printer = pModule.getLayoutPrinter();
  if (printer) {
    printer->recordFragment(O->getInputFile(), O, T);
    printer->recordSymbol(T, symbol);
  }
  return T;
}

HexagonTLSStub *HexagonLDLEStub::Create(Module &pModule, ELFSection *O) {
  DiagnosticEngine *DiagEngine = pModule.getConfig().getDiagEngine();
  HexagonTLSStub *T = make<HexagonLDLEStub>(O, nullptr);
  LDSymbol *symbol =
      pModule.getIRBuilder()->addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
          O->getInputFile(), T->name(), ResolveInfo::Function,
          ResolveInfo::Define, ResolveInfo::Global,
          T->size(), // size
          0,         // value
          make<FragmentRef>(*T, 0), ResolveInfo::Default,
          true /* isPostLTOPhase */);
  symbol->resolveInfo()->setResolvedOrigin(O->getInputFile());
  if (pModule.getConfig().options().isSymbolTracingRequested() &&
      pModule.getConfig().options().traceSymbol(T->name()))
    DiagEngine->raise(Diag::target_specific_symbol) << T->name();
  // GC is already complete, mark this stub as it is needed
  symbol->setShouldIgnore(false);
  T->setSymInfo(symbol->resolveInfo());
  LayoutPrinter *printer = pModule.getLayoutPrinter();
  if (printer) {
    printer->recordFragment(O->getInputFile(), O, T);
    printer->recordSymbol(T, symbol);
  }
  return T;
}
