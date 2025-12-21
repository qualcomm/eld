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
#include "eld/Input/ObjectFile.h"
#include "eld/Object/ObjectBuilder.h"
#include "eld/Support/Memory.h"
#include "eld/Support/MemoryArea.h"
#include "eld/Support/MsgHandling.h"
#include "eld/Support/RegisterTimer.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/SymbolResolver/NamePool.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "eld/SymbolResolver/SymbolInfo.h"
#include "eld/SymbolResolver/SymbolResolutionInfo.h"
#include "eld/Target/Relocator.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Object/ELF.h"
#include "llvm/Support/StringSaver.h"
#include <unordered_map>

using namespace eld;

//===----------------------------------------------------------------------===//
// IRBuilder
//===----------------------------------------------------------------------===//
IRBuilder::IRBuilder(Module &CurModule, LinkerConfig &Config)
    : ThisModule(CurModule), ThisConfig(Config), ThisInputBuilder(Config) {
  IsGarbageCollected = Config.options().gcSections();
}

IRBuilder::~IRBuilder() {}

void IRBuilder::requestGarbageCollection() { IsGarbageCollected = true; }

LDSymbol *IRBuilder::makeLDSymbol(ResolveInfo *R) {
  return make<LDSymbol>(R, IsGarbageCollected);
}

