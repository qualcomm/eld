//===- NamePool.cpp--------------------------------------------------------===//
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

#include "eld/SymbolResolver/NamePool.h"
#include "eld/Diagnostics/DiagnosticPrinter.h"
#include "eld/Fragment/FragmentRef.h"
#include "eld/Input/InputFile.h"
#include "eld/LayoutMap/LayoutInfo.h"
#include "eld/Plugin/PluginManager.h"
#include "eld/Support/Memory.h"
#include "eld/Support/MsgHandling.h"
#include "eld/Support/StringRefUtils.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/SymbolResolver/StaticResolver.h"
#include "eld/SymbolResolver/SymbolInfo.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// NamePool
//===----------------------------------------------------------------------===//
NamePool::NamePool(eld::LinkerConfig &Config, PluginManager &Pm)
    : ThisConfig(Config), SymbolResolver(make<StaticResolver>()),
      IsSymbolTracingRequested(ThisConfig.options().isSymbolTracingRequested()),
      SymbolResInfo(), PM(Pm) {}

NamePool::~NamePool() {}

/// createSymbol - create a symbol
ResolveInfo *NamePool::createSymbol(
    InputFile *Input, std::string SymbolName, bool IsSymbolInDynamicLibrary,
    ResolveInfo::Type Type, ResolveInfo::Desc Desc,
    ResolveInfo::Binding Binding, ResolveInfo::SizeType Size,
    ResolveInfo::Visibility Visibility, bool IsPostLtoPhase) {
  llvm::StringRef SymName = Saver.save(SymbolName);
  // FIXME: Why not pass all the values in the constructor itself?
  // Passing values in the constructor will ensure that the caller
  // does not miss setting any important property.
  ResolveInfo *Info = make<ResolveInfo>(SymName);
  Info->setIsSymbol(true);
  Info->setSource(IsSymbolInDynamicLibrary);
  Info->setType(Type);
  Info->setDesc(Desc);
  Info->setBinding(Binding);
  Info->setVisibility(Visibility);
  Info->setSize(Size);
  if (Input)
    Info->setResolvedOrigin(Input);
  if (Binding != ResolveInfo::Local)
    return Info;
  if (getSymbolTracingRequested() &&
      ThisConfig.options().traceSymbol(SymbolName)) {
    std::string InputPath =
        Input ? Input->getInput()->decoratedPath() : "(Not applicable)";
    ThisConfig.raise(Diag::add_new_symbol)
        << SymbolName << InputPath << Info->infoAsString();
  }
  LocalSymbols.push_back(Info);
  return Info;
}

ResolveInfo *NamePool::insertLocalSymbol(ResolveInfo InputSymbolResolveInfo,
                                         const LDSymbol &Sym) {
  ASSERT(InputSymbolResolveInfo.isLocal(), "Invalid symbol binding!");
  ResolveInfo *RI = make<ResolveInfo>(InputSymbolResolveInfo);
  InputFile *IF = InputSymbolResolveInfo.resolvedOrigin();
  if (getSymbolTracingRequested() &&
      ThisConfig.options().traceSymbol(Sym, InputSymbolResolveInfo)) {
    std::string InputPath =
        IF ? IF->getInput()->decoratedPath() : "(Not applicable)";
    ThisConfig.raise(Diag::add_new_symbol)
        << getDecoratedName(Sym, InputSymbolResolveInfo) << InputPath
        << RI->infoAsString();
  }
  LocalSymbols.push_back(RI);
  return RI;
}

