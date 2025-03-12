//===- IRBuilder.cpp-------------------------------------------------------===//
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

#include "eld/SymbolResolver/IRBuilder.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticPrinter.h"
#include "eld/Fragment/FragmentRef.h"
#include "eld/Fragment/RegionFragmentEx.h"
#include "eld/Input/ArchiveMemberInput.h"
#include "eld/Input/ELFDynObjectFile.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/Object/ObjectBuilder.h"
#include "eld/Support/Memory.h"
#include "eld/Support/MemoryArea.h"
#include "eld/Support/MsgHandling.h"
#include "eld/Support/RegisterTimer.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/SymbolResolver/NamePool.h"
#include "eld/SymbolResolver/SymbolInfo.h"
#include "eld/SymbolResolver/SymbolResolutionInfo.h"
#include "eld/Target/Relocator.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/BinaryFormat/ELF.h"
#include <unordered_map>

using namespace eld;

//===----------------------------------------------------------------------===//
// IRBuilder
//===----------------------------------------------------------------------===//
IRBuilder::IRBuilder(Module &pModule, LinkerConfig &pConfig)
    : m_Module(pModule), m_Config(pConfig), m_InputBuilder(pConfig) {
  m_isGC = pConfig.options().GCSections();
}

IRBuilder::~IRBuilder() {}

void IRBuilder::requestGarbageCollection() { m_isGC = true; }

LDSymbol *IRBuilder::makeLDSymbol(ResolveInfo *R) {
  return make<LDSymbol>(R, m_isGC);
}

