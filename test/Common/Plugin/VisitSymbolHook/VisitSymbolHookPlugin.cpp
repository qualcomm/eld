#include "Defines.h"
#include "PluginVersion.h"
#include "LinkerPlugin.h"
#include <iostream>

class DLL_A_EXPORT VisitSymbolHook : public eld::plugin::LinkerPlugin {
public:
  VisitSymbolHook() : eld::plugin::LinkerPlugin("VisitSymbolHook") {}
  void Init(const std::string &options) override {
    getLinker()->enableVisitSymbol();
  }

  void VisitSymbol(eld::plugin::InputSymbol S) override {
    std::cout << "Visiting symbol: " << S.getName() << "\n";
  }
};

eld::plugin::PluginBase *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new VisitSymbolHook();
  return true;
}

eld::plugin::PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }

void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}