/// Performs symbol resolution for the specified symbol.
///
/// All the symbols with the same name share the same ResolveInfo
/// object. If there are 2 symbols with the same name: inputSym1 and
/// inputSym2, and inputSym2 overrides inputSym1 then ResolveInfo object
/// will be modified to reflect the override. ResolveInfo stores the output
/// symbol properties.
///
/// This function sets/overrides all the output symbol properties stored in
/// the ResolveInfo object, except symbol value.
///
/// @returns true if symbol resolution is successful; Otherwise returns false.
/// Symbol resolution fails if the previous symbol and the new symbol are not
/// compatible.
/// @returns pResult: a tuple of {resolveInfo, existent, overridden}.
bool NamePool::insertSymbol(
    InputFile *Input, std::string SymbolName, bool IsSymbolInDynamicLibrary,
    ResolveInfo::Type Type, ResolveInfo::Desc Desc,
    ResolveInfo::Binding Binding, ResolveInfo::SizeType Size,
    LDSymbol::ValueType Value, ResolveInfo::Visibility Visibility,
    ResolveInfo *pOldInfo, Resolver::Result &PResult, bool IsPostLtoPhase,
    bool IsBitCode, unsigned int PSymIdx, bool IsPatchable,
    DiagnosticPrinter *Printer) {
  bool Exist = false;
  ResolveInfo *NewSymbol = nullptr;
  ResolveInfo *old_symbol = nullptr;
  auto I = GlobalSymbols.find(SymbolName);

  // Setup the New symbol.
  llvm::StringRef SymName = Saver.save(SymbolName);
  NewSymbol = make<ResolveInfo>(SymName);

  if (I == GlobalSymbols.end()) {
    GlobalSymbols[SymbolName] = NewSymbol;
  } else {
    old_symbol = I->getValue();
    Exist = true;
  }

  NewSymbol->setIsSymbol(true);
  NewSymbol->setSource(IsSymbolInDynamicLibrary);
  NewSymbol->setType(Type);
  NewSymbol->setDesc(Desc);
  NewSymbol->setBinding(Binding);
  NewSymbol->setVisibility(Visibility);
  NewSymbol->setSize(Size);
  if (Input)
    NewSymbol->setResolvedOrigin(Input);
  if (IsBitCode)
    NewSymbol->setInBitCode(IsBitCode);
  if (IsPatchable)
    NewSymbol->setPatchable();

  SymbolInfo SymInfo(Input, Size, Binding, Type, Visibility, Desc, IsBitCode);
  // We do not create input symbols for non object file symbols!
  DiagnosticPrinter *DP = ThisConfig.getPrinter();
  auto OldErrorCount = DP->getNumErrors() + DP->getNumFatalErrors();
  PM.callVisitSymbolHook(/*sym=*/nullptr, SymName, SymInfo);
  auto NewErrorCount = DP->getNumErrors() + DP->getNumFatalErrors();
  if (NewErrorCount != OldErrorCount)
    return false;

  bool Traceme = false;

  if (getSymbolTracingRequested() &&
      ThisConfig.options().traceSymbol(SymbolName)) {
    Traceme = true;
    std::string InputPath =
        Input ? Input->getInput()->decoratedPath() : "(Not applicable)";
    ThisConfig.raise(Diag::add_new_symbol)
        << SymbolName << InputPath << NewSymbol->infoAsString();
  }

  if (!Exist) {
    // old_symbol is neither existed nor a symbol.
    PResult.Info = NewSymbol;
    PResult.Existent = false;
    PResult.Overriden = true;
    return true;
  }
  if (nullptr != pOldInfo) {
    // existent, remember its attribute
    pOldInfo->override(*old_symbol);
  }

  if (!canSymbolsBeResolved(old_symbol, NewSymbol))
    return false;

  // exist and is a symbol
  // symbol resolution
  bool Override = false;

  bool OverrideByLto =
      IsPostLtoPhase && old_symbol->isBitCode() && !NewSymbol->isBitCode();
  if (IsPostLtoPhase && !old_symbol->isBitCode() && !NewSymbol->isBitCode() &&
      !old_symbol->isUndef() && !old_symbol->isDyn() &&
      !old_symbol->isCommon() && old_symbol->outSymbol()) {
    if (old_symbol->outSymbol()->hasFragRef() && old_symbol->outSymbol()
                                                     ->fragRef()
                                                     ->frag()
                                                     ->getOwningSection()
                                                     ->isIgnore())
      OverrideByLto = true;
  }
  if (OverrideByLto) {

    old_symbol->override(*NewSymbol, false);
    old_symbol->setSize(NewSymbol->size());
    old_symbol->overrideAttributes(*NewSymbol);
    old_symbol->overrideVisibility(*NewSymbol);
    old_symbol->setBinding(NewSymbol->binding());
    old_symbol->setInBitCode(NewSymbol->isBitCode());

    PResult.Info = old_symbol;
    PResult.Existent = true;
    PResult.Overriden = true;
  } else if (SymbolResolver->resolve(*old_symbol, *NewSymbol, Override, Value,
                                     &ThisConfig, IsPostLtoPhase)) {
    PResult.Info = old_symbol;
    PResult.Existent = true;
    PResult.Overriden = Override;
    if (pOldInfo)
      PResult.Info->setReserved(pOldInfo->reserved());
  } else {
    return false;
  }

  if (Traceme) {
    InputFile *ResolvedOrigin = old_symbol->resolvedOrigin();
    std::string InputPath = ResolvedOrigin
                                ? ResolvedOrigin->getInput()->decoratedPath()
                                : "(Not applicable)";
    ThisConfig.raise(Diag::resolve_new_symbol)
        << SymbolName << InputPath << old_symbol->infoAsString();
  }
  return true;
}

