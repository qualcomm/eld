#include "LinkerPlugin.h"
#include "LinkerScript.h"
#include "PluginVersion.h"
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

class DLL_A_EXPORT SearchDiagnosticsPlugin : public eld::plugin::LinkerPlugin {
public:

  SearchDiagnosticsPlugin() : LinkerPlugin("TEST") {}

  void Init(const std::string &Options) override {
    getLinker()->findConfigFile(PluginName + ".config");
  }
};

// Standard Linker plugin template.
std::unordered_map<std::string, eld::plugin::PluginBase *> Plugins;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  Plugins["TEST"] = new SearchDiagnosticsPlugin();
  return true;
}
eld::plugin::PluginBase DLL_A_EXPORT *getPlugin(const char *T) {
  return Plugins[std::string(T)];
}
void DLL_A_EXPORT Cleanup() {
  for (auto &A : Plugins)
    delete A.second;
  Plugins.clear();
}
}
