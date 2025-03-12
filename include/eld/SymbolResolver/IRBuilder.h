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
class LayoutPrinter;

class IRBuilder {
public:
  enum ObjectFormat { ELF, MachO, COFF };

  enum SymbolDefinePolicy { Force, AsReferred };

  enum SymbolResolvePolicy { Unresolve, Resolve };

public:
  IRBuilder(Module &pModule, LinkerConfig &pConfig);

  ~IRBuilder();

  InputBuilder &getInputBuilder() { return m_InputBuilder; }

  Module &getModule() { return m_Module; }

  LinkerConfig &getConfig() { return m_Config; }

  void requestGarbageCollection();

  bool shouldRunGarbageCollection() const { return m_isGC; }

  /// Create an empty LDSymbol. The purpose of this wrapper is to automatically
  /// set the initial state (isIgnore), depending on the GC options.
  LDSymbol *makeLDSymbol(ResolveInfo *);

  LDSymbol *AddSymbol(InputFile &pInput, const std::string &pName,
                      ResolveInfo::Type pType, ResolveInfo::Desc pDesc,
                      ResolveInfo::Binding pBind, ResolveInfo::SizeType pSize,
                      LDSymbol::ValueType pValue = 0x0,
                      ELFSection *pSection = nullptr,
                      ResolveInfo::Visibility pVis = ResolveInfo::Default,
                      bool isPostLTOPhase = true, uint32_t shndx = 0,
                      uint32_t idx = 0, bool isPatchable = false);

  template <SymbolDefinePolicy POLICY, SymbolResolvePolicy RESOLVE>
  LDSymbol *
  AddSymbol(InputFile *pInput, std::string pName, ResolveInfo::Type pType,
            ResolveInfo::Desc pDesc, ResolveInfo::Binding pBinding,
            ResolveInfo::SizeType pSize = 0, LDSymbol::ValueType pValue = 0x0,
            FragmentRef *pFragmentRef = FragmentRef::Null(),
            ResolveInfo::Visibility pVisibility = ResolveInfo::Default,
            bool isPostLTOPhase = true, bool isBitCode = false,
            bool isPatchable = false);

  static Relocation *AddRelocation(const Relocator *pRelocator,
                                   ELFSection *pSection, Relocation::Type pType,
                                   LDSymbol &pSym, uint32_t pOffset,
                                   Relocation::Address pAddend = 0);

  static Relocation *AddRelocation(const Relocator *pRelocator, Fragment &pFrag,
                                   Relocation::Type pType, LDSymbol &pSym,
                                   uint32_t pOffset,
                                   Relocation::Address pAddend = 0);

  Relocation *CreateRelocation(const Relocator *pRelocator, Fragment &pFrag,
                               Relocation::Type pType, LDSymbol &pSym,
                               uint32_t pOffset, Relocation::Address pAddend);

private:
  LDSymbol *addSymbolFromObject(
      InputFile &pInput, const std::string &pName, ResolveInfo::Type pType,
      ResolveInfo::Desc pDesc, ResolveInfo::Binding pBinding,
      ResolveInfo::SizeType pSize, LDSymbol::ValueType pValue,
      FragmentRef *pFragmentRef, ResolveInfo::Visibility pVisibility,
      uint32_t shndx, bool isPostLTOPhase, uint32_t Idx, bool isPatchable);

  LDSymbol *addSymbolFromDynObj(
      InputFile &pInput, const std::string &pName, ResolveInfo::Type pType,
      ResolveInfo::Desc pDesc, ResolveInfo::Binding pBinding,
      ResolveInfo::SizeType pSize, LDSymbol::ValueType pValue,
      ResolveInfo::Visibility pVisibility, uint32_t shndx, bool isPostLTOPhase);

public:
  void addToCref(InputFile &pInput, Resolver::Result pResult);

  Fragment *findMergeStr(uint64_t hash) const;

  void addLinkerInternalLocalSymbol(InputFile *F, std::string Name,
                                    FragmentRef *fragRef, size_t Size);

private:
  Module &m_Module;
  LinkerConfig &m_Config;
  bool m_isGC;
  InputBuilder m_InputBuilder;
};

template <>
LDSymbol *IRBuilder::AddSymbol<IRBuilder::Force, IRBuilder::Unresolve>(
    InputFile *pInput, std::string pName, ResolveInfo::Type pType,
    ResolveInfo::Desc pDesc, ResolveInfo::Binding pBinding,
    ResolveInfo::SizeType pSize, LDSymbol::ValueType pValue,
    FragmentRef *pFragmentRef, ResolveInfo::Visibility pVisibility,
    bool isPostLTOPhase, bool isBitCode, bool isPatchable);

template <>
LDSymbol *IRBuilder::AddSymbol<IRBuilder::AsReferred, IRBuilder::Unresolve>(
    InputFile *pInput, std::string pName, ResolveInfo::Type pType,
    ResolveInfo::Desc pDesc, ResolveInfo::Binding pBinding,
    ResolveInfo::SizeType pSize, LDSymbol::ValueType pValue,
    FragmentRef *pFragmentRef, ResolveInfo::Visibility pVisibility,
    bool isPostLTOPhase, bool isBitCode, bool isPatchable);

template <>
LDSymbol *IRBuilder::AddSymbol<IRBuilder::Force, IRBuilder::Resolve>(
    InputFile *pInput, std::string pName, ResolveInfo::Type pType,
    ResolveInfo::Desc pDesc, ResolveInfo::Binding pBinding,
    ResolveInfo::SizeType pSize, LDSymbol::ValueType pValue,
    FragmentRef *pFragmentRef, ResolveInfo::Visibility pVisibility,
    bool isPostLTOPhase, bool isBitCode, bool isPatchable);

template <>
LDSymbol *IRBuilder::AddSymbol<IRBuilder::AsReferred, IRBuilder::Resolve>(
    InputFile *pInput, std::string pName, ResolveInfo::Type pType,
    ResolveInfo::Desc pDesc, ResolveInfo::Binding pBinding,
    ResolveInfo::SizeType pSize, LDSymbol::ValueType pValue,
    FragmentRef *pFragmentRef, ResolveInfo::Visibility pVisibility,
    bool isPostLTOPhase, bool isBitCode, bool isPatchable);

} // end of namespace eld

#endif
