//===- PluginManager.cpp---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#include "eld/Plugin/PluginManager.h"
#include "eld/Config/GeneralOptions.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Diagnostics/MsgHandler.h"
#include "eld/Input/InputFile.h"
#include "eld/Script/Plugin.h"
#include "eld/Support/RegisterTimer.h"
#include "eld/Support/StringUtils.h"

using namespace eld;

void PluginManager::storeUniversalPlugins() {
  UniversalPlugins = LS.getPluginForType(plugin::PluginBase::LinkerPlugin);
}

bool PluginManager::callInitHook() {
  RegisterTimer T("Init", "Plugins", ShouldPrintTimingStats);
  for (auto P : UniversalPlugins) {
    P->callInitHook();
    if (!DE.diagnose())
      return false;
  }
  return true;
}

bool PluginManager::callDestroyHook() {
  RegisterTimer T("Destroy", "Plugins", ShouldPrintTimingStats);
  for (auto P : UniversalPlugins) {
    P->callDestroyHook();
    if (!DE.diagnose())
      return false;
  }
  return true;
}

bool PluginManager::processPluginCommandLineOptions(GeneralOptions &options) {
  RegisterTimer T("PluginCommandLineOptions", "Plugins",
                  ShouldPrintTimingStats);
  const auto &unknownOpts = options.getUnknownOptions();
  std::unordered_set<std::size_t> usedOpts;
  for (Plugin *P : UniversalPlugins) {
    for (std::size_t unknownOptIdx = 0; unknownOptIdx < unknownOpts.size();
         ++unknownOptIdx) {
      const std::string &option = unknownOpts[unknownOptIdx];
      std::string::size_type pos = option.find("=");
      std::string optName = option.substr(0, pos);
      std::optional<std::string> optValue;
      if (pos != std::string::npos)
        optValue = string::unquote(option.substr(pos + 1));
      for (const auto &optSpec : P->getPluginCommandLineOptions()) {
        if (optSpec.match(optName, optValue)) {
          P->callCommandLineOptionHandler(optName, optValue,
                                          optSpec.OptionHandler);
          usedOpts.insert(unknownOptIdx);
          if (!DE.diagnose())
            return false;
        }
      }
    }
  }
  std::vector<std::string> unusedOpts;
  for (std::size_t i = 0; i < unknownOpts.size(); ++i) {
    if (!usedOpts.count(i))
      unusedOpts.push_back(unknownOpts[i]);
  }
  options.setUnknownOptions(unusedOpts);
  return true;
}

bool PluginManager::callVisitSectionsHook(InputFile &IF) {
  RegisterTimer T("VisitSections", "Plugins", ShouldPrintTimingStats);
  for (auto P : UniversalPlugins) {
    P->callVisitSectionsHook(IF);
    if (!DE.diagnose())
      return false;
  }
  return true;
}

bool PluginManager::callActBeforeRuleMatchingHook() {
  RegisterTimer T("ActBeforeRuleMatching", "Plugins", ShouldPrintTimingStats);
  for (auto P : UniversalPlugins) {
    P->callActBeforeRuleMatchingHook();
    if (!DE.diagnose())
      return false;
  }
  return true;
}

bool PluginManager::callVisitSymbolHook(LDSymbol *sym, llvm::StringRef symName,
                                        const SymbolInfo &symInfo) {
  RegisterTimer T("VisitSymbol", "Plugins", ShouldPrintTimingStats);
  for (auto P : UniversalPlugins) {
    if (SymbolVisitors.count(P)) {
      P->callVisitSymbolHook(sym, symName, symInfo);
      if (!DE.diagnose())
        return false;
    }
  }
  return true;
}

void PluginManager::addSymbolVisitor(eld::Plugin *P) {
  SymbolVisitors.insert(P);
  if (DE.getPrinter()->tracePlugins())
    DE.raise(diag::trace_plugin_enable_visit_symbol) << P->getPluginName();
}

bool PluginManager::callActBeforeSectionMergingHook() {
  RegisterTimer T("ActBeforeSectionMerging", "Plugins", ShouldPrintTimingStats);
  for (auto P : UniversalPlugins) {
    P->callActBeforeSectionMergingHook();
    if (!DE.diagnose())
      return false;
  }
  return true;
}

void PluginManager::enableShowRMSectNameInDiag(LinkerConfig &config,
                                               const eld::Plugin &P) {
  DE.raise(diag::trace_show_RM_sect_name_in_diag) << P.getPluginName();
  config.options().enableShowRMSectNameInDiag();
}

void PluginManager::setAuxiliarySymbolNameMap(
    ObjectFile *objFile,
    const ObjectFile::AuxiliarySymbolNameMap &auxSymNameMap, const Plugin *P) {
  objFile->setAuxiliarySymbolNameMap(auxSymNameMap);
  AuxSymNameMapProvider[objFile] = P;
  if (DE.getPrinter()->tracePlugins())
    DE.raise(diag::trace_set_aux_sym_name_map)
        << P->getPluginName() << objFile->getInput()->decoratedPath();
}

bool PluginManager::callActBeforePerformingLayoutHook() {
  RegisterTimer T("ActBeforePerformingLayout", "Plugins",
                  ShouldPrintTimingStats);
  for (auto P : UniversalPlugins) {
    P->callActBeforePerformingLayoutHook();
    if (!DE.diagnose())
      return false;
  }
  return true;
}

bool PluginManager::callActBeforeWritingOutputHook() {
  RegisterTimer T("ActBeforeWritingOutput", "Plugins", ShouldPrintTimingStats);
  for (auto P : UniversalPlugins) {
    P->callActBeforeWritingOutputHook();
    if (!DE.diagnose())
      return false;
  }
  return true;
}
