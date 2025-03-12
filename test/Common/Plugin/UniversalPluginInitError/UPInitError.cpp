#include "Defines.h"
#include "PluginVersion.h"
#include "LinkerPlugin.h"
#include <iostream>

class DLL_A_EXPORT UPInitError : public eld::plugin::LinkerPlugin {
public:
  UPInitError() : eld::plugin::LinkerPlugin("UPInitError") {}
  void Init(const std::string &options) override {
    auto errID = getLinker()->getErrorDiagID("Something bad happened!");
    getLinker()->reportDiag(errID);
  }
};

eld::plugin::PluginBase *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new UPInitError();
  return true;
}

eld::plugin::PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }

void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}
