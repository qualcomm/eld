#include "LinkerPlugin.h"
#include "PluginVersion.h"
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

using namespace eld::plugin;

class DLL_A_EXPORT LinkModeLinkerPlugin : public LinkerPlugin {
public:
  LinkModeLinkerPlugin() : LinkerPlugin("LINKMODE") {}

  void Init(const std::string &Options) override;

  void ActBeforeSectionMerging() override;

  void Destroy() override {}
};

void LinkModeLinkerPlugin::Init(const std::string &Options) {}

void LinkModeLinkerPlugin::ActBeforeSectionMerging() {
  LinkerWrapper::LinkMode LinkMode = getLinker()->getLinkMode();
  if (LinkMode == LinkerWrapper::LinkMode::UnknownLinkMode) {
    std::cout << "Unknown" << "\n";
    return;
  }
  if (LinkMode == LinkerWrapper::LinkMode::StaticExecutable) {
    std::cout << "StaticExecutable" << "\n";
    return;
  }
  if (LinkMode == LinkerWrapper::LinkMode::DynamicExecutable) {
    std::cout << "DynamicExecutable" << "\n";
    return;
  }
  if (LinkMode == LinkerWrapper::LinkMode::SharedLibrary) {
    std::cout << "SharedLibrary" << "\n";
    return;
  }
  if (LinkMode == LinkerWrapper::LinkMode::PIE) {
    std::cout << "PIE" << "\n";
    return;
  }
  if (LinkMode == LinkerWrapper::LinkMode::PartialLink) {
    std::cout << "PartialLink" << "\n";
    return;
  }
}

// Standard Linker plugin template.
std::unordered_map<std::string, PluginBase *> Plugins;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  Plugins["LINKMODE"] = new LinkModeLinkerPlugin();
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
