#include "Defines.h"
#include "PluginVersion.h"
#include "LinkerPlugin.h"
#include <iostream>

class DLL_A_EXPORT UP : public eld::plugin::LinkerPlugin {
public:
  UP() : eld::plugin::LinkerPlugin("UP") {}
  void Init(const std::string &options) override {
    std::cout << "UP: Hello World!\n";
  }

  void Destroy() override { std::cout << "UP: Bye World!\n"; }
};

eld::plugin::PluginBase *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new UP();
  return true;
}

eld::plugin::PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }

void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}
