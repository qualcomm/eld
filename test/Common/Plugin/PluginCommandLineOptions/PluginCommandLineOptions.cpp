#include "Defines.h"
#include "PluginVersion.h"
#include "LinkerPlugin.h"
#include <iostream>

class DLL_A_EXPORT PluginCommandLineOptions : public eld::plugin::LinkerPlugin {
public:
  PluginCommandLineOptions() : eld::plugin::LinkerPlugin("PluginCommandLineOptions") {}
  void Init(const std::string &options) override {
    auto commandLineOptHandler = [=](const std::string &option,
                                     const std::optional<std::string> &val) {
      std::cout << "option: " << option;
      if (val)
        std::cout << ", value: " << val.value();
      std::cout << "\n";
    };

    getLinker()->registerCommandLineOption("--optA", /*hasValue=*/true,
                                           commandLineOptHandler);
    getLinker()->registerCommandLineOption("--optB", /*hasValue=*/true,
                                           commandLineOptHandler);
    getLinker()->registerCommandLineOption("--flagA", /*hasValue=*/false,
                                           commandLineOptHandler);
    getLinker()->registerCommandLineOption("--flagB", /*hasValue=*/false,
                                           commandLineOptHandler);
  }
};

eld::plugin::PluginBase *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new PluginCommandLineOptions();
  return true;
}

eld::plugin::PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }

void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}
