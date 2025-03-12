#include "PluginVersion.h"
#include "SectionMatcherPlugin.h"
#include <iostream>

using namespace eld::plugin;

class DLL_A_EXPORT nonUP : public SectionMatcherPlugin {
public:
  nonUP()
      : SectionMatcherPlugin("nonUP") {}

  void Init(std::string Options) override {
    std::cout << "nonUP: Hello World!\n";
  }

  void processSection(eld::plugin::Section S) override { }

  Status Run(bool Trace) override { return Status::SUCCESS; }

  void Destroy() override {
    std::cout << "nonUP: Bye World!\n";
  }

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "nonUP"; }
};

Plugin *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new nonUP();
  return true;
}
PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }
void DLL_A_EXPORT Cleanup() {
  delete ThisPlugin;
}
}
