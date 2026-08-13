//===- SymbolResolutionInfo.h----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SYMBOLRESOLVER_SYMBOLRESOLUTIONINFO_H
#define ELD_SYMBOLRESOLVER_SYMBOLRESOLUTIONINFO_H
#include "eld/SymbolResolver/SymbolInfo.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/JSON.h"
#include <optional>

namespace eld {
class Assignment;
class GeneralOptions;
class LDSymbol;
class LinkerConfig;
class LinkerScript;
class Module;
class NamePool;
class Plugin;

/// Stores information required for reporting symbol resolution.
class SymbolResolutionInfo {
public:
  SymbolResolutionInfo() {}
  using CandidatesType = std::vector<const LDSymbol *>;
  using CandidatesTableType = llvm::StringMap<CandidatesType>;
  using SymbolInfoMapType = llvm::MapVector<const LDSymbol *, SymbolInfo>;

  /// Returns the (decorated) section name for a defined symbol, or an empty
  /// string when the symbol has no associated section (Undef/Abs/unknown
  /// bitcode section).
  std::string getSymbolSectionName(const LDSymbol *Sym,
                                   const SymbolInfo &SymInfo,
                                   const GeneralOptions &Options) const;

  /// Returns the plugin that provided the symbol, or nullptr for
  /// non-plugin symbols.
  const Plugin *getSymbolPlugin(const LDSymbol *Sym) const {
    auto Iter = SymbolToPluginMap.find(Sym);
    if (Iter != SymbolToPluginMap.end())
      return Iter->second;
    return nullptr;
  }

  /// Setup symbol resolution candidates information. This information is
  /// required for creating symbol resolution report. This function does two
  /// things:
  /// - Generates candidates table. The candidates table contains a map of
  ///   symbol names to the vector of symbols with that name.
  /// - Creates a map of bitcode symbols to the corresponding LTO-object
  /// symbols.
  /// \note This function must only be called once.
  void setupCandidatesInfo(NamePool &NP, const LinkerScript &Script);

  /// Returns a vector of symbol resolution candidates for the specified symbol
  /// name.
  const CandidatesType &getCandidates(llvm::StringRef);

  std::optional<SymbolInfo> getSymbolInfo(const LDSymbol *Sym) {
    auto *Iter = SymbolInfoMap.find(Sym);
    if (Iter != SymbolInfoMap.end())
      return Iter->second;
    return {};
  }

  void recordSymbolInfo(const LDSymbol *Sym, SymbolInfo SymInfo);

  const LDSymbol *getCorrespondingLTOObjectSymIfAny(const LDSymbol *S) const;

  void recordPluginSymbol(const LDSymbol *Sym, const Plugin *Plugin) {
    SymbolToPluginMap[Sym] = Plugin;
  }

  /// Emits the symbol resolution report as standalone JSON to \p Filename.
  /// Returns false (and raises a diagnostic) if the file cannot be written.
  bool emitSymbolResolutionReport(Module &CurModule, llvm::StringRef Filename);

private:
  /// Builds the JSON object describing a single symbol resolution candidate.
  llvm::json::Object buildCandidateObject(const LDSymbol *Candidate,
                                          const SymbolInfo &SymInfo,
                                          const GeneralOptions &Options,
                                          bool IsSelected);

  CandidatesTableType Candidates;
  SymbolInfoMapType SymbolInfoMap;
  std::vector<const LDSymbol *> LTOObjectSymbols;
  std::unordered_map<const LDSymbol *, const LDSymbol *>
      BitcodeSymToLtoObjectSymMap;
  std::unordered_map<const LDSymbol *, const Plugin *> SymbolToPluginMap;
};

} // namespace eld
#endif