bool NamePool::getSymbolTracingRequested() const {
  return IsSymbolTracingRequested;
}
/// findInfo - find the resolved ResolveInfo
ResolveInfo *NamePool::findInfo(std::string SymbolName) {
  auto I = GlobalSymbols.find(SymbolName);
  if (I == GlobalSymbols.end())
    return nullptr;
  return I->getValue();
}

/// findInfo - find the resolved ResolveInfo
// FIXME: Pass name by const reference, or use llvm::StringRef
const ResolveInfo *NamePool::findInfo(std::string SymbolName) const {
  auto I = GlobalSymbols.find(SymbolName);
  if (I == GlobalSymbols.end())
    return nullptr;
  return I->getValue();
}

/// findSymbol - find the resolved output LDSymbol
// FIXME: Pass name by const reference, or use llvm::StringRef
LDSymbol *NamePool::findSymbol(std::string SymbolName) {
  ResolveInfo *Info = findInfo(SymbolName);
  if (nullptr == Info)
    return nullptr;
  return Info->outSymbol();
}

/// findSymbol - find the resolved output LDSymbol
// FIXME: Pass name by const reference, or use llvm::StringRef
const LDSymbol *NamePool::findSymbol(std::string SymbolName) const {
  const ResolveInfo *Info = findInfo(SymbolName);
  if (nullptr == Info)
    return nullptr;
  return Info->outSymbol();
}

void NamePool::setupNullSymbol() {
  // Setup Null LDSymbol.
  LDSymbol *NullSymbol = LDSymbol::null();
  if (!NullSymbol->resolveInfo()) {
    ResolveInfo::null()->setBinding(ResolveInfo::Local);
    ResolveInfo::null()->setOutSymbol(NullSymbol);
    NullSymbol->setResolveInfo(*ResolveInfo::null());
  }
}

/// createSymbol - create a symbol
LDSymbol *NamePool::createPluginSymbol(InputFile *Input, std::string SymbolName,
                                       Fragment *CurFragment, uint64_t Val,
                                       LayoutInfo *layoutInfo) {
  llvm::StringRef SymName = Saver.save(SymbolName);
  ResolveInfo *Info = make<ResolveInfo>(SymName);
  Info->setIsSymbol(true);
  Info->setSource(false);
  Info->setType(ResolveInfo::NoType);
  Info->setDesc(ResolveInfo::Define);
  Info->setBinding(ResolveInfo::Local);
  Info->setVisibility(ResolveInfo::Default);
  Info->setSize(0);
  if (Input)
    Info->setResolvedOrigin(Input);
  LDSymbol *Sym = make<LDSymbol>(Info, false);
  if (Val > CurFragment->getOwningSection()->size())
    Val = CurFragment->getOwningSection()->size();
  Sym->setFragmentRef(make<FragmentRef>(*CurFragment, Val));
  Info->setOutSymbol(Sym);
  LocalSymbols.push_back(Info);
  if (layoutInfo && layoutInfo->showSymbolResolution())
    getSRI().recordSymbolInfo(
        Sym, SymbolInfo{Input, Info->size(),
                        static_cast<ResolveInfo::Binding>(Info->binding()),
                        static_cast<ResolveInfo::Type>(Info->type()),
                        Info->visibility(),
                        static_cast<ResolveInfo::Desc>(Info->desc()),
                        /*isBitcode=*/false});

  return Sym;
}

