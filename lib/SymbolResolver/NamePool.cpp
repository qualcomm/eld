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
#include "eld/LayoutMap/LayoutPrinter.h"
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
NamePool::NamePool(eld::LinkerConfig &Config, PluginManager &pm)
    : m_Config(Config), m_pResolver(make<StaticResolver>()),
      m_SymbolTracingRequested(m_Config.options().isSymbolTracingRequested()),
      m_SRI(), PM(pm) {}

NamePool::~NamePool() {}

/// createSymbol - create a symbol
ResolveInfo *NamePool::createSymbol(InputFile *pInput, std::string pName,
                                    bool pIsDyn, ResolveInfo::Type pType,
                                    ResolveInfo::Desc pDesc,
                                    ResolveInfo::Binding pBinding,
                                    ResolveInfo::SizeType pSize,
                                    ResolveInfo::Visibility pVisibility,
                                    bool isPostLTOPhase) {
  llvm::StringRef SymName = Saver.save(pName);
  // FIXME: Why not pass all the values in the constructor itself?
  // Passing values in the constructor will ensure that the caller
  // does not miss setting any important property.
  ResolveInfo *info = make<ResolveInfo>(SymName);
  info->setIsSymbol(true);
  info->setSource(pIsDyn);
  info->setType(pType);
  info->setDesc(pDesc);
  info->setBinding(pBinding);
  info->setVisibility(pVisibility);
  info->setSize(pSize);
  if (pInput)
    info->setResolvedOrigin(pInput);
  if (pBinding != ResolveInfo::Local)
    return info;
  if (getSymbolTracingRequested() && m_Config.options().traceSymbol(pName)) {
    std::string inputPath =
        pInput ? pInput->getInput()->decoratedPath() : "(Not applicable)";
    m_Config.raise(diag::add_new_symbol)
        << pName << inputPath << info->infoAsString();
  }
  m_Locals.push_back(info);
  return info;
}

ResolveInfo *NamePool::insertLocalSymbol(ResolveInfo inputSymRI,
                                         const LDSymbol &sym) {
  ASSERT(inputSymRI.isLocal(), "Invalid symbol binding!");
  ResolveInfo *RI = make<ResolveInfo>(inputSymRI);
  InputFile *IF = inputSymRI.resolvedOrigin();
  if (getSymbolTracingRequested() &&
      m_Config.options().traceSymbol(sym, inputSymRI)) {
    std::string inputPath =
        IF ? IF->getInput()->decoratedPath() : "(Not applicable)";
    m_Config.raise(diag::add_new_symbol)
        << getDecoratedName(sym, inputSymRI) << inputPath << RI->infoAsString();
  }
  m_Locals.push_back(RI);
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
    InputFile *pInput, std::string pName, bool pIsDyn, ResolveInfo::Type pType,
    ResolveInfo::Desc pDesc, ResolveInfo::Binding pBinding,
    ResolveInfo::SizeType pSize, LDSymbol::ValueType pValue,
    ResolveInfo::Visibility pVisibility, ResolveInfo *pOldInfo,
    Resolver::Result &pResult, bool isPostLTOPhase, bool isBitCode,
    unsigned int pSymIdx, bool isPatchable, DiagnosticPrinter *Printer) {
  bool exist = false;
  ResolveInfo *new_symbol = nullptr;
  ResolveInfo *old_symbol = nullptr;
  auto I = m_Globals.find(pName);

  // Setup the New symbol.
  llvm::StringRef SymName = Saver.save(pName);
  new_symbol = make<ResolveInfo>(SymName);

  if (I == m_Globals.end()) {
    m_Globals[pName] = new_symbol;
  } else {
    old_symbol = I->getValue();
    exist = true;
  }

  new_symbol->setIsSymbol(true);
  new_symbol->setSource(pIsDyn);
  new_symbol->setType(pType);
  new_symbol->setDesc(pDesc);
  new_symbol->setBinding(pBinding);
  new_symbol->setVisibility(pVisibility);
  new_symbol->setSize(pSize);
  if (pInput)
    new_symbol->setResolvedOrigin(pInput);
  if (isBitCode)
    new_symbol->setInBitCode(isBitCode);
  if (isPatchable)
    new_symbol->setPatchable();

  SymbolInfo symInfo(pInput, pSize, pBinding, pType, pVisibility, pDesc,
                     isBitCode);
  // We do not create input symbols for non object file symbols!
  DiagnosticPrinter *DP = m_Config.getPrinter();
  auto oldErrorCount = DP->getNumErrors() + DP->getNumFatalErrors();
  PM.callVisitSymbolHook(/*sym=*/nullptr, SymName, symInfo);
  auto newErrorCount = DP->getNumErrors() + DP->getNumFatalErrors();
  if (newErrorCount != oldErrorCount)
    return false;

  bool traceme = false;

  if (getSymbolTracingRequested() && m_Config.options().traceSymbol(pName)) {
    traceme = true;
    std::string inputPath =
        pInput ? pInput->getInput()->decoratedPath() : "(Not applicable)";
    m_Config.raise(diag::add_new_symbol)
        << pName << inputPath << new_symbol->infoAsString();
  }

  if (!exist) {
    // old_symbol is neither existed nor a symbol.
    pResult.info = new_symbol;
    pResult.existent = false;
    pResult.overriden = true;
    return true;
  } else if (nullptr != pOldInfo) {
    // existent, remember its attribute
    pOldInfo->override(*old_symbol);
  }

  if (!canSymbolsBeResolved(old_symbol, new_symbol))
    return false;

  // exist and is a symbol
  // symbol resolution
  bool override = false;

  bool overrideByLTO =
      isPostLTOPhase && old_symbol->isBitCode() && !new_symbol->isBitCode();
  if (isPostLTOPhase && !old_symbol->isBitCode() && !new_symbol->isBitCode() &&
      !old_symbol->isUndef() && !old_symbol->isDyn() &&
      !old_symbol->isCommon() && old_symbol->outSymbol()) {
    if (old_symbol->outSymbol()->hasFragRef() && old_symbol->outSymbol()
                                                     ->fragRef()
                                                     ->frag()
                                                     ->getOwningSection()
                                                     ->isIgnore())
      overrideByLTO = true;
  }
  if (overrideByLTO) {

    old_symbol->override(*new_symbol, false);
    old_symbol->setSize(new_symbol->size());
    old_symbol->overrideAttributes(*new_symbol);
    old_symbol->overrideVisibility(*new_symbol);
    old_symbol->setBinding(new_symbol->binding());
    old_symbol->setInBitCode(new_symbol->isBitCode());

    pResult.info = old_symbol;
    pResult.existent = true;
    pResult.overriden = true;
  } else if (m_pResolver->resolve(*old_symbol, *new_symbol, override, pValue,
                                  &m_Config, isPostLTOPhase)) {
    pResult.info = old_symbol;
    pResult.existent = true;
    pResult.overriden = override;
    if (pOldInfo)
      pResult.info->setReserved(pOldInfo->reserved());
  } else {
    return false;
  }

  if (traceme) {
    InputFile *resolvedOrigin = old_symbol->resolvedOrigin();
    std::string inputPath = resolvedOrigin
                                ? resolvedOrigin->getInput()->decoratedPath()
                                : "(Not applicable)";
    m_Config.raise(diag::resolve_new_symbol)
        << pName << inputPath << old_symbol->infoAsString();
  }
  return true;
}

