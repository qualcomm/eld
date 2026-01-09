//===- NamePool.h----------------------------------------------------------===//
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

#ifndef ELD_SYMBOLRESOLVER_NAMEPOOL_H
#define ELD_SYMBOLRESOLVER_NAMEPOOL_H

#include "eld/Config/Config.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Script/Expression.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "eld/SymbolResolver/Resolver.h"
#include "eld/SymbolResolver/SymbolInfo.h"
#include "eld/SymbolResolver/SymbolResolutionInfo.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include <map>
#include <utility>
#include <vector>

namespace eld {

class LayoutInfo;
class LDSymbol;
class Module;
class PluginManager;
class StringTable;
class Input;

/** \class NamePool
 *  \brief Store symbol and search symbol by name. Can help symbol resolution.
 *
 */
class NamePool {
public:
  explicit NamePool(eld::LinkerConfig &Config, PluginManager &Pm);

  ~NamePool();

  ResolveInfo createInputSymbolRI(const std::string &SymName, InputFile &IF,
                                  bool IsDyn, ResolveInfo::Type SymType,
                                  ResolveInfo::Desc SymDesc,
                                  ResolveInfo::Binding SymBinding,
                                  ResolveInfo::SizeType SymSize,
                                  ResolveInfo::Visibility SymVisibility,
                                  LDSymbol::ValueType SymValue,
                                  bool IsPatchable) const;

  // -----  modifiers  ----- //
  /// createSymbol - create a symbol but do not insert into the pool.
  ResolveInfo *createSymbol(InputFile *Input, std::string SymbolName,
                            bool IsSymbolInDynamicLibrary,
                            ResolveInfo::Type Type, ResolveInfo::Desc Desc,
                            ResolveInfo::Binding Binding,
                            ResolveInfo::SizeType Size,
                            ResolveInfo::Visibility Visibility,
                            bool IsPostLtoPhase);

  ResolveInfo *insertLocalSymbol(ResolveInfo InputSymbolResolveInfo,
                                 const LDSymbol &Sym);

  bool insertNonLocalSymbol(ResolveInfo InputSymbolResolveInfo,
                            const LDSymbol &Sym, bool IsPostLtoPhase,
                            Resolver::Result &PResult);

  /// insertSymbol - insert a symbol and resolve the symbol immediately
  bool insertSymbol(InputFile *Input, std::string SymbolName,
                    bool IsSymbolInDynamicLibrary, ResolveInfo::Type Type,
                    ResolveInfo::Desc Desc, ResolveInfo::Binding Binding,
                    ResolveInfo::SizeType Size, LDSymbol::ValueType Value,
                    ResolveInfo::Visibility Visibility,
                    ResolveInfo *OldSymbolInfo, Resolver::Result &PResult,
                    bool IsLtoPhase, bool IsBitCode, unsigned int PSymIdx,
                    bool IsPatchable, DiagnosticPrinter *Printer);

  LDSymbol *createPluginSymbol(InputFile *Input, std::string SymbolName,
                               Fragment *CurFragment, uint64_t Val,
                               LayoutInfo *layoutInfo);

  size_t getNumGlobalSize() const { return GlobalSymbols.size(); }

  /// findSymbol - find the resolved output LDSymbol
  const LDSymbol *findSymbol(std::string SymbolName) const;
  LDSymbol *findSymbol(std::string SymbolName);

  /// findInfo - find the resolved ResolveInfo
  const ResolveInfo *findInfo(std::string SymbolName) const;
  ResolveInfo *findInfo(std::string SymbolName);

  // Get Local symbols.
  std::vector<ResolveInfo *> &getLocals() { return LocalSymbols; }

  // Get Global symbols.
  llvm::StringMap<ResolveInfo *> &getGlobals() { return GlobalSymbols; }

  void setupNullSymbol();

  std::string getDecoratedName(const ResolveInfo *R, bool DoDemangle) const;

  std::string getDecoratedName(const LDSymbol &Sym,
                               const ResolveInfo &RI) const;

  bool getSymbolTracingRequested() const;

  /// --------------------- Symbol References and checks ----------------
  bool canSymbolsBeResolved(const ResolveInfo *, const ResolveInfo *) const;
  bool checkTLSTypes(const ResolveInfo *, const ResolveInfo *) const;

  SymbolResolutionInfo &getSRI() { return SymbolResInfo; }

  void addSharedLibSymbol(LDSymbol *Sym) {
    ASSERT(Sym->resolveInfo(), "symbol must have a resolveInfo!");
    SharedLibsSymbols[Sym->resolveInfo()] = Sym;
  }

#ifdef ELD_ENABLE_SYMBOL_VERSIONING
  void addSharedLibSymbol(LDSymbol *Sym, ResolveInfo *RI) {
    SharedLibsSymbols[RI] = Sym;
  }
#endif

  LDSymbol *getSharedLibSymbol(const ResolveInfo *RI) {
    auto Iter = SharedLibsSymbols.find(RI);
    if (Iter != SharedLibsSymbols.end())
      return Iter->second;
    return nullptr;
  }

  // Helper function to add an undefined symbol to the NamePool
  void
  addUndefinedELFSymbol(InputFile *I, std::string SymbolName,
                        ResolveInfo::Visibility Vis = ResolveInfo::Default);

private:
  eld::LinkerConfig &ThisConfig;
  Resolver *SymbolResolver;
  llvm::StringMap<ResolveInfo *> GlobalSymbols;
  std::vector<ResolveInfo *> LocalSymbols;
  bool IsSymbolTracingRequested;
  SymbolResolutionInfo SymbolResInfo;
  std::map<const ResolveInfo *, LDSymbol *> SharedLibsSymbols;
  PluginManager &PM;
};

} // namespace eld

#endif
