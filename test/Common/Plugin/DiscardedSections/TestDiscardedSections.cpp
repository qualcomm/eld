#include "LinkerWrapper.h"
#include "PluginVersion.h"
#include "SectionMatcherPlugin.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>

using namespace eld::plugin;

class DLL_A_EXPORT DiscardedSections : public SectionMatcherPlugin {
public:
  DiscardedSections() : SectionMatcherPlugin("DiscardedSections") {}

  void Init(std::string Options) override {}

  void processSection(Section S) override {
    std::cout << "Processing section: " << S.getName() << "\n";
  }

  Status Run(bool Trace) override { return Plugin::Status::SUCCESS; }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "DiscardedSections"; }
};

std::unordered_map<std::string, Plugin *> Plugins;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  Plugins["DiscardedSections"] = new DiscardedSections();
  return true;
}

PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return Plugins[T]; }
void DLL_A_EXPORT Cleanup() {
  for (auto &item : Plugins) {
    if (item.second)
      delete item.second;
  }
  Plugins.clear();
}
}