/// addSymbol - To add a symbol in the input file and resolve the symbol
/// immediately
LDSymbol *IRBuilder::addSymbol(InputFile &Input, const std::string &SymbolName,
                               ResolveInfo::Type Type, ResolveInfo::Desc Desc,
                               ResolveInfo::Binding Bind,
                               ResolveInfo::SizeType Size,
                               LDSymbol::ValueType Value,
                               ELFSection *CurSection,
                               ResolveInfo::Visibility Vis, bool IsPostLtoPhase,
                               uint32_t Shndx, uint32_t Idx, bool IsPatchable) {
  if (Input.getInput()->getAttribute().isPatchBase()) {
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
    if (IsPatchable) {
      // Add patchable symbols as "sym-def".
      // Ignore patchable non-global or hidden symbols.
      if (Bind == ResolveInfo::Global || Bind == ResolveInfo::Absolute) {
        ThisModule.getBackend().addSymDefProvideSymbol(
            SymbolName, Type, Value, &Input, /* isPatchable */ true);
        // Also add them as undefined because it's needed to process
        // relocations.
        Desc = ResolveInfo::Undefined;
        Bind = ResolveInfo::Global;
        CurSection = nullptr;
        Shndx = llvm::ELF::SHN_UNDEF;
      }
    } else {
      // Non-patchable patch-base symbols are added as Absolute.
      if (Bind == ResolveInfo::Global && Desc == ResolveInfo::Define) {
        Bind = ResolveInfo::Absolute;
        CurSection = nullptr;
        Shndx = llvm::ELF::SHN_ABS;
      }
    }
    // Non-patchable symbols are added as normal defined symbols.
  } else if (Input.getInput()->getAttribute().isJustSymbols()) {
    ThisModule.getBackend().addSymDefProvideSymbol(SymbolName, Type, Value,
                                                   &Input);
    return nullptr;
  }
  // rename symbols
  std::string Name = SymbolName;
  ResolveInfo::Binding Binding = Bind;
  ELFObjectFile *EObj = llvm::dyn_cast<ELFObjectFile>(&Input);
  if (!ThisConfig.options().renameMap().empty()) {
    auto RenameSym = ThisConfig.options().renameMap().find(SymbolName);
    if (ThisConfig.options().renameMap().end() != RenameSym) {
      bool TraceWrap = ThisModule.getPrinter()->traceWrapSymbols();
      // If the renameMap is not empty, some symbols should be renamed.
      // --wrap and --portable defines the symbol rename map.
      if (ResolveInfo::Undefined == Desc && (!EObj || !EObj->isLTOObject())) {
        Name = RenameSym->getValue();
        if (TraceWrap)
          ThisConfig.raise(Diag::rename_undef_sym)
              << Input.getInput()->decoratedPath() << SymbolName
              << RenameSym->getValue();
      } else if (Desc == ResolveInfo::Define && EObj && EObj->isLTOObject()) {
        if (TraceWrap)
          ThisConfig.raise(Diag::restore_def_binding) << SymbolName;
        Binding = static_cast<ResolveInfo::Binding>(
            ThisModule.getWrapSymBinding(SymbolName));
      }
    }
  }

  ArchiveMemberInput *AMI =
      llvm::dyn_cast<eld::ArchiveMemberInput>(Input.getInput());
  // Fix up the visibility if object has no export set.
  if (AMI && AMI->noExport() && (Desc != ResolveInfo::Undefined)) {
    if ((Vis == ResolveInfo::Default) || (Vis == ResolveInfo::Protected)) {
      Vis = ResolveInfo::Hidden;
    }
  }

  LayoutInfo *layoutInfo = ThisModule.getLayoutInfo();

  switch (Input.getKind()) {
  case InputFile::BinaryFileKind:
  case InputFile::ELFObjFileKind:
  case InputFile::ELFExecutableFileKind: {

    FragmentRef *FragRef = nullptr;
    if (nullptr == CurSection || ResolveInfo::Undefined == Desc ||
        ResolveInfo::Common == Desc || ResolveInfo::Absolute == Binding ||
        CurSection->isIgnore() || CurSection->isDiscard() ||
        (CurSection->isGroupKind() &&
         LinkerConfig::Object != ThisConfig.codeGenType())) {
      if (CurSection && (CurSection->isIgnore() || CurSection->isDiscard()))
        FragRef = FragmentRef::discard();
      else
        FragRef = FragmentRef::null();
    } else {
      if (CurSection->isMergeKind()) {
        auto *Strings = llvm::cast<MergeStringFragment>(
            CurSection->getFragmentList().front());
        FragRef = make<FragmentRef>(*Strings, Value);
      }
      if (!FragRef) {
        FragRef = FragmentRef::null();
        if (CurSection->hasSectionData()) {
          Fragment *Frag = CurSection->getFragmentList().front();
          FragRef = make<FragmentRef>(*Frag, Value - CurSection->addr());
        }
      }
    }

    ObjectFile *ObjFile = llvm::dyn_cast<eld::ObjectFile>(&Input);

    LDSymbol *InputSym = nullptr;
    {
      eld::RegisterTimer T("Add symbols from object files", "Symbol Resolution",
                           ThisConfig.options().printTimingStats());

      InputSym = addSymbolFromObject(Input, Name, Type, Desc, Binding, Size,
                                     Value, FragRef, Vis, Shndx, IsPostLtoPhase,
                                     Idx, IsPatchable);
    }
    // Symbols from non allocatable sections should not participate in garbage
    // collection
    if (IsPostLtoPhase && CurSection && !CurSection->isAlloc()) {
      InputSym->setShouldIgnore(false);
      LDSymbol *OutSymbol = InputSym->resolveInfo()->outSymbol();
      if (OutSymbol)
        OutSymbol->setShouldIgnore(false);
    }
    // FIXME: Why is input_sym nullptr? Shouldn't there be any error in this
    // case?
    if (!InputSym)
      return nullptr;

    // FIXME: Why don't we record symbol if there is no pSection?
    if (layoutInfo && CurSection) {
      layoutInfo->recordFragment(&Input, CurSection, FragRef->frag());
      layoutInfo->recordSymbol(FragRef->frag(), InputSym);
    }
    ObjFile->addSymbol(InputSym);
    if (CurSection && ThisConfig.options().isSectionTracingRequested() &&
        ThisConfig.options().traceSection(CurSection))
      ThisConfig.raise(Diag::symbols_in_section_info)
          << CurSection->getDecoratedName(ThisConfig.options())
          << InputSym->name();
    return InputSym;
  }
  case InputFile::ELFDynObjFileKind: {
    {
      eld::RegisterTimer T("Add symbols from dynamic object files",
                           "Symbol Resolution",
                           ThisConfig.options().printTimingStats());

      LDSymbol *InputSym =
          addSymbolFromDynObj(Input, Name, Type, Desc, Binding, Size, Value,
                              Vis, Shndx, IsPostLtoPhase, Idx);
      if (InputSym)
        llvm::cast<ELFDynObjectFile>(&Input)->addSymbol(InputSym);
      return InputSym;
    }
  }
  default:
    break;
  }
  return nullptr;
}