std::string NamePool::getDecoratedName(const ResolveInfo *R,
                                       bool DoDeMangle) const {
  return R->getDecoratedName(DoDeMangle);
}

bool NamePool::checkTLSTypes(const ResolveInfo *Old,
                             const ResolveInfo *New) const {
  if (!(Old->isThreadLocal() || New->isThreadLocal()))
    return true;
  if (Old->type() == New->type())
    return true;

  std::string ErrMessage = Old->name();
  if (New->type() == ResolveInfo::ThreadLocal)
    ErrMessage += ": TLS ";
  else
    ErrMessage += ": non-TLS ";

  if (New->desc() != ResolveInfo::Undefined && !New->isCommon())
    ErrMessage += "definition in ";
  else
    ErrMessage += "reference in ";
  ErrMessage +=
      New->resolvedOrigin()->getInput()->decoratedPath(false) + " mismatches ";

  if (Old->type() == ResolveInfo::ThreadLocal)
    ErrMessage += "TLS ";
  else
    ErrMessage += "non-TLS ";

  if (Old->desc() != ResolveInfo::Undefined && !Old->isCommon())
    ErrMessage += "definition in ";
  else
    ErrMessage += "reference in ";
  ErrMessage += Old->resolvedOrigin()->getInput()->decoratedPath(false);
  ThisConfig.raise(Diag::tls_non_tls_symbol_mismatch) << ErrMessage;
  return false;
}

bool NamePool::canSymbolsBeResolved(const ResolveInfo *Old,
                                    const ResolveInfo *New) const {
  return checkTLSTypes(Old, New);
}

ResolveInfo NamePool::createInputSymbolRI(
    const std::string &SymName, InputFile &IF, bool IsDyn,
    ResolveInfo::Type SymType, ResolveInfo::Desc SymDesc,
    ResolveInfo::Binding SymBinding, ResolveInfo::SizeType SymSize,
    ResolveInfo::Visibility SymVisibility, LDSymbol::ValueType SymValue,
    bool IsPatchable) const {
  llvm::StringRef SymNameRef = Saver.save(SymName);
  ResolveInfo RI(SymNameRef);
  RI.setIsSymbol(true);
  RI.setSource(IsDyn);
  RI.setType(SymType);
  RI.setDesc(SymDesc);
  RI.setBinding(SymBinding);
  RI.setVisibility(SymVisibility);
  RI.setSize(SymSize);
  RI.setResolvedOrigin(&IF);
  RI.setValue(SymValue, /*isFinal=*/false);
  RI.setInBitCode(IF.isBitcode());
  if (IsPatchable)
    RI.setPatchable();
  if (IsDyn && RI.canBePreemptible())
    RI.setExportToDyn();
  return RI;
}