/// AddSymbol - To add a symbol in the input file and resolve the symbol
/// immediately
LDSymbol *IRBuilder::AddSymbol(InputFile &pInput, const std::string &pName,
                               ResolveInfo::Type pType, ResolveInfo::Desc pDesc,
                               ResolveInfo::Binding pBind,
                               ResolveInfo::SizeType pSize,
                               LDSymbol::ValueType pValue, ELFSection *pSection,
                               ResolveInfo::Visibility pVis,
                               bool isPostLTOPhase, uint32_t shndx,
                               uint32_t idx, bool isPatchable) {
  if (pInput.getInput()->getAttribute().isPatchBase()) {
    // Add patchable symbols from the base image because they will be referred
    // from relocation by indexes. We have to add all symbols including local
    // to ensure indexes the indexes are correct.
    // Non-patchable symbols from the base will be added as normal symbols.
    // Not sure how to better do this. Global symbols can become undefined
    // + provide, or weak. Absolute should be converted to global as there are
    // no undefined absolute. Patching local symbols is not supported, but it
    // could be added in the future. In that case, they have to become undefined
    // + provide, cannot be weak. But it won't work with provide? But the bigger
    // problem with local symbols is that they can't be resolved by name.
    if (isPatchable) {
      // Add patchable symbols as "sym-def".
      // Ignore patchable non-global or hidden symbols.
      if (pBind == ResolveInfo::Global || pBind == ResolveInfo::Absolute) {
        m_Module.getBackend()->addSymDefProvideSymbol(
            pName, pType, pValue, &pInput, /* isPatchable */ true);
        // Also add them as undefined because it's needed to process
        // relocations.
        pDesc = ResolveInfo::Undefined;
        pBind = ResolveInfo::Global;
        pSection = nullptr;
        shndx = llvm::ELF::SHN_UNDEF;
      }
    } else {
      // Non-patchable patch-base symbols are added as Absolute.
      if (pBind == ResolveInfo::Global && pDesc == ResolveInfo::Define) {
        pBind = ResolveInfo::Absolute;
        pSection = nullptr;
        shndx = llvm::ELF::SHN_ABS;
      }
    }
    // Non-patchable symbols are added as normal defined symbols.
  } else if (pInput.getInput()->getAttribute().isJustSymbols()) {
    m_Module.getBackend()->addSymDefProvideSymbol(pName, pType, pValue,
                                                  &pInput);
    return nullptr;
  }
  // rename symbols
  std::string name = pName;
  ResolveInfo::Binding binding = pBind;
  ELFObjectFile *EObj = llvm::dyn_cast<ELFObjectFile>(&pInput);
  if (!m_Config.options().renameMap().empty()) {
    auto renameSym = m_Config.options().renameMap().find(pName);
    if (m_Config.options().renameMap().end() != renameSym) {
      bool traceWrap = m_Module.getPrinter()->traceWrapSymbols();
      // If the renameMap is not empty, some symbols should be renamed.
      // --wrap and --portable defines the symbol rename map.
      if (ResolveInfo::Undefined == pDesc && (!EObj || !EObj->isLTOObject())) {
        name = renameSym->getValue();
        if (traceWrap)
          m_Config.raise(diag::rename_undef_sym)
              << pInput.getInput()->decoratedPath() << pName
              << renameSym->getValue();
      } else if (pDesc == ResolveInfo::Define && EObj && EObj->isLTOObject()) {
        if (traceWrap)
          m_Config.raise(diag::restore_def_binding) << pName;
        binding = static_cast<ResolveInfo::Binding>(
            m_Module.getWrapSymBinding(pName));
      }
    }
  }

  ArchiveMemberInput *AMI =
      llvm::dyn_cast<eld::ArchiveMemberInput>(pInput.getInput());
  // Fix up the visibility if object has no export set.
  if (AMI && AMI->noExport() && (pDesc != ResolveInfo::Undefined)) {
    if ((pVis == ResolveInfo::Default) || (pVis == ResolveInfo::Protected)) {
      pVis = ResolveInfo::Hidden;
    }
  }

  LayoutPrinter *printer = m_Module.getLayoutPrinter();

  switch (pInput.getKind()) {
  case InputFile::BinaryFileKind:
  case InputFile::ELFObjFileKind:
  case InputFile::ELFExecutableFileKind: {

    FragmentRef *fragRef = nullptr;
    if (nullptr == pSection || ResolveInfo::Undefined == pDesc ||
        ResolveInfo::Common == pDesc || ResolveInfo::Absolute == binding ||
        pSection->isIgnore() || pSection->isDiscard() ||
        (pSection->isGroupKind() &&
         LinkerConfig::Object != m_Config.codeGenType())) {
      if (pSection && (pSection->isIgnore() || pSection->isDiscard()))
        fragRef = FragmentRef::Discard();
      else
        fragRef = FragmentRef::Null();
    } else {
      if (pSection->isMergeKind()) {
        auto *Strings = llvm::cast<MergeStringFragment>(
            pSection->getFragmentList().front());
        fragRef = make<FragmentRef>(*Strings, pValue);
      }
      if (!fragRef) {
        fragRef = FragmentRef::Null();
        if (pSection->hasSectionData()) {
          Fragment *frag = pSection->getFragmentList().front();
          fragRef = make<FragmentRef>(*frag, pValue - pSection->addr());
        }
      }
    }

    ObjectFile *ObjFile = llvm::dyn_cast<eld::ObjectFile>(&pInput);

    LDSymbol *input_sym = nullptr;
    {
      eld::RegisterTimer T("Add symbols from object files", "Symbol Resolution",
                           m_Config.options().printTimingStats());

      input_sym = addSymbolFromObject(pInput, name, pType, pDesc, binding,
                                      pSize, pValue, fragRef, pVis, shndx,
                                      isPostLTOPhase, idx, isPatchable);
    }
    // Symbols from non allocatable sections should not participate in garbage
    // collection
    if (isPostLTOPhase && pSection && !pSection->isAlloc()) {
      input_sym->setShouldIgnore(false);
      LDSymbol *outSymbol = input_sym->resolveInfo()->outSymbol();
      if (outSymbol)
        outSymbol->setShouldIgnore(false);
    }
    // FIXME: Why is input_sym nullptr? Shouldn't there be any error in this
    // case?
    if (!input_sym)
      return nullptr;

    // FIXME: Why don't we record symbol if there is no pSection?
    if (printer && pSection) {
      printer->recordFragment(&pInput, pSection, fragRef->frag());
      printer->recordSymbol(fragRef->frag(), input_sym);
    }
    ObjFile->addSymbol(input_sym);
    if (pSection && m_Config.options().isSectionTracingRequested() &&
        m_Config.options().traceSection(pSection))
      m_Config.raise(diag::symbols_in_section_info)
          << pSection->getDecoratedName(m_Config.options())
          << input_sym->name();
    return input_sym;
  }
  case InputFile::ELFDynObjFileKind: {
    {
      eld::RegisterTimer T("Add symbols from dynamic object files",
                           "Symbol Resolution",
                           m_Config.options().printTimingStats());

      LDSymbol *input_sym =
          addSymbolFromDynObj(pInput, name, pType, pDesc, binding, pSize,
                              pValue, pVis, shndx, isPostLTOPhase);
      if (input_sym)
        llvm::cast<ELFDynObjectFile>(&pInput)->addSymbol(input_sym);
      return input_sym;
    }
  }
  default:
    break;
  }
  return nullptr;
}

