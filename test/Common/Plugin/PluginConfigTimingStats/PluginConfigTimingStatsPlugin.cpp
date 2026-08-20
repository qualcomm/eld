#include "Defines.h"
#include "LinkerPlugin.h"
#include "PluginVersion.h"
#include <iostream>

class DLL_A_EXPORT PluginConfigTimingStatsPlugin
    : public eld::plugin::LinkerPlugin {
public:
  PluginConfigTimingStatsPlugin()
      : eld::plugin::LinkerPlugin("PluginConfigTimingStatsPlugin") {}

  void Init(const std::string &options) override {
    std::cout << "Init called with options: " << options << "\n";
  }
};

eld::plugin::PluginBase *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new PluginConfigTimingStatsPlugin();
  return true;
}

eld::plugin::PluginBase DLL_A_EXPORT *getPlugin(const char *T) {
  return ThisPlugin;
}

void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}
