#include "Defines.h"
#include "LinkerPlugin.h"
#include "LinkerWrapper.h"
#include "PluginADT.h"
#include "PluginVersion.h"
#include <iostream>

using namespace eld::plugin;

class DLL_A_EXPORT geteldVersion : public LinkerPlugin {

public:
  geteldVersion() : LinkerPlugin("GETELDVERSION") {}

  void Init(const std::string &Options) override;

  void ActBeforeRuleMatching() override {
    std::cout << "ELD Major Version is: " << getLinker()->getEldMajorVersion()
              << "\n";
    std::cout << "ELD Minor Version is: " << getLinker()->getEldMinorVersion()
              << "\n";
  }
  void Destroy() override {}
};

void geteldVersion::Init(const std::string &Options) {}

std::unordered_map<std::string, PluginBase *> Plugins;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  Plugins["GETELDVERSION"] = new geteldVersion();
  return true;
}
PluginBase DLL_A_EXPORT *getPlugin(const char *T) {
  return Plugins[std::string(T)];
}
void DLL_A_EXPORT Cleanup() {
  for (auto &A : Plugins)
    delete A.second;
  Plugins.clear();
}
}