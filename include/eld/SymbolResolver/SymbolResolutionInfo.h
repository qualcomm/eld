//===- SymbolResolutionInfo.h----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SYMBOLRESOLVER_SYMBOLRESOLUTION_H
#define ELD_SYMBOLRESOLVER_SYMBOLRESOLUTION_H
#include "eld/SymbolResolver/SymbolInfo.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/StringRef.h"
#include <optional>

namespace eld {
class Assignment;
class GeneralOptions;
class LDSymbol;
class LinkerConfig;
class LinkerScript;
class NamePool;
class Plugin;

/// Stores information required for reporting symbol resolution.
class SymbolResolutionInfo {
public:
  SymbolResolutionInfo() {}
  using CandidatesType = std::vector<const LDSymbol *>;
  using CandidatesTableType = llvm::StringMap<CandidatesType>;
  using SymbolInfoMapType = llvm::MapVector<const LDSymbol *, SymbolInfo>;

  std::string getSymbolInfoAsString(const LDSymbol *sym,
                                    const GeneralOptions &options);

  /// Setup symbol resolution candidates information. This information is
  /// required for creating symbol resolution report. This function does two
  /// things:
  /// - Generates candidates table. The candidates table contains a map of
  ///   symbol names to the vector of symbols with that name.
  /// - Creates a map of bitcode symbols to the corresponding LTO-object
  /// symbols.
  /// \note This function must only be called once.
  void setupCandidatesInfo(NamePool &NP, const LinkerScript &script);

  /// Returns a vector of symbol resolution candidates for the specified symbol
  /// name.
  const CandidatesType &getCandidates(llvm::StringRef);

  std::optional<SymbolInfo> getSymbolInfo(const LDSymbol *sym) {
    auto iter = m_SymbolInfoMap.find(sym);
    if (iter != m_SymbolInfoMap.end())
      return iter->second;
    return {};
  }

  void recordSymbolInfo(const LDSymbol *sym, SymbolInfo symInfo);

  const LDSymbol *getCorrespondingLTOObjectSymIfAny(const LDSymbol *S) const;

  void recordPluginSymbol(const LDSymbol *sym, const Plugin *plugin) {
    m_SymbolToPluginMap[sym] = plugin;
  }

private:
  CandidatesTableType m_Candidates;
  SymbolInfoMapType m_SymbolInfoMap;
  std::vector<const LDSymbol *> m_LTOObjectSymbols;
  std::unordered_map<const LDSymbol *, const LDSymbol *>
      m_BitcodeSymToLTOObjectSymMap;
  std::unordered_map<const LDSymbol *, const Plugin *> m_SymbolToPluginMap;
};

} // namespace eld
#endif