bool NamePool::getSymbolTracingRequested() const {
  return m_SymbolTracingRequested;
}
/// findInfo - find the resolved ResolveInfo
ResolveInfo *NamePool::findInfo(std::string pName) {
  auto I = m_Globals.find(pName);
  if (I == m_Globals.end())
    return nullptr;
  return I->getValue();
}

/// findInfo - find the resolved ResolveInfo
// FIXME: Pass name by const reference, or use llvm::StringRef
const ResolveInfo *NamePool::findInfo(std::string pName) const {
  auto I = m_Globals.find(pName);
  if (I == m_Globals.end())
    return nullptr;
  return I->getValue();
}

/// findSymbol - find the resolved output LDSymbol
// FIXME: Pass name by const reference, or use llvm::StringRef
LDSymbol *NamePool::findSymbol(std::string pName) {
  ResolveInfo *info = findInfo(pName);
  if (nullptr == info)
    return nullptr;
  return info->outSymbol();
}

/// findSymbol - find the resolved output LDSymbol
// FIXME: Pass name by const reference, or use llvm::StringRef
const LDSymbol *NamePool::findSymbol(std::string pName) const {
  const ResolveInfo *info = findInfo(pName);
  if (nullptr == info)
    return nullptr;
  return info->outSymbol();
}

void NamePool::setupNullSymbol() {
  // Setup Null LDSymbol.
  LDSymbol *NullSymbol = LDSymbol::Null();
  if (!NullSymbol->resolveInfo()) {
    ResolveInfo::Null()->setBinding(ResolveInfo::Local);
    ResolveInfo::Null()->setOutSymbol(NullSymbol);
    NullSymbol->setResolveInfo(*ResolveInfo::Null());
  }
}