LDSymbol *IRBuilder::addSymbolFromObject(
    InputFile &Input, const std::string &SymbolName, ResolveInfo::Type Type,
    ResolveInfo::Desc Desc, ResolveInfo::Binding Binding,
    ResolveInfo::SizeType Size, LDSymbol::ValueType Value,
    FragmentRef *CurFragmentRef, ResolveInfo::Visibility Visibility,
    uint32_t Shndx, bool IsPostLtoPhase, uint32_t Idx, bool IsPatchable) {
  // Step 1. calculate a Resolver::Result
  // ResolvedResult is a triple <resolved_info, existent, override>
  Resolver::Result ResolvedResult = {nullptr, false, false};
  eld::RegisterTimer T("Create && Resolve symbols", "Symbol Resolution",
                       ThisConfig.options().printTimingStats());
  NamePool &NP = ThisModule.getNamePool();
  ResolveInfo InputSymbolResolveInfo =
      NP.createInputSymbolRI(SymbolName, Input, /*isDyn=*/false, Type, Desc,
                             Binding, Size, Visibility, Value, IsPatchable);
  LDSymbol *InputSym = makeLDSymbol(nullptr);
  InputSym->setFragmentRef(CurFragmentRef);
  InputSym->setSectionIndex(Shndx);
  InputSym->setSymbolIndex(Idx);

  auto &PM = ThisModule.getPluginManager();
  SymbolInfo SymInfo(&Input, Size, Binding, Type, Visibility, Desc,
                     /*isBitcode=*/false);
  DiagnosticPrinter *DP = ThisConfig.getPrinter();
  auto OldErrorCount = DP->getNumErrors() + DP->getNumFatalErrors();
  PM.callVisitSymbolHook(InputSym, InputSymbolResolveInfo.getName(), SymInfo);
  auto NewErrorCount = DP->getNumErrors() + DP->getNumFatalErrors();
  if (NewErrorCount != OldErrorCount)
    return nullptr;

  if (Binding == ResolveInfo::Binding::Local) {
    ResolveInfo *RI = NP.insertLocalSymbol(InputSymbolResolveInfo, *InputSym);
    RI->setOutSymbol(InputSym);
    InputSym->setResolveInfo(*RI);
    return InputSym;
  }

  if (ThisModule.getLayoutInfo() &&
      ThisModule.getLayoutInfo()->showSymbolResolution())
    NP.getSRI().recordSymbolInfo(InputSym, SymInfo);

  bool S = NP.insertNonLocalSymbol(InputSymbolResolveInfo, *InputSym,
                                   IsPostLtoPhase, ResolvedResult);
  if (!S)
    return nullptr;

  if (ThisConfig.options().cref() || ThisConfig.options().buildCRef())
    addToCref(Input, ResolvedResult);

  // Step 3. Set up corresponding output LDSymbol
  LDSymbol *OutputSym = ResolvedResult.Info->outSymbol();

  bool HasOutputSym = (nullptr != OutputSym);

  InputSym->setResolveInfo(*ResolvedResult.Info);

  if (ResolvedResult.Overriden) {
    if (CurFragmentRef->frag()) {
      ELFSection *S = CurFragmentRef->frag()->getOwningSection();
      Value = Value - S->addr();
    }
    ResolvedResult.Info->setValue(Value, /*isFinal=*/false);
    ResolvedResult.Info->setOutSymbol(InputSym);
  } else if (!ResolvedResult.Overriden && !HasOutputSym) {
    // Set the out symbol for the corresponding shared library symbol because
    // the symbol is referenced by an object file.
    // For shared library symbols, out symbol in ResolveInfo is only set if the
    // symbol is referenced by a relocatable object file.
    LDSymbol *SharedLibSym = NP.getSharedLibSymbol(ResolvedResult.Info);
    if (SharedLibSym)
      ResolvedResult.Info->setOutSymbol(SharedLibSym);
  }

  if (ThisModule.getPrinter()->traceSymbols())
    ThisConfig.raise(Diag::obj_symbol_created)
        << InputSym->name() << InputSym->sectionIndex()
        << InputSym->resolveInfo()->infoAsString();
  return InputSym;
}