LDSymbol *IRBuilder::addSymbolFromObject(
    InputFile &pInput, const std::string &pName, ResolveInfo::Type pType,
    ResolveInfo::Desc pDesc, ResolveInfo::Binding pBinding,
    ResolveInfo::SizeType pSize, LDSymbol::ValueType pValue,
    FragmentRef *pFragmentRef, ResolveInfo::Visibility pVisibility,
    uint32_t shndx, bool isPostLTOPhase, uint32_t Idx, bool isPatchable) {
  // Step 1. calculate a Resolver::Result
  // resolved_result is a triple <resolved_info, existent, override>
  Resolver::Result resolved_result = {nullptr, false, false};
  eld::RegisterTimer T("Create && Resolve symbols", "Symbol Resolution",
                       m_Config.options().printTimingStats());
  NamePool &NP = m_Module.getNamePool();
  ResolveInfo inputSymRI =
      NP.createInputSymbolRI(pName, pInput, /*isDyn=*/false, pType, pDesc,
                             pBinding, pSize, pVisibility, pValue, isPatchable);
  LDSymbol *input_sym = makeLDSymbol(nullptr);
  input_sym->setFragmentRef(pFragmentRef);
  input_sym->setSectionIndex(shndx);
  input_sym->setSymbolIndex(Idx);

  auto &PM = m_Module.getPluginManager();
  SymbolInfo symInfo(&pInput, pSize, pBinding, pType, pVisibility, pDesc,
                     /*isBitcode=*/false);
  DiagnosticPrinter *DP = m_Config.getPrinter();
  auto oldErrorCount = DP->getNumErrors() + DP->getNumFatalErrors();
  PM.callVisitSymbolHook(input_sym, inputSymRI.getName(), symInfo);
  auto newErrorCount = DP->getNumErrors() + DP->getNumFatalErrors();
  if (newErrorCount != oldErrorCount)
    return nullptr;

  if (pBinding == ResolveInfo::Binding::Local) {
    ResolveInfo *RI = NP.insertLocalSymbol(inputSymRI, *input_sym);
    RI->setOutSymbol(input_sym);
    input_sym->setResolveInfo(*RI);
    return input_sym;
  }

  if (m_Module.getLayoutPrinter() &&
      m_Module.getLayoutPrinter()->showSymbolResolution())
    NP.getSRI().recordSymbolInfo(input_sym, symInfo);

  bool S = NP.insertNonLocalSymbol(inputSymRI, *input_sym, isPostLTOPhase,
                                   resolved_result);
  if (!S)
    return nullptr;

  if (m_Config.options().cref() || m_Config.options().buildCRef())
    addToCref(pInput, resolved_result);

  // Step 3. Set up corresponding output LDSymbol
  LDSymbol *output_sym = resolved_result.info->outSymbol();

  bool has_output_sym = (nullptr != output_sym);

  input_sym->setResolveInfo(*resolved_result.info);

  if (resolved_result.overriden) {
    if (pFragmentRef->frag()) {
      ELFSection *S = pFragmentRef->frag()->getOwningSection();
      pValue = pValue - S->addr();
    }
    resolved_result.info->setValue(pValue, /*isFinal=*/false);
    resolved_result.info->setOutSymbol(input_sym);
  } else if (!resolved_result.overriden && !has_output_sym) {
    // Set the out symbol for the corresponding shared library symbol because
    // the symbol is referenced by an object file.
    // For shared library symbols, out symbol in ResolveInfo is only set if the
    // symbol is referenced by a relocatable object file.
    LDSymbol *sharedLibSym = NP.getSharedLibSymbol(resolved_result.info);
    if (sharedLibSym)
      resolved_result.info->setOutSymbol(sharedLibSym);
  }

  if (m_Module.getPrinter()->traceSymbols())
    m_Config.raise(diag::obj_symbol_created)
        << input_sym->name() << input_sym->sectionIndex()
        << input_sym->resolveInfo()->infoAsString();
  return input_sym;
}

