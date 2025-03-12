#include "PluginVersion.h"
#include "LinkerPlugin.h"
#include <iostream>

class ActBeforePerformingLayoutPlugin : public eld::plugin::LinkerPlugin {
public:
  ActBeforePerformingLayoutPlugin()
      : eld::plugin::LinkerPlugin("ActBeforePerformingLayoutPlugin") {}
  void ActBeforePerformingLayout() override {
    getLinker()->reportDiag(
        getLinker()->getNoteDiagID("In ActBeforePerformingLayout"));
  }
};

eld::plugin::PluginBase *ThisPlugin = nullptr;

extern "C" {
bool RegisterAll() {
  ThisPlugin = new ActBeforePerformingLayoutPlugin();
  return true;
}

eld::plugin::PluginBase *getPlugin(const char *T) { return ThisPlugin; }

void Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}