LDSymbol *IRBuilder::addSymbolFromDynObj(
    InputFile &Input, const std::string &SymbolName, ResolveInfo::Type Type,
    ResolveInfo::Desc Desc, ResolveInfo::Binding Binding,
    ResolveInfo::SizeType Size, LDSymbol::ValueType Value,
    ResolveInfo::Visibility Visibility, uint32_t Shndx, bool IsPostLtoPhase,
    uint32_t SymIdx) {
  // We don't need sections of dynamic objects. So we ignore section symbols.
  if (Type == ResolveInfo::Section)
    return nullptr;

  // ignore symbols with local binding or that have internal or hidden
  // visibility
  if (Binding == ResolveInfo::Local || Visibility == ResolveInfo::Internal ||
      Visibility == ResolveInfo::Hidden)
    return nullptr;

  eld::RegisterTimer T("Create && Resolve dynamic symbols", "Symbol Resolution",
                       ThisConfig.options().printTimingStats());
  NamePool &NP = ThisModule.getNamePool();
  ResolveInfo InputSymbolResolveInfo = NP.createInputSymbolRI(
      SymbolName, Input, /*isDyn=*/true, Type, Desc, Binding, Size, Visibility,
      Value, /*isPatchable=*/false);

#ifdef ELD_ENABLE_SYMBOL_VERSIONING
  ELFDynObjectFile *DynObjFile = llvm::cast<ELFDynObjectFile>(&Input);
  bool IsDefaultVersionedSymbol = false;
  std::optional<std::string> OptVersionedName;
  if (DynObjFile->hasSymbolVersioningInfo()) {
    llvm::StringRef VerName =
        (ThisConfig.targets().is32Bits()
             ? DynObjFile->getSymbolVersionName<llvm::object::ELF32LE>(SymIdx,
                                                                       Desc)
             : DynObjFile->getSymbolVersionName<llvm::object::ELF64LE>(SymIdx,
                                                                       Desc));
    // Version name is empty for the VER_NDX_GLOBAL and VER_NDX_LOCAL versions.
    if (!VerName.empty())
      OptVersionedName = SymbolName + "@" + VerName.str();

    IsDefaultVersionedSymbol = DynObjFile->isDefaultVersionedSymbol(SymIdx);
  }
#endif
  SymbolInfo SymInfo(&Input, Size, Binding, Type, Visibility, Desc,
                     /*isBitcode=*/false);
  auto AddSymbol = [&Input, IsPostLtoPhase, &NP, Shndx, SymIdx, SymInfo, this,
                    Value](ResolveInfo RI) -> LDSymbol * {
    // insert symbol and resolve it immediately
    // create an input LDSymbol.
    LDSymbol *InputSym = makeLDSymbol(nullptr);
    InputSym->setFragmentRef(FragmentRef::null());
    InputSym->setSectionIndex(Shndx);
    InputSym->setSymbolIndex(SymIdx);

    if (ThisModule.getLayoutInfo() &&
        ThisModule.getLayoutInfo()->showSymbolResolution())
      ThisModule.getNamePool().getSRI().recordSymbolInfo(InputSym, SymInfo);

    Resolver::Result ResolvedResult = {nullptr, false, false};
    auto &PM = ThisModule.getPluginManager();
    DiagnosticPrinter *DP = ThisConfig.getPrinter();
    auto OldErrorCount = DP->getNumErrors() + DP->getNumFatalErrors();
    PM.callVisitSymbolHook(InputSym, RI.getName(), SymInfo);
    auto NewErrorCount = DP->getNumErrors() + DP->getNumFatalErrors();
    if (NewErrorCount != OldErrorCount)
      return nullptr;
    bool S =
        NP.insertNonLocalSymbol(RI, *InputSym, IsPostLtoPhase, ResolvedResult);
    // Resolve symbol
    if (!S)
      return nullptr;
    // the return ResolveInfo should not nullptr
    assert(nullptr != ResolvedResult.Info);
    if (ThisConfig.options().cref() || ThisConfig.options().buildCRef())
      addToCref(Input, ResolvedResult);

    InputSym->setResolveInfo(*(ResolvedResult.Info));
    if (ResolvedResult.Overriden || !ResolvedResult.Existent) {
      ResolvedResult.Info->setValue(Value, false);
      Input.setNeeded();
      NP.addSharedLibSymbol(InputSym);
    }
    if (ResolvedResult.Overriden && ResolvedResult.Existent) {
      ResolvedResult.Info->setOutSymbol(InputSym);
    }
    // If the symbol is from dynamic library and we are not making a dynamic
    // library, we either need to export the symbol by dynamic list or sometimes
    // we export it since the dynamic library may be referring it defined in
    // executable, either case it must be in .dynsym
    ResolveInfo *InputSymRI = InputSym->resolveInfo();
    LDSymbol *OutSym = InputSymRI->outSymbol();
    if (ThisConfig.codeGenType() != LinkerConfig::DynObj &&
        ((OutSym && OutSym->hasFragRef()) || InputSymRI->isCommon()))
      InputSymRI->setExportToDyn();
    return InputSym;
  };

#ifdef ELD_ENABLE_SYMBOL_VERSIONING
  // foo
  LDSymbol *SymbolWithoutVerName = nullptr;
  // foo@VerName
  LDSymbol *CanonicalSymbol = nullptr;
  if (IsDefaultVersionedSymbol) {
    SymbolWithoutVerName = AddSymbol(InputSymbolResolveInfo);
    if (!SymbolWithoutVerName)
      return nullptr;
    DynObjFile->addNonCanonicalSymbol(SymbolWithoutVerName);
  }
  if (OptVersionedName)
    InputSymbolResolveInfo.setName(Saver.save(OptVersionedName.value()));
  CanonicalSymbol = AddSymbol(InputSymbolResolveInfo);
  if (!CanonicalSymbol)
    return nullptr;
  if (SymbolWithoutVerName && CanonicalSymbol)
    VersionedSymbols.push_back({CanonicalSymbol, SymbolWithoutVerName});
  return CanonicalSymbol;
#else
  LDSymbol *Sym = AddSymbol(InputSymbolResolveInfo);
  return Sym;
#endif
}

