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
  for (auto *P : UniversalPlugins) {
    RegisterTimer T(P->getPluginName() + "/Init", "Plugins",
                    ShouldPrintTimingStats);
    P->callInitHook();
    if (!DE.diagnose())
      return false;
  }
  return true;
}

bool PluginManager::callDestroyHook() {
  for (auto *P : UniversalPlugins) {
    RegisterTimer T(P->getPluginName() + "/Destroy", "Plugins",
                    ShouldPrintTimingStats);
    P->callDestroyHook();
    if (!DE.diagnose())
      return false;
  }
  return true;
}

bool PluginManager::processPluginCommandLineOptions(GeneralOptions &Options) {
  const auto &UnknownOpts = Options.getUnknownOptions();
  std::unordered_set<std::size_t> UsedOpts;
  for (Plugin *P : UniversalPlugins) {
    RegisterTimer T(P->getPluginName() + "/CommandLineOptions", "Plugins",
                    ShouldPrintTimingStats);
    for (std::size_t UnknownOptIdx = 0; UnknownOptIdx < UnknownOpts.size();
         ++UnknownOptIdx) {
      const std::string &Option = UnknownOpts[UnknownOptIdx];
      std::string::size_type Pos = Option.find("=");
      std::string OptName = Option.substr(0, Pos);
      std::optional<std::string> OptValue;
      if (Pos != std::string::npos)
        OptValue = string::unquote(Option.substr(Pos + 1));
      for (const auto &OptSpec : P->getPluginCommandLineOptions()) {
        if (OptSpec.match(OptName, OptValue)) {
          P->callCommandLineOptionHandler(OptName, OptValue,
                                          OptSpec.OptionHandler);
          UsedOpts.insert(UnknownOptIdx);
          if (!DE.diagnose())
            return false;
        }
      }
    }
  }
  std::vector<std::string> UnusedOpts;
  for (std::size_t I = 0; I < UnknownOpts.size(); ++I) {
    if (!UsedOpts.count(I))
      UnusedOpts.push_back(UnknownOpts[I]);
  }
  Options.setUnknownOptions(UnusedOpts);
  return true;
}

bool PluginManager::callVisitSectionsHook(InputFile &IF) {
  for (auto *P : UniversalPlugins) {
    RegisterTimer T(P->getPluginName() + "/VisitSections", "Plugins",
                    ShouldPrintTimingStats);
    P->callVisitSectionsHook(IF);
    if (!DE.diagnose())
      return false;
  }
  return true;
}

bool PluginManager::callActBeforeRuleMatchingHook() {
  for (auto *P : UniversalPlugins) {
    RegisterTimer T(P->getPluginName() + "/ActBeforeRuleMatching", "Plugins",
                    ShouldPrintTimingStats);
    P->callActBeforeRuleMatchingHook();
    if (!DE.diagnose())
      return false;
  }
  return true;
}

bool PluginManager::callVisitSymbolHook(LDSymbol *Sym, llvm::StringRef SymName,
                                        const SymbolInfo &SymInfo) {
  for (auto *P : UniversalPlugins) {
    if (SymbolVisitors.count(P)) {
      RegisterTimer T(P->getPluginName() + "/VisitSymbol", "Plugins",
                      ShouldPrintTimingStats);
      P->callVisitSymbolHook(Sym, SymName, SymInfo);
      if (!DE.diagnose())
        return false;
    }
  }
  return true;
}

void PluginManager::addSymbolVisitor(eld::Plugin *P) {
  SymbolVisitors.insert(P);
  if (DE.getPrinter()->tracePlugins())
    DE.raise(Diag::trace_plugin_enable_visit_symbol) << P->getPluginName();
}

bool PluginManager::callActBeforeSectionMergingHook() {
  for (auto *P : UniversalPlugins) {
    RegisterTimer T(P->getPluginName() + "/ActBeforeSectionMerging", "Plugins",
                    ShouldPrintTimingStats);
    P->callActBeforeSectionMergingHook();
    if (!DE.diagnose())
      return false;
  }
  return true;
}

void PluginManager::enableShowRMSectNameInDiag(LinkerConfig &Config,
                                               const eld::Plugin &P) {
  DE.raise(Diag::trace_show_RM_sect_name_in_diag) << P.getPluginName();
  Config.options().enableShowRMSectNameInDiag();
}

void PluginManager::setAuxiliarySymbolNameMap(
    ObjectFile *ObjFile,
    const ObjectFile::AuxiliarySymbolNameMap &AuxSymNameMap, const Plugin *P) {
  ObjFile->setAuxiliarySymbolNameMap(AuxSymNameMap);
  AuxSymNameMapProvider[ObjFile] = P;
  if (DE.getPrinter()->tracePlugins())
    DE.raise(Diag::trace_set_aux_sym_name_map)
        << P->getPluginName() << ObjFile->getInput()->decoratedPath();
}

bool PluginManager::callActBeforePerformingLayoutHook() {
  for (auto *P : UniversalPlugins) {
    RegisterTimer T(P->getPluginName() + "/ActBeforePerformingLayout",
                    "Plugins", ShouldPrintTimingStats);
    P->callActBeforePerformingLayoutHook();
    if (!DE.diagnose())
      return false;
  }
  return true;
}

bool PluginManager::callActBeforeWritingOutputHook() {
  for (auto *P : UniversalPlugins) {
    RegisterTimer T(P->getPluginName() + "/ActBeforeWritingOutput", "Plugins",
                    ShouldPrintTimingStats);
    P->callActBeforeWritingOutputHook();
    if (!DE.diagnose())
      return false;
  }
  return true;
}
