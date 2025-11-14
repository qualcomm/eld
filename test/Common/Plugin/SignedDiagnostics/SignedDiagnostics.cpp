#include "PluginVersion.h"
#include "SectionIteratorPlugin.h"
#include <string>

using namespace eld::plugin;

class DLL_A_EXPORT SignedDiagnostics : public SectionIteratorPlugin {

public:
  SignedDiagnostics() : SectionIteratorPlugin("SignedDiagnostics") {}

  void Init(std::string Options) override {}

  void processSection(eld::plugin::Section S) override {}

  Status Run(bool Trace) override {
    auto Diag = getLinker()->getWarningDiagID(
        "positive 64-bit value: %0\n negative 64-bit value: %1");
    getLinker()->reportDiag(Diag, std::numeric_limits<int64_t>::max(),
                            std::numeric_limits<int64_t>::min());
    return Plugin::Status::SUCCESS;
  }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "SignedDiagnostics"; }
};

Plugin *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new SignedDiagnostics();
  return true;
}
PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }
void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
}
}