void IRBuilder::addToCref(InputFile &Input, Resolver::Result PResult) {
  eld::RegisterTimer T("Add Symbols to Cross Reference Table",
                       "Symbol Resolution",
                       ThisConfig.options().printTimingStats());

  GeneralOptions &Options = ThisConfig.options();
  GeneralOptions::CrefTableType &Table = Options.crefTable();
  ResolveInfo *Info = PResult.Info;

  assert(nullptr != Info);

  std::vector<std::pair<const InputFile *, bool>> &RHS = Table[Info];
  if (PResult.Overriden == true && Info->isDefine()) {
    // we add symbol to cref in front if it is defined in this file
    RHS.insert(RHS.begin(),
               std::pair<const InputFile *, bool>(&Input, Info->isBitCode()));
  } else {
    // otherwise we simply add it to the end.
    RHS.push_back(
        std::pair<const InputFile *, bool>(&Input, Info->isBitCode()));
  }
}

void IRBuilder::addLinkerInternalLocalSymbol(InputFile *F, std::string Name,
                                             FragmentRef *FragRef,
                                             size_t Size) {
  LDSymbol *Sym = addSymbol<Force, Resolve>(
      F, Name, ResolveInfo::NoType, ResolveInfo::Define, ResolveInfo::Local,
      Size, 0, FragRef, ResolveInfo::Default, true);
  getModule().addSymbol(Sym->resolveInfo());
}

