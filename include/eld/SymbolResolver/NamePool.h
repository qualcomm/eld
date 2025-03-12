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

class LayoutPrinter;
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
  explicit NamePool(eld::LinkerConfig &Config, PluginManager &pm);

  ~NamePool();

  ResolveInfo createInputSymbolRI(const std::string &symName, InputFile &IF,
                                  bool isDyn, ResolveInfo::Type symType,
                                  ResolveInfo::Desc symDesc,
                                  ResolveInfo::Binding symBinding,
                                  ResolveInfo::SizeType symSize,
                                  ResolveInfo::Visibility symVisibility,
                                  LDSymbol::ValueType symValue,
                                  bool isPatchable) const;

  // -----  modifiers  ----- //
  /// createSymbol - create a symbol but do not insert into the pool.
  ResolveInfo *createSymbol(InputFile *pInput, std::string pName, bool pIsDyn,
                            ResolveInfo::Type pType, ResolveInfo::Desc pDesc,
                            ResolveInfo::Binding pBinding,
                            ResolveInfo::SizeType pSize,
                            ResolveInfo::Visibility pVisibility,
                            bool isPostLTOPhase);

  ResolveInfo *insertLocalSymbol(ResolveInfo inputSymRI, const LDSymbol &sym);

  bool insertNonLocalSymbol(ResolveInfo inputSymRI, const LDSymbol &sym,
                            bool isPostLTOPhase, Resolver::Result &pResult);

  /// insertSymbol - insert a symbol and resolve the symbol immediately
  bool insertSymbol(InputFile *pInput, std::string pName, bool pIsDyn,
                    ResolveInfo::Type pType, ResolveInfo::Desc pDesc,
                    ResolveInfo::Binding pBinding, ResolveInfo::SizeType pSize,
                    LDSymbol::ValueType pValue,
                    ResolveInfo::Visibility pVisibility, ResolveInfo *pOldInfo,
                    Resolver::Result &pResult, bool isLTOPhase, bool isBitCode,
                    unsigned int pSymIdx, bool isPatchable,
                    DiagnosticPrinter *Printer);

  LDSymbol *createPluginSymbol(InputFile *pInput, std::string pName,
                               Fragment *pFragment, uint64_t Val,
                               LayoutPrinter *LP);

  size_t getNumGlobalSize() const { return m_Globals.size(); }

  /// findSymbol - find the resolved output LDSymbol
  const LDSymbol *findSymbol(std::string pName) const;
  LDSymbol *findSymbol(std::string pName);

  /// findInfo - find the resolved ResolveInfo
  const ResolveInfo *findInfo(std::string pName) const;
  ResolveInfo *findInfo(std::string pName);

  // Get Local symbols.
  std::vector<ResolveInfo *> &getLocals() { return m_Locals; }

  // Get Global symbols.
  llvm::StringMap<ResolveInfo *> &getGlobals() { return m_Globals; }

  void setupNullSymbol();

  std::string getDecoratedName(const ResolveInfo *R, bool DoDemangle) const;

  std::string getDecoratedName(const LDSymbol &sym,
                               const ResolveInfo &RI) const;

  bool getSymbolTracingRequested() const;

  /// --------------------- Symbol References and checks ----------------
  bool canSymbolsBeResolved(const ResolveInfo *, const ResolveInfo *) const;
  bool checkTLSTypes(const ResolveInfo *, const ResolveInfo *) const;

  SymbolResolutionInfo &getSRI() { return m_SRI; }

  void addSharedLibSymbol(LDSymbol *sym) {
    ASSERT(sym->resolveInfo(), "symbol must have a resolveInfo!");
    m_SharedLibsSymbols[sym->resolveInfo()] = sym;
  }

  LDSymbol *getSharedLibSymbol(const ResolveInfo *RI) {
    auto iter = m_SharedLibsSymbols.find(RI);
    if (iter != m_SharedLibsSymbols.end())
      return iter->second;
    return nullptr;
  }

private:
  eld::LinkerConfig &m_Config;
  Resolver *m_pResolver;
  llvm::StringMap<ResolveInfo *> m_Globals;
  std::vector<ResolveInfo *> m_Locals;
  bool m_SymbolTracingRequested;
  SymbolResolutionInfo m_SRI;
  std::map<const ResolveInfo *, LDSymbol *> m_SharedLibsSymbols;
  PluginManager &PM;
};

} // namespace eld

#endif
