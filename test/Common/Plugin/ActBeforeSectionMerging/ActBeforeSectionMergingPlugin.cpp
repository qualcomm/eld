#include "Defines.h"
#include "PluginVersion.h"
#include "LinkerPlugin.h"
#include <iostream>

class DLL_A_EXPORT ActBeforeSectionMergingPlugin : public eld::plugin::LinkerPlugin {
public:
  ActBeforeSectionMergingPlugin() : eld::plugin::LinkerPlugin("ActBeforeSectionMergingPlugin") {}
  void ActBeforeSectionMerging() override {
    std::cout << "In ActBeforeSectionMergingPlugin\n";
  }
};

eld::plugin::PluginBase *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new ActBeforeSectionMergingPlugin();
  return true;
}

eld::plugin::PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }

void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}