/// addRelocation - add a relocation entry
///
/// All symbols should be read and resolved before calling this function.
Relocation *IRBuilder::addRelocation(const Relocator *CurRelocator,
                                     ELFSection *CurSection,
                                     Relocation::Type Type, LDSymbol &PSym,
                                     uint32_t POffset,
                                     Relocation::Address CurAddend) {

  ResolveInfo *ResolveInfo = PSym.resolveInfo();

  if (!ResolveInfo)
    return nullptr;

  FragmentRef *FragRef = FragmentRef::null();
  if (CurSection->hasSectionData()) {
    Fragment *Frag = CurSection->getFragmentList().front();
    FragRef = make<FragmentRef>(*Frag, POffset);
  }
  Relocation *Relocation =
      Relocation::Create(Type, CurRelocator->getSize(Type), FragRef, CurAddend);

  Relocation->setSymInfo(PSym.resolveInfo());

  return Relocation;
}

Relocation *IRBuilder::addRelocation(const Relocator *CurRelocator,
                                     Fragment &CurFrag, Relocation::Type Type,
                                     LDSymbol &PSym, uint32_t POffset,
                                     Relocation::Address CurAddend) {

  ResolveInfo *ResolveInfo = PSym.resolveInfo();

  if (!ResolveInfo)
    return nullptr;

  if (!PSym.hasFragRef() && ResolveInfo::Section == ResolveInfo->type() &&
      ResolveInfo::Undefined == ResolveInfo->desc())
    return nullptr;

  FragmentRef *FragRef = make<FragmentRef>(CurFrag, POffset);

  Relocation *Relocation =
      Relocation::Create(Type, CurRelocator->getSize(Type), FragRef, CurAddend);

  Relocation->setSymInfo(PSym.resolveInfo());

  return Relocation;
}

Relocation *IRBuilder::createRelocation(const Relocator *CurRelocator,
                                        Fragment &CurFrag,
                                        Relocation::Type Type, LDSymbol &PSym,
                                        uint32_t POffset,
                                        Relocation::Address CurAddend) {

  ResolveInfo *ResolveInfo = PSym.resolveInfo();

  if (!ResolveInfo)
    return nullptr;

  if (!PSym.hasFragRef() && ResolveInfo::Section == ResolveInfo->type() &&
      ResolveInfo::Undefined == ResolveInfo->desc())
    return nullptr;

  FragmentRef *FragRef = make<FragmentRef>(CurFrag, POffset);

  Relocation *Relocation =
      make<eld::Relocation>(CurRelocator, Type, FragRef, CurAddend);

  Relocation->setSymInfo(PSym.resolveInfo());

  return Relocation;
}

/// addSymbol - define an output symbol and override it immediately
template <>
LDSymbol *IRBuilder::addSymbol<IRBuilder::Force, IRBuilder::Unresolve>(
    InputFile *Input, std::string SymbolName, ResolveInfo::Type Type,
    ResolveInfo::Desc Desc, ResolveInfo::Binding Binding,
    ResolveInfo::SizeType Size, LDSymbol::ValueType Value,
    FragmentRef *CurFragmentRef, ResolveInfo::Visibility Visibility,
    bool IsPostLtoPhase, bool IsBitCode, bool IsPatchable) {
  ResolveInfo *Info = ThisModule.getNamePool().findInfo(SymbolName);
  LDSymbol *OutputSym = nullptr;
  if (nullptr == Info) {
    // the symbol is not in the pool, create a new one.
    // create a ResolveInfo
    Resolver::Result Result;
    bool S = ThisModule.getNamePool().insertSymbol(
        Input, SymbolName, false, Type, Desc, Binding, Size, Value, Visibility,
        nullptr, Result, IsPostLtoPhase, IsBitCode, 0, IsPatchable,
        ThisModule.getPrinter());
    if (!S)
      return nullptr;
    assert(!Result.Existent);

    // create a output LDSymbol
    OutputSym = makeLDSymbol(Result.Info);
    Result.Info->setOutSymbol(OutputSym);
  } else {
    // the symbol is already in the pool, override it
    ResolveInfo OldInfo;
    if (ThisConfig.showLinkerScriptWarnings() && !Info->isUndef())
      ThisConfig.raise(Diag::warning_override_symbol)
          << SymbolName << Input->getInput()->decoratedPath()
          << Info->getResolvedPath();
    OldInfo.override(*Info);

    Info->setRegular();
    Info->setType(Type);
    Info->setDesc(Desc);
    Info->setBinding(Binding);
    Info->setVisibility(Visibility);
    Info->setIsSymbol(true);
    Info->setSize(Size);

    // create a output LDSymbol
    OutputSym = makeLDSymbol(Info);
    Info->setOutSymbol(OutputSym);
  }

  if (nullptr != OutputSym) {
    OutputSym->setFragmentRef(CurFragmentRef);
    OutputSym->setValue(Value, false);
  }

  if (ThisModule.getLayoutInfo() &&
      ThisModule.getLayoutInfo()->showSymbolResolution()) {
    SymbolResolutionInfo &SRI = ThisModule.getNamePool().getSRI();
    SRI.recordSymbolInfo(OutputSym,
                         SymbolInfo{Input, Size, Binding, Type, Visibility,
                                    Desc, /*isBitcode=*/false});
  }

  return OutputSym;
}

