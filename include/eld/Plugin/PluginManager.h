//===- PluginManager.h-----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#ifndef ELD_PLUGIN_PLUGINMANAGER_H
#define ELD_PLUGIN_PLUGINMANAGER_H
#include "eld/Core/LinkerScript.h"
#include <unordered_map>
#include <vector>
namespace eld {
class DiagnosticEngine;
class GeneralOptions;
class InputFile;
class LinkerScript;
class Plugin;

/// Manages plugins.
///
/// It is used to house plugin related functionalities such as
/// routines for calling a plugin hook for all plugins.
class PluginManager {
public:
  explicit PluginManager(const LinkerScript &linkerScript,
                         DiagnosticEngine &diagEngine,
                         bool shouldPrintTimingStats)
      : LS(linkerScript), DE(diagEngine),
        ShouldPrintTimingStats(shouldPrintTimingStats) {}

  /// Stores all the plugins having the UniversalPlugin type.
  ///
  /// It is used to efficiently iterate over universal plugins.
  void storeUniversalPlugins();

  /// Call Init hook handler of each universal plugin. Returns false if
  /// any of the callbacks returns false.
  bool callInitHook();

  /// Call Destroy hook handler of each universal plugin. Returns false if
  /// any of the callbacks returns false.
  bool callDestroyHook();

  const LinkerScript::PluginVectorT &getUniversalPlugins() {
    return UniversalPlugins;
  }

  /// Invokes command-line option handlers for registered options present in the
  /// link command-line.
  bool processPluginCommandLineOptions(GeneralOptions &options);

  /// Call VisitSection hook handler of each universal plugin. Returns false if
  /// any of the callback fails.
  bool callVisitSectionsHook(InputFile &IF);

  bool callVisitSymbolHook(LDSymbol *sym, llvm::StringRef,
                           const SymbolInfo &symInfo);

  void addSymbolVisitor(eld::Plugin *P);

  void addRMSectionNameMapProvider(const InputFile *IF, const Plugin *P) {
    RMSectNameMapProvider[IF] = P;
  }

  const Plugin *getRMSectionNameMapProvider(const InputFile *IF) {
    auto it = RMSectNameMapProvider.find(IF);
    if (it != RMSectNameMapProvider.end())
      return it->second;
    return nullptr;
  }

  bool callActBeforeRuleMatchingHook();

  bool callActBeforeSectionMergingHook();

  void enableShowRMSectNameInDiag(LinkerConfig &config, const eld::Plugin &P);

  void setAuxiliarySymbolNameMap(
      ObjectFile *objFile,
      const ObjectFile::AuxiliarySymbolNameMap &auxSymNameMap, const Plugin *P);

  const Plugin *getAuxiliarySymbolNameMapProvider(const InputFile *IF) {
    auto it = AuxSymNameMapProvider.find(IF);
    if (it != AuxSymNameMapProvider.end())
      return it->second;
    return nullptr;
  }

  bool callActBeforePerformingLayoutHook();

  bool callActBeforeWritingOutputHook();

private:
  const LinkerScript &LS;
  DiagnosticEngine &DE;
  LinkerScript::PluginVectorT UniversalPlugins;

  /// Set of plugins that have enabled VisitSymbols plugin hook.
  std::unordered_set<eld::Plugin *> SymbolVisitors;

  /// Rule-matching section name map provider. This map keeps track of which
  /// plugin provided the rule-matching section name map to an input file.
  std::unordered_map<const InputFile *, const Plugin *> RMSectNameMapProvider;

  /// Auxiliary symbol name map provider. This map keeps track of which plugin
  /// provided the auxiliary symbol name map to an input file.
  std::unordered_map<const InputFile *, const Plugin *> AuxSymNameMapProvider;

  const bool ShouldPrintTimingStats;
};
} // namespace eld

#endif