/// createSymbol - create a symbol
LDSymbol *NamePool::createPluginSymbol(InputFile *pInput, std::string pName,
                                       Fragment *pFragment, uint64_t Val,
                                       LayoutPrinter *LP) {
  llvm::StringRef SymName = Saver.save(pName);
  ResolveInfo *info = make<ResolveInfo>(SymName);
  info->setIsSymbol(true);
  info->setSource(false);
  info->setType(ResolveInfo::NoType);
  info->setDesc(ResolveInfo::Define);
  info->setBinding(ResolveInfo::Local);
  info->setVisibility(ResolveInfo::Default);
  info->setSize(0);
  if (pInput)
    info->setResolvedOrigin(pInput);
  LDSymbol *Sym = make<LDSymbol>(info, false);
  if (Val > pFragment->getOwningSection()->size())
    Val = pFragment->getOwningSection()->size();
  Sym->setFragmentRef(make<FragmentRef>(*pFragment, Val));
  info->setOutSymbol(Sym);
  m_Locals.push_back(info);
  if (LP && LP->showSymbolResolution())
    getSRI().recordSymbolInfo(
        Sym, SymbolInfo{pInput, info->size(),
                        static_cast<ResolveInfo::Binding>(info->binding()),
                        static_cast<ResolveInfo::Type>(info->type()),
                        info->visibility(),
                        static_cast<ResolveInfo::Desc>(info->desc()),
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
  m_Config.raise(diag::tls_non_tls_symbol_mismatch) << ErrMessage;
  return false;
}

bool NamePool::canSymbolsBeResolved(const ResolveInfo *Old,
                                    const ResolveInfo *New) const {
  return checkTLSTypes(Old, New);
}

ResolveInfo NamePool::createInputSymbolRI(
    const std::string &symName, InputFile &IF, bool isDyn,
    ResolveInfo::Type symType, ResolveInfo::Desc symDesc,
    ResolveInfo::Binding symBinding, ResolveInfo::SizeType symSize,
    ResolveInfo::Visibility symVisibility, LDSymbol::ValueType symValue,
    bool isPatchable) const {
  llvm::StringRef symNameRef = Saver.save(symName);
  ResolveInfo RI(symNameRef);
  RI.setIsSymbol(true);
  RI.setSource(isDyn);
  RI.setType(symType);
  RI.setDesc(symDesc);
  RI.setBinding(symBinding);
  RI.setVisibility(symVisibility);
  RI.setSize(symSize);
  RI.setResolvedOrigin(&IF);
  RI.setValue(symValue, /*isFinal=*/false);
  RI.setInBitCode(IF.isBitcode());
  if (isPatchable)
    RI.setPatchable();
  return RI;
}

bool NamePool::insertNonLocalSymbol(ResolveInfo inputSymRI, const LDSymbol &sym,
                                    bool isPostLTOPhase,
                                    Resolver::Result &pResult) {
  bool exist = false;
  llvm::StringRef symName = inputSymRI.getName();
  auto I = m_Globals.find(symName);
  ResolveInfo *old_symbol = nullptr;
  if (I == m_Globals.end()) {
    m_Globals[symName] = make<ResolveInfo>(inputSymRI);
  } else {
    old_symbol = I->getValue();
    exist = true;
  }

  bool traceme = false;
  if (getSymbolTracingRequested() &&
      m_Config.options().traceSymbol(sym, inputSymRI)) {
    traceme = true;
    InputFile *IF = inputSymRI.resolvedOrigin();
    std::string inputPath =
        IF ? IF->getInput()->decoratedPath() : "(Not applicable)";
    m_Config.raise(diag::add_new_symbol)
        << getDecoratedName(sym, inputSymRI) << inputPath
        << inputSymRI.infoAsString();
  }

  if (!exist) {
    pResult.info = m_Globals[symName];
    pResult.existent = false;
    pResult.overriden = true;
    return true;
  }

  if (!canSymbolsBeResolved(old_symbol, &inputSymRI))
    return false;

  bool override = false;

  bool overrideByLTO =
      isPostLTOPhase && old_symbol->isBitCode() && !inputSymRI.isBitCode();
  if (isPostLTOPhase && !old_symbol->isBitCode() && !inputSymRI.isBitCode() &&
      !old_symbol->isUndef() && !old_symbol->isDyn() &&
      !old_symbol->isCommon() && old_symbol->outSymbol()) {
    if (old_symbol->outSymbol()->hasFragRef() && old_symbol->outSymbol()
                                                     ->fragRef()
                                                     ->frag()
                                                     ->getOwningSection()
                                                     ->isIgnore())
      overrideByLTO = true;
  }

  if (overrideByLTO) {
    old_symbol->override(inputSymRI, false);
    old_symbol->setSize(inputSymRI.size());
    old_symbol->overrideAttributes(inputSymRI);
    old_symbol->overrideVisibility(inputSymRI);
    old_symbol->setBinding(inputSymRI.binding());
    old_symbol->setInBitCode(inputSymRI.isBitCode());

    pResult.info = old_symbol;
    pResult.existent = true;
    pResult.overriden = true;
  } else if (m_pResolver->resolve(*old_symbol, inputSymRI, override,
                                  inputSymRI.value(), &m_Config,
                                  isPostLTOPhase)) {
    pResult.info = old_symbol;
    pResult.existent = true;
    pResult.overriden = override;
  } else {
    return false;
  }
  if (traceme) {
    InputFile *resolvedOrigin = old_symbol->resolvedOrigin();
    std::string inputPath = resolvedOrigin
                                ? resolvedOrigin->getInput()->decoratedPath()
                                : "(Not applicable)";
    m_Config.raise(diag::resolve_new_symbol)
        << getDecoratedName(sym, inputSymRI) << inputPath
        << old_symbol->infoAsString();
  }
  return true;
}

std::string NamePool::getDecoratedName(const LDSymbol &sym,
                                       const ResolveInfo &RI) const {
  ResolveInfo info(RI);
  LDSymbol S(sym);
  info.setOutSymbol(&S);
  return info.getDecoratedName(m_Config.options().shouldDemangle());
}
