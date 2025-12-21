//===- IRBuilder.h---------------------------------------------------------===//
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
#ifndef ELD_SYMBOLRESOLVER_IRBUILDER_H
#define ELD_SYMBOLRESOLVER_IRBUILDER_H

#include "eld/Fragment/FillFragment.h"
#include "eld/Fragment/Fragment.h"
#include "eld/Fragment/FragmentRef.h"
#include "eld/Fragment/RegionFragment.h"
#include "eld/Input/Input.h"
#include "eld/Input/InputBuilder.h"
#include "eld/Input/InputFile.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Readers/Relocation.h"
#include "eld/Support/Path.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/SymbolResolver/Resolver.h"
#include "llvm/ADT/DenseMap.h"

namespace eld {

class Module;
class LinkerConfig;
class InputTree;
class InputFile;
class LayoutInfo;

class IRBuilder {
public:
  enum ObjectFormat { ELF, MachO, COFF };

  enum SymbolDefinePolicy { Force, AsReferred };

  enum SymbolResolvePolicy { Unresolve, Resolve };

public:
  IRBuilder(Module &CurModule, LinkerConfig &Config);

  ~IRBuilder();

  InputBuilder &getInputBuilder() { return ThisInputBuilder; }

  Module &getModule() { return ThisModule; }

  LinkerConfig &getConfig() { return ThisConfig; }

  void requestGarbageCollection();

  bool shouldRunGarbageCollection() const { return IsGarbageCollected; }

  /// Create an empty LDSymbol. The purpose of this wrapper is to automatically
  /// set the initial state (isIgnore), depending on the GC options.
  LDSymbol *makeLDSymbol(ResolveInfo *);

  LDSymbol *addSymbol(InputFile &Input, const std::string &SymbolName,
                      ResolveInfo::Type Type, ResolveInfo::Desc Desc,
                      ResolveInfo::Binding Bind, ResolveInfo::SizeType Size,
                      LDSymbol::ValueType Value = 0x0,
                      ELFSection *CurSection = nullptr,
                      ResolveInfo::Visibility Vis = ResolveInfo::Default,
                      bool IsPostLtoPhase = true, uint32_t Shndx = 0,
                      uint32_t Idx = 0, bool IsPatchable = false);

  template <SymbolDefinePolicy POLICY, SymbolResolvePolicy RESOLVE>
  LDSymbol *
  addSymbol(InputFile *Input, std::string SymbolName, ResolveInfo::Type Type,
            ResolveInfo::Desc Desc, ResolveInfo::Binding Binding,
            ResolveInfo::SizeType Size = 0, LDSymbol::ValueType Value = 0x0,
            FragmentRef *CurFragmentRef = FragmentRef::null(),
            ResolveInfo::Visibility Visibility = ResolveInfo::Default,
            bool IsPostLtoPhase = true, bool IsBitCode = false,
            bool IsPatchable = false);

  static Relocation *addRelocation(const Relocator *CurRelocator,
                                   ELFSection *CurSection,
                                   Relocation::Type Type, LDSymbol &PSym,
                                   uint32_t POffset,
                                   Relocation::Address CurAddend = 0);

  static Relocation *addRelocation(const Relocator *CurRelocator,
                                   Fragment &CurFrag, Relocation::Type Type,
                                   LDSymbol &PSym, uint32_t POffset,
                                   Relocation::Address CurAddend = 0);

  Relocation *createRelocation(const Relocator *CurRelocator, Fragment &CurFrag,
                               Relocation::Type Type, LDSymbol &PSym,
                               uint32_t POffset, Relocation::Address CurAddend);

#ifdef ELD_ENABLE_SYMBOL_VERSIONING
  void normalizeSymbols();
#endif

private:
  LDSymbol *
  addSymbolFromObject(InputFile &Input, const std::string &SymbolName,
                      ResolveInfo::Type Type, ResolveInfo::Desc Desc,
                      ResolveInfo::Binding Binding, ResolveInfo::SizeType Size,
                      LDSymbol::ValueType Value, FragmentRef *CurFragmentRef,
                      ResolveInfo::Visibility Visibility, uint32_t Shndx,
                      bool IsPostLtoPhase, uint32_t Idx, bool IsPatchable);

  LDSymbol *addSymbolFromDynObj(InputFile &Input, const std::string &SymbolName,
                                ResolveInfo::Type Type, ResolveInfo::Desc Desc,
                                ResolveInfo::Binding Binding,
                                ResolveInfo::SizeType Size,
                                LDSymbol::ValueType Value,
                                ResolveInfo::Visibility Visibility,
                                uint32_t Shndx, bool IsPostLtoPhase,
                                uint32_t SymIdx);

public:
  void addToCref(InputFile &Input, Resolver::Result PResult);

  Fragment *findMergeStr(uint64_t Hash) const;

  void addLinkerInternalLocalSymbol(InputFile *F, std::string Name,
                                    FragmentRef *FragRef, size_t Size);

private:
  Module &ThisModule;
  LinkerConfig &ThisConfig;
  bool IsGarbageCollected;
  InputBuilder ThisInputBuilder;

#ifdef ELD_ENABLE_SYMBOL_VERSIONING
  // 0th element -> Canonical version symbol, 1st element -> Non-canonical
  // version symbol.
  std::vector<std::pair<LDSymbol *, LDSymbol *>> VersionedSymbols;
#endif
};

template <>
LDSymbol *IRBuilder::addSymbol<IRBuilder::Force, IRBuilder::Unresolve>(
    InputFile *Input, std::string SymbolName, ResolveInfo::Type Type,
    ResolveInfo::Desc Desc, ResolveInfo::Binding Binding,
    ResolveInfo::SizeType Size, LDSymbol::ValueType Value,
    FragmentRef *CurFragmentRef, ResolveInfo::Visibility Visibility,
    bool IsPostLtoPhase, bool IsBitCode, bool IsPatchable);

template <>
LDSymbol *IRBuilder::addSymbol<IRBuilder::AsReferred, IRBuilder::Unresolve>(
    InputFile *Input, std::string SymbolName, ResolveInfo::Type Type,
    ResolveInfo::Desc Desc, ResolveInfo::Binding Binding,
    ResolveInfo::SizeType Size, LDSymbol::ValueType Value,
    FragmentRef *CurFragmentRef, ResolveInfo::Visibility Visibility,
    bool IsPostLtoPhase, bool IsBitCode, bool IsPatchable);

template <>
LDSymbol *IRBuilder::addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
    InputFile *Input, std::string SymbolName, ResolveInfo::Type Type,
    ResolveInfo::Desc Desc, ResolveInfo::Binding Binding,
    ResolveInfo::SizeType Size, LDSymbol::ValueType Value,
    FragmentRef *CurFragmentRef, ResolveInfo::Visibility Visibility,
    bool IsPostLtoPhase, bool IsBitCode, bool IsPatchable);

template <>
LDSymbol *IRBuilder::addSymbol<IRBuilder::AsReferred, IRBuilder::Resolve>(
    InputFile *Input, std::string SymbolName, ResolveInfo::Type Type,
    ResolveInfo::Desc Desc, ResolveInfo::Binding Binding,
    ResolveInfo::SizeType Size, LDSymbol::ValueType Value,
    FragmentRef *CurFragmentRef, ResolveInfo::Visibility Visibility,
    bool IsPostLtoPhase, bool IsBitCode, bool IsPatchable);

} // end of namespace eld

#endif