LDSymbol *IRBuilder::addSymbolFromDynObj(
    InputFile &pInput, const std::string &pName, ResolveInfo::Type pType,
    ResolveInfo::Desc pDesc, ResolveInfo::Binding pBinding,
    ResolveInfo::SizeType pSize, LDSymbol::ValueType pValue,
    ResolveInfo::Visibility pVisibility, uint32_t shndx, bool isPostLTOPhase) {
  // We don't need sections of dynamic objects. So we ignore section symbols.
  if (pType == ResolveInfo::Section)
    return nullptr;

  // ignore symbols with local binding or that have internal or hidden
  // visibility
  if (pBinding == ResolveInfo::Local || pVisibility == ResolveInfo::Internal ||
      pVisibility == ResolveInfo::Hidden)
    return nullptr;

  eld::RegisterTimer T("Create && Resolve dynamic symbols", "Symbol Resolution",
                       m_Config.options().printTimingStats());
  // create an input LDSymbol.
  LDSymbol *input_sym = makeLDSymbol(nullptr);
  input_sym->setFragmentRef(FragmentRef::Null());
  input_sym->setSectionIndex(shndx);

  SymbolInfo symInfo(&pInput, pSize, pBinding, pType, pVisibility, pDesc,
                     /*isBitcode=*/false);

  if (m_Module.getLayoutPrinter() &&
      m_Module.getLayoutPrinter()->showSymbolResolution())
    m_Module.getNamePool().getSRI().recordSymbolInfo(input_sym, symInfo);
  // insert symbol and resolve it immediately
  Resolver::Result resolved_result = {nullptr, false, false};
  NamePool &NP = m_Module.getNamePool();
  ResolveInfo inputSymRI = NP.createInputSymbolRI(
      pName, pInput, /*isDyn=*/true, pType, pDesc, pBinding, pSize, pVisibility,
      pValue, /*isPatchable=*/false);

  auto &PM = m_Module.getPluginManager();
  DiagnosticPrinter *DP = m_Config.getPrinter();
  auto oldErrorCount = DP->getNumErrors() + DP->getNumFatalErrors();
  PM.callVisitSymbolHook(input_sym, inputSymRI.getName(), symInfo);
  auto newErrorCount = DP->getNumErrors() + DP->getNumFatalErrors();
  if (newErrorCount != oldErrorCount)
    return nullptr;
  // Resolve symbol
  bool S = NP.insertNonLocalSymbol(inputSymRI, *input_sym, isPostLTOPhase,
                                   resolved_result);
  if (!S)
    return nullptr;

  // the return ResolveInfo should not nullptr
  assert(nullptr != resolved_result.info);
  if (m_Config.options().cref() || m_Config.options().buildCRef())
    addToCref(pInput, resolved_result);

  input_sym->setResolveInfo(*(resolved_result.info));

  if (resolved_result.overriden || !resolved_result.existent) {
    resolved_result.info->setValue(pValue, false);
    pInput.setNeeded();
    NP.addSharedLibSymbol(input_sym);
  }
  if (resolved_result.overriden && resolved_result.existent) {
    resolved_result.info->setOutSymbol(input_sym);
  }
  // If the symbol is from dynamic library and we are not making a dynamic
  // library, we either need to export the symbol by dynamic list or sometimes
  // we export it since the dynamic library may be referring it defined in
  // executable, either case it must be in .dynsym
  if (m_Config.codeGenType() != LinkerConfig::DynObj &&
      ((input_sym->resolveInfo()->outSymbol() &&
        input_sym->resolveInfo()->outSymbol()->hasFragRef()) ||
       input_sym->resolveInfo()->isCommon()))
    input_sym->resolveInfo()->setExportToDyn();
  return input_sym;
}

