#include "Defines.h"
#include "PluginVersion.h"
#include "LinkerPlugin.h"
#include <iostream>

class DLL_A_EXPORT UPInitAndDestroy : public eld::plugin::LinkerPlugin {
public:
  UPInitAndDestroy() : eld::plugin::LinkerPlugin("UPInitAndDestroy") {}
  void Init(const std::string &options) override {
    std::cout << "Hello World!\n";
    std::cout << "options: " << options << "\n";
  }

  void Destroy() override { std::cout << "Bye World!\n"; }
};

eld::plugin::PluginBase *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new UPInitAndDestroy();
  return true;
}

eld::plugin::PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }

void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}
