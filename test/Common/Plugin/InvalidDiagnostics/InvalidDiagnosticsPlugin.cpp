#include "Diagnostics.h"
#include "PluginVersion.h"
#include "SectionMatcherPlugin.h"

using namespace eld::plugin;

class DLL_A_EXPORT InvalidDiagnosticsPlugin : public SectionMatcherPlugin {
public:
  InvalidDiagnosticsPlugin()
      : SectionMatcherPlugin("InvalidDiagnosticsPlugin") {}

  void Init(std::string Options) override {}

  void processSection(eld::plugin::Section S) override { }

  Status Run(bool Trace) override {
    getLinker()->reportDiag(10000, "arg1");
    getLinker()->reportDiag(eld::plugin::Diagnostic::error_file_does_not_exist());
    return Plugin::Status::SUCCESS;
  }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "InvalidDiagnosticsPlugin"; }
};

Plugin *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new InvalidDiagnosticsPlugin();
  return true;
}
PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }
void DLL_A_EXPORT Cleanup() {
  delete ThisPlugin;
}
}
