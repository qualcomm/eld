#include "Defines.h"
#include "PluginVersion.h"
#include "LinkerPlugin.h"
#include <iostream>

class DLL_A_EXPORT InvalidPrefixAndEmptyOpt : public eld::plugin::LinkerPlugin {
public:
  InvalidPrefixAndEmptyOpt()
      : eld::plugin::LinkerPlugin("InvalidPrefixAndEmptyOpt") {}
  void Init(const std::string &options) override {
    auto commandLineOptHandler = [](const std::string &option,
                                    const std::optional<std::string> &val) {};
    auto expRegOpt1 = getLinker()->registerCommandLineOption(
        "-optA", /*hasValue=*/true, commandLineOptHandler);
    if (!expRegOpt1)
      getLinker()->reportDiagEntry(std::move(expRegOpt1.error()));
    auto expRegOpt2 = getLinker()->registerCommandLineOption(
        "optB", /*hasValue=*/true, commandLineOptHandler);
    if (!expRegOpt2)
      getLinker()->reportDiagEntry(std::move(expRegOpt2.error()));
    auto expRegOpt3 = getLinker()->registerCommandLineOption(
        "--", /*hasValue=*/false, commandLineOptHandler);
    if (!expRegOpt3)
      getLinker()->reportDiagEntry(std::move(expRegOpt3.error()));
  }
};

eld::plugin::PluginBase *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new InvalidPrefixAndEmptyOpt();
  return true;
}

eld::plugin::PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }

void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}