void IRBuilder::addToCref(InputFile &pInput, Resolver::Result pResult) {
  eld::RegisterTimer T("Add Symbols to Cross Reference Table",
                       "Symbol Resolution",
                       m_Config.options().printTimingStats());

  GeneralOptions &options = m_Config.options();
  GeneralOptions::CrefTable &table = options.crefTable();
  ResolveInfo *info = pResult.info;

  assert(nullptr != info);

  std::vector<std::pair<const InputFile *, bool>> &RHS = table[info];
  if (pResult.overriden == true && info->isDefine()) {
    // we add symbol to cref in front if it is defined in this file
    RHS.insert(RHS.begin(),
               std::pair<const InputFile *, bool>(&pInput, info->isBitCode()));
  } else {
    // otherwise we simply add it to the end.
    RHS.push_back(
        std::pair<const InputFile *, bool>(&pInput, info->isBitCode()));
  }
}

void IRBuilder::addLinkerInternalLocalSymbol(InputFile *F, std::string Name,
                                             FragmentRef *fragRef,
                                             size_t Size) {
  LDSymbol *Sym = AddSymbol<Force, Resolve>(
      F, Name, ResolveInfo::NoType, ResolveInfo::Define, ResolveInfo::Local,
      Size, 0, fragRef, ResolveInfo::Default, true);
  getModule().addSymbol(Sym->resolveInfo());
}

/// AddRelocation - add a relocation entry
///
/// All symbols should be read and resolved before calling this function.
Relocation *IRBuilder::AddRelocation(const Relocator *pRelocator,
                                     ELFSection *pSection,
                                     Relocation::Type pType, LDSymbol &pSym,
                                     uint32_t pOffset,
                                     Relocation::Address pAddend) {

  ResolveInfo *resolve_info = pSym.resolveInfo();

  if (!resolve_info)
    return nullptr;

  FragmentRef *frag_ref = FragmentRef::Null();
  if (pSection->hasSectionData()) {
    Fragment *frag = pSection->getFragmentList().front();
    frag_ref = make<FragmentRef>(*frag, pOffset);
  }
  Relocation *relocation =
      Relocation::Create(pType, pRelocator->getSize(pType), frag_ref, pAddend);

  relocation->setSymInfo(pSym.resolveInfo());

  return relocation;
}