bool NamePool::insertNonLocalSymbol(ResolveInfo InputSymbolResolveInfo,
                                    const LDSymbol &Sym, bool IsPostLtoPhase,
                                    Resolver::Result &PResult) {
  bool Exist = false;
  llvm::StringRef SymName = InputSymbolResolveInfo.getName();
  auto I = GlobalSymbols.find(SymName);
  ResolveInfo *OldSymbol = nullptr;
  if (I == GlobalSymbols.end()) {
    GlobalSymbols[SymName] = make<ResolveInfo>(InputSymbolResolveInfo);
  } else {
    OldSymbol = I->getValue();
    Exist = true;
  }

  bool Traceme = false;
  if (getSymbolTracingRequested() &&
      ThisConfig.options().traceSymbol(Sym, InputSymbolResolveInfo)) {
    Traceme = true;
    InputFile *IF = InputSymbolResolveInfo.resolvedOrigin();
    std::string InputPath =
        IF ? IF->getInput()->decoratedPath() : "(Not applicable)";
    ThisConfig.raise(Diag::add_new_symbol)
        << getDecoratedName(Sym, InputSymbolResolveInfo) << InputPath
        << InputSymbolResolveInfo.infoAsString();
  }

  if (!Exist) {
    PResult.Info = GlobalSymbols[SymName];
    PResult.Existent = false;
    PResult.Overriden = true;
    return true;
  }

  if (!canSymbolsBeResolved(OldSymbol, &InputSymbolResolveInfo))
    return false;

  bool Override = false;

  bool OverrideByLto = IsPostLtoPhase && OldSymbol->isBitCode() &&
                       !InputSymbolResolveInfo.isBitCode();
  if (IsPostLtoPhase && !OldSymbol->isBitCode() &&
      !InputSymbolResolveInfo.isBitCode() && !OldSymbol->isUndef() &&
      !OldSymbol->isDyn() && !OldSymbol->isCommon() && OldSymbol->outSymbol()) {
    if (OldSymbol->outSymbol()->hasFragRef() && OldSymbol->outSymbol()
                                                    ->fragRef()
                                                    ->frag()
                                                    ->getOwningSection()
                                                    ->isIgnore())
      OverrideByLto = true;
  }

  if (OverrideByLto) {
    OldSymbol->override(InputSymbolResolveInfo, false);
    OldSymbol->setSize(InputSymbolResolveInfo.size());
    OldSymbol->overrideAttributes(InputSymbolResolveInfo);
    OldSymbol->overrideVisibility(InputSymbolResolveInfo);
    OldSymbol->setBinding(InputSymbolResolveInfo.binding());
    OldSymbol->setInBitCode(InputSymbolResolveInfo.isBitCode());

    PResult.Info = OldSymbol;
    PResult.Existent = true;
    PResult.Overriden = true;
  } else if (SymbolResolver->resolve(*OldSymbol, InputSymbolResolveInfo,
                                     Override, InputSymbolResolveInfo.value(),
                                     &ThisConfig, IsPostLtoPhase)) {
    PResult.Info = OldSymbol;
    PResult.Existent = true;
    PResult.Overriden = Override;
  } else {
    return false;
  }
  if (Traceme) {
    InputFile *ResolvedOrigin = OldSymbol->resolvedOrigin();
    std::string InputPath = ResolvedOrigin
                                ? ResolvedOrigin->getInput()->decoratedPath()
                                : "(Not applicable)";
    ThisConfig.raise(Diag::resolve_new_symbol)
        << getDecoratedName(Sym, InputSymbolResolveInfo) << InputPath
        << OldSymbol->infoAsString();
  }
  return true;
}

std::string NamePool::getDecoratedName(const LDSymbol &Sym,
                                       const ResolveInfo &RI) const {
  ResolveInfo Info(RI);
  LDSymbol S(Sym);
  Info.setOutSymbol(&S);
  return Info.getDecoratedName(ThisConfig.options().shouldDemangle());
}

void NamePool::addUndefinedELFSymbol(InputFile *I, std::string SymbolName,
                                     ResolveInfo::Visibility Vis) {
  // If we have a symbol already no need to add
  if (findInfo(SymbolName))
    return;
  Resolver::Result Result;
  insertSymbol(I, SymbolName, false, ResolveInfo::NoType,
               ResolveInfo::Undefined, ResolveInfo::Global, 0, 0, Vis, nullptr,
               Result, false /* postLTOPhase*/, false, 0,
               false /* isPatchable */, ThisConfig.getPrinter());
  // create a output LDSymbol
  LDSymbol *OutputSym =
      make<LDSymbol>(Result.Info, ThisConfig.options().gcSections());
  Result.Info->setOutSymbol(OutputSym);
}
