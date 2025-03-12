#include "LinkerWrapper.h"
#include "PluginVersion.h"
#include "LinkerPlugin.h"
#include <iostream>
#include <unordered_map>

class AuxiliarySymbolNamePlugin : public eld::plugin::LinkerPlugin {
public:
  AuxiliarySymbolNamePlugin()
      : eld::plugin::LinkerPlugin("AuxiliarySymbolNamePlugin") {}

  void ActBeforeSectionMerging() override {
    const auto &inputFiles = getLinker()->getInputFiles();
    for (const eld::plugin::InputFile &IF : inputFiles) {
      std::string inputFileName = IF.getFileName();
      if (inputFileName.find("1.o") != std::string::npos) {
        eld::plugin::AuxiliarySymbolNameMap nameMap;
        for (const eld::plugin::Symbol S : IF.getSymbols()) {
          if (S.getName() == "foo")
            nameMap[S.getSymbolIndex()] = "myFoo";
          else if (S.getName() == "bar")
            nameMap[S.getSymbolIndex()] = "myBar";
        }
        auto expSetAuxSymNameMap =
            getLinker()->setAuxiliarySymbolNameMap(IF, nameMap);
        ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
            getLinker(), expSetAuxSymNameMap);
      }
    }
  }
};

class AuxiliarySymbolNamePluginAgain : public eld::plugin::LinkerPlugin {
public:
  AuxiliarySymbolNamePluginAgain()
      : eld::plugin::LinkerPlugin("AuxiliarySymbolNamePluginAgain") {}

  void ActBeforeSectionMerging() override {
    const auto &inputFiles = getLinker()->getInputFiles();
    for (const eld::plugin::InputFile &IF : inputFiles) {
      std::string inputFileName = IF.getFileName();
      if (inputFileName.find("1.o") != std::string::npos) {
        eld::plugin::AuxiliarySymbolNameMap nameMap;
        for (const eld::plugin::Symbol S : IF.getSymbols()) {
          if (S.getName() == "foo")
            nameMap[S.getSymbolIndex()] = "YetAnotherFoo";
          else if (S.getName() == "bar")
            nameMap[S.getSymbolIndex()] = "YetAnotherBar";
        }
        auto expSetAuxSymNameMap =
            getLinker()->setAuxiliarySymbolNameMap(IF, nameMap);
        ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
            getLinker(), expSetAuxSymNameMap);
      }
    }
  }
};

std::unordered_map<std::string, eld::plugin::PluginBase *> plugins;

extern "C" {
bool RegisterAll() {
  plugins["AuxiliarySymbolNamePlugin"] = new AuxiliarySymbolNamePlugin();
  plugins["AuxiliarySymbolNamePluginAgain"] =
      new AuxiliarySymbolNamePluginAgain();
  return true;
}

eld::plugin::PluginBase *getPlugin(const char *T) { return plugins[T]; }

void Cleanup() {
  for (auto &P : plugins) {
    delete P.second;
  }
  plugins.clear();
}
}