Relocation *IRBuilder::AddRelocation(const Relocator *pRelocator,
                                     Fragment &pFrag, Relocation::Type pType,
                                     LDSymbol &pSym, uint32_t pOffset,
                                     Relocation::Address pAddend) {

  ResolveInfo *resolve_info = pSym.resolveInfo();

  if (!resolve_info)
    return nullptr;

  if (!pSym.hasFragRef() && ResolveInfo::Section == resolve_info->type() &&
      ResolveInfo::Undefined == resolve_info->desc())
    return nullptr;

  FragmentRef *frag_ref = make<FragmentRef>(pFrag, pOffset);

  Relocation *relocation =
      Relocation::Create(pType, pRelocator->getSize(pType), frag_ref, pAddend);

  relocation->setSymInfo(pSym.resolveInfo());

  return relocation;
}

Relocation *IRBuilder::CreateRelocation(const Relocator *pRelocator,
                                        Fragment &pFrag, Relocation::Type pType,
                                        LDSymbol &pSym, uint32_t pOffset,
                                        Relocation::Address pAddend) {

  ResolveInfo *resolve_info = pSym.resolveInfo();

  if (!resolve_info)
    return nullptr;

  if (!pSym.hasFragRef() && ResolveInfo::Section == resolve_info->type() &&
      ResolveInfo::Undefined == resolve_info->desc())
    return nullptr;

  FragmentRef *frag_ref = make<FragmentRef>(pFrag, pOffset);

  Relocation *relocation =
      make<Relocation>(pRelocator, pType, frag_ref, pAddend);

  relocation->setSymInfo(pSym.resolveInfo());

  return relocation;
}

/// AddSymbol - define an output symbol and override it immediately
template <>
LDSymbol *IRBuilder::AddSymbol<IRBuilder::Force, IRBuilder::Unresolve>(
    InputFile *pInput, std::string pName, ResolveInfo::Type pType,
    ResolveInfo::Desc pDesc, ResolveInfo::Binding pBinding,
    ResolveInfo::SizeType pSize, LDSymbol::ValueType pValue,
    FragmentRef *pFragmentRef, ResolveInfo::Visibility pVisibility,
    bool isPostLTOPhase, bool isBitCode, bool isPatchable) {
  ResolveInfo *info = m_Module.getNamePool().findInfo(pName);
  LDSymbol *output_sym = nullptr;
  if (nullptr == info) {
    // the symbol is not in the pool, create a new one.
    // create a ResolveInfo
    Resolver::Result result;
    bool S = m_Module.getNamePool().insertSymbol(
        pInput, pName, false, pType, pDesc, pBinding, pSize, pValue,
        pVisibility, nullptr, result, isPostLTOPhase, isBitCode, 0, isPatchable,
        m_Module.getPrinter());
    if (!S)
      return nullptr;
    assert(!result.existent);

    // create a output LDSymbol
    output_sym = makeLDSymbol(result.info);
    result.info->setOutSymbol(output_sym);
  } else {
    // the symbol is already in the pool, override it
    ResolveInfo old_info;
    old_info.override(*info);

    info->setRegular();
    info->setType(pType);
    info->setDesc(pDesc);
    info->setBinding(pBinding);
    info->setVisibility(pVisibility);
    info->setIsSymbol(true);
    info->setSize(pSize);

    // create a output LDSymbol
    output_sym = makeLDSymbol(info);
    info->setOutSymbol(output_sym);
  }

  if (nullptr != output_sym) {
    output_sym->setFragmentRef(pFragmentRef);
    output_sym->setValue(pValue, false);
  }

  if (m_Module.getLayoutPrinter() &&
      m_Module.getLayoutPrinter()->showSymbolResolution()) {
    SymbolResolutionInfo &SRI = m_Module.getNamePool().getSRI();
    SRI.recordSymbolInfo(output_sym,
                         SymbolInfo{pInput, pSize, pBinding, pType, pVisibility,
                                    pDesc, /*isBitcode=*/false});
  }

  return output_sym;
}