/// addSymbol - define an output symbol and override it immediately
template <>
LDSymbol *IRBuilder::addSymbol<IRBuilder::AsReferred, IRBuilder::Unresolve>(
    InputFile *Input, std::string SymbolName, ResolveInfo::Type Type,
    ResolveInfo::Desc Desc, ResolveInfo::Binding Binding,
    ResolveInfo::SizeType Size, LDSymbol::ValueType Value,
    FragmentRef *CurFragmentRef, ResolveInfo::Visibility Visibility,
    bool IsPostLtoPhase, bool IsBitCode, bool IsPatchable) {
  assert(!IsPatchable);
  ResolveInfo *Info = ThisModule.getNamePool().findInfo(SymbolName);

  if (nullptr == Info || !(Info->isUndef() || Info->isDyn())) {
    // only undefined symbol and dynamic symbol can make a reference.
    return nullptr;
  }

  // the symbol is already in the pool, override it
  ResolveInfo OldInfo;
  OldInfo.override(*Info);

  Info->setRegular();
  Info->setType(Type);
  Info->setDesc(Desc);
  Info->setBinding(Binding);
  Info->setVisibility(Visibility);
  Info->setIsSymbol(true);
  Info->setSize(Size);
  if (!Info->resolvedOrigin())
    Info->setResolvedOrigin(Input);

  LDSymbol *OutputSym = Info->outSymbol();
  if (nullptr != OutputSym) {
    OutputSym->setFragmentRef(CurFragmentRef);
    OutputSym->setValue(Value, false);
  } else {
    // create a output LDSymbol
    OutputSym = makeLDSymbol(Info);
    Info->setOutSymbol(OutputSym);
  }

  return OutputSym;
}

/// addSymbol - define an output symbol and resolve it
/// immediately
template <>
LDSymbol *IRBuilder::addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
    InputFile *Input, std::string SymbolName, ResolveInfo::Type Type,
    ResolveInfo::Desc Desc, ResolveInfo::Binding Binding,
    ResolveInfo::SizeType Size, LDSymbol::ValueType Value,
    FragmentRef *CurFragmentRef, ResolveInfo::Visibility Visibility,
    bool IsPostLtoPhase, bool IsBitCode, bool IsPatchable) {
  // Result is <info, existent, override>
  Resolver::Result Result;
  ResolveInfo OldInfo;

  bool S = ThisModule.getNamePool().insertSymbol(
      Input, SymbolName, false, Type, Desc, Binding, Size, Value, Visibility,
      &OldInfo, Result, IsPostLtoPhase, IsBitCode, 0, IsPatchable,
      ThisModule.getPrinter());

  if (!S)
    return nullptr;

  LDSymbol *OutputSym = Result.Info->outSymbol();
  bool HasOutputSym = (nullptr != OutputSym);

  if (!Result.Existent || !HasOutputSym) {
    OutputSym = makeLDSymbol(Result.Info);
    Result.Info->setOutSymbol(OutputSym);
  }

  if (Result.Overriden || !HasOutputSym) {
    OutputSym->setFragmentRef(CurFragmentRef);
    OutputSym->setValue(Value, false);
  }

  return OutputSym;
}

