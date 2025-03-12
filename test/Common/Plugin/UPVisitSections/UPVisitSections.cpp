#include "Defines.h"
#include "PluginVersion.h"
#include "LinkerPlugin.h"
#include <iostream>

class DLL_A_EXPORT UPVisitSections : public eld::plugin::LinkerPlugin {
public:
  UPVisitSections() : eld::plugin::LinkerPlugin("UPVisitSections") {}
  void VisitSections(eld::plugin::InputFile IF) override {
    std::cout << "InputFile: " << IF.getFileName() << "\n";
    std::cout << "Sections:\n";
    for (auto S : IF.getSections()) {
      std::cout << "  " << S.getName() << "\n";
    }
    std::cout << "\n";
  }
};

eld::plugin::PluginBase *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new UPVisitSections();
  return true;
}

eld::plugin::PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }

void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}