/// AddSymbol - define an output symbol and override it immediately
template <>
LDSymbol *IRBuilder::AddSymbol<IRBuilder::AsReferred, IRBuilder::Unresolve>(
    InputFile *pInput, std::string pName, ResolveInfo::Type pType,
    ResolveInfo::Desc pDesc, ResolveInfo::Binding pBinding,
    ResolveInfo::SizeType pSize, LDSymbol::ValueType pValue,
    FragmentRef *pFragmentRef, ResolveInfo::Visibility pVisibility,
    bool isPostLTOPhase, bool isBitCode, bool isPatchable) {
  assert(!isPatchable);
  ResolveInfo *info = m_Module.getNamePool().findInfo(pName);

  if (nullptr == info || !(info->isUndef() || info->isDyn())) {
    // only undefined symbol and dynamic symbol can make a reference.
    return nullptr;
  }

  // the symbol is already in the pool, override it
  ResolveInfo old_info;
  old_info.override(*info);

  info->setRegular();
  info->setType(pType);
  info->setDesc(pDesc);
  info->setBinding(pBinding);
  info->setVisibility(pVisibility);
  info->setIsSymbol(true);
  info->setSize(pSize);
  if (!info->resolvedOrigin())
    info->setResolvedOrigin(pInput);

  LDSymbol *output_sym = info->outSymbol();
  if (nullptr != output_sym) {
    output_sym->setFragmentRef(pFragmentRef);
    output_sym->setValue(pValue, false);
  } else {
    // create a output LDSymbol
    output_sym = makeLDSymbol(info);
    info->setOutSymbol(output_sym);
  }

  return output_sym;
}

/// AddSymbol - define an output symbol and resolve it
/// immediately
template <>
LDSymbol *IRBuilder::AddSymbol<IRBuilder::Force, IRBuilder::Resolve>(
    InputFile *pInput, std::string pName, ResolveInfo::Type pType,
    ResolveInfo::Desc pDesc, ResolveInfo::Binding pBinding,
    ResolveInfo::SizeType pSize, LDSymbol::ValueType pValue,
    FragmentRef *pFragmentRef, ResolveInfo::Visibility pVisibility,
    bool isPostLTOPhase, bool isBitCode, bool isPatchable) {
  // Result is <info, existent, override>
  Resolver::Result result;
  ResolveInfo old_info;

  bool S = m_Module.getNamePool().insertSymbol(
      pInput, pName, false, pType, pDesc, pBinding, pSize, pValue, pVisibility,
      &old_info, result, isPostLTOPhase, isBitCode, 0, isPatchable,
      m_Module.getPrinter());

  if (!S)
    return nullptr;

  LDSymbol *output_sym = result.info->outSymbol();
  bool has_output_sym = (nullptr != output_sym);

  if (!result.existent || !has_output_sym) {
    output_sym = makeLDSymbol(result.info);
    result.info->setOutSymbol(output_sym);
  }

  if (result.overriden || !has_output_sym) {
    output_sym->setFragmentRef(pFragmentRef);
    output_sym->setValue(pValue, false);
  }

  return output_sym;
}

/// defineSymbol - define an output symbol and resolve it immediately.
template <>
LDSymbol *IRBuilder::AddSymbol<IRBuilder::AsReferred, IRBuilder::Resolve>(
    InputFile *pInput, std::string pName, ResolveInfo::Type pType,
    ResolveInfo::Desc pDesc, ResolveInfo::Binding pBinding,
    ResolveInfo::SizeType pSize, LDSymbol::ValueType pValue,
    FragmentRef *pFragmentRef, ResolveInfo::Visibility pVisibility,
    bool isPostLTOPhase, bool isBitCode, bool isPatchable) {
  ResolveInfo *info = m_Module.getNamePool().findInfo(pName);

  if (nullptr == info || !(info->isUndef() || info->isDyn())) {
    // only undefined symbol and dynamic symbol can make a reference.
    return nullptr;
  }

  return AddSymbol<Force, Resolve>(pInput, pName, pType, pDesc, pBinding, pSize,
                                   pValue, pFragmentRef, pVisibility,
                                   isPostLTOPhase, isBitCode, isPatchable);
}