/// defineSymbol - define an output symbol and resolve it immediately.
template <>
LDSymbol *IRBuilder::addSymbol<IRBuilder::AsReferred, IRBuilder::Resolve>(
    InputFile *Input, std::string SymbolName, ResolveInfo::Type Type,
    ResolveInfo::Desc Desc, ResolveInfo::Binding Binding,
    ResolveInfo::SizeType Size, LDSymbol::ValueType Value,
    FragmentRef *CurFragmentRef, ResolveInfo::Visibility Visibility,
    bool IsPostLtoPhase, bool IsBitCode, bool IsPatchable) {
  ResolveInfo *Info = ThisModule.getNamePool().findInfo(SymbolName);

  if (nullptr == Info || !(Info->isUndef() || Info->isDyn())) {
    // only undefined symbol and dynamic symbol can make a reference.
    return nullptr;
  }

  return addSymbol<Force, Resolve>(Input, SymbolName, Type, Desc, Binding, Size,
                                   Value, CurFragmentRef, Visibility,
                                   IsPostLtoPhase, IsBitCode, IsPatchable);
}

void IRBuilder::normalizeSymbols() {
  std::unordered_map<ResolveInfo *, ResolveInfo *> RIReplacementMap;
  for (const auto &P : VersionedSymbols) {
    // P.first is the canonical version symbol, P.second is the non-canonical
    // version symbol.
    LDSymbol *CanonicalSym = P.first;
    LDSymbol *NonCanonicalSym = P.second;
    assert(CanonicalSym && NonCanonicalSym && "must not be null!");

    if (CanonicalSym->resolveInfo()->outSymbol() == CanonicalSym &&
        NonCanonicalSym->resolveInfo()->outSymbol() == NonCanonicalSym) {
      ResolveInfo *CanonicalRI = P.first->resolveInfo();
      ResolveInfo *NonCanonicalRI = P.second->resolveInfo();
      if (NonCanonicalRI->exportToDyn())
        CanonicalRI->setExportToDyn();
      RIReplacementMap[NonCanonicalRI] = CanonicalRI;
    }
  }

  const auto &ObjectFiles = ThisModule.getObjectList();
  const auto &DynObjectFiles = ThisModule.getDynLibraryList();

  std::vector<InputFile *> AllInputs;
  AllInputs.insert(AllInputs.end(), ObjectFiles.begin(), ObjectFiles.end());
  AllInputs.insert(AllInputs.end(), DynObjectFiles.begin(),
                   DynObjectFiles.end());

  for (InputFile *Input : AllInputs) {
    ObjectFile *ObjFile = llvm::dyn_cast<ObjectFile>(Input);
    if (ObjFile) {
      for (LDSymbol *Sym : ObjFile->getSymbols()) {
        ResolveInfo *RI = Sym->resolveInfo();
        auto it = RIReplacementMap.find(RI);

        if (it != RIReplacementMap.end()) {
          Sym->setResolveInfo(*(it->second));
        }
      }
    }

    ELFDynObjectFile *DynObjFile = llvm::dyn_cast<ELFDynObjectFile>(Input);
    if (!DynObjFile)
      continue;

    const auto &NonCanonicalSymbols = DynObjFile->getNonCanonicalSymbols();
    for (LDSymbol *NonCanonicalSym : NonCanonicalSymbols) {
      ResolveInfo *NonCanonicalRI = NonCanonicalSym->resolveInfo();
      auto it = RIReplacementMap.find(NonCanonicalRI);

      if (it != RIReplacementMap.end()) {
        NonCanonicalSym->setResolveInfo(*(it->second));
      }
    }
  }
}