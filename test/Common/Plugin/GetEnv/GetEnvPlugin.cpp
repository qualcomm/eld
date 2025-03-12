#include "PluginVersion.h"
#include "SectionMatcherPlugin.h"
#include <iostream>

using namespace eld::plugin;

class DLL_A_EXPORT GetEnvPlugin : public SectionMatcherPlugin {
public:
  GetEnvPlugin()
      : SectionMatcherPlugin("GetEnvPlugin") {}

  void Init(std::string Options) override {}

  void processSection(eld::plugin::Section S) override { }

  Status Run(bool Trace) override {
    std::optional<std::string> valueA = getLinker()->getEnv("ValueA");
    std::optional<std::string> valueB = getLinker()->getEnv("ValueB");
    if (valueA)
      std::cout << "ValueA: " << *valueA << "\n";
    else
      std::cout << "ValueA not defined!\n";
    if (valueB)
      std::cout << "ValueB: " << *valueB << "\n";
    else
      std::cout << "ValueB not defined!\n";
    return Plugin::Status::SUCCESS;
  }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "GetEnvPlugin"; }
};

Plugin *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new GetEnvPlugin();
  return true;
}
PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }
void DLL_A_EXPORT Cleanup() {
  delete ThisPlugin;
}
}
