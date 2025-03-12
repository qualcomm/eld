#include "PluginVersion.h"
#include "LinkerPlugin.h"
#include <iostream>

class TimingReportPlugin : public eld::plugin::LinkerPlugin {
public:
  TimingReportPlugin() : eld::plugin::LinkerPlugin("TimingReportPlugin") {}
};

eld::plugin::PluginBase *ThisPlugin = nullptr;

extern "C" {
bool RegisterAll() {
  ThisPlugin = new TimingReportPlugin();
  return true;
}

eld::plugin::PluginBase *getPlugin(const char *T) { return ThisPlugin; }

void Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}
