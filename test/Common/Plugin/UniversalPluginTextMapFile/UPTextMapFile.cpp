#include "Defines.h"
#include "PluginVersion.h"
#include "LinkerPlugin.h"
#include <iostream>

class DLL_A_EXPORT UPTextMapFile : public eld::plugin::LinkerPlugin {
public:
  UPTextMapFile() : eld::plugin::LinkerPlugin("UPTextMapFile") {}
};

eld::plugin::PluginBase *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new UPTextMapFile();
  return true;
}

eld::plugin::PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }

void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}
