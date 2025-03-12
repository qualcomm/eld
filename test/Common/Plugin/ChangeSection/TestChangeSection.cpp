#include "LinkerWrapper.h"
#include "PluginVersion.h"
#include "SectionMatcherPlugin.h"

class ChangeSection : public eld::plugin::SectionMatcherPlugin {
public:
  ChangeSection() : eld::plugin::SectionMatcherPlugin("ChangeSection") {}

  void Init(std::string cfg) override {}

  void processSection(eld::plugin::Section S) override {
    if (S.matchPattern("*foo")) {
      eld::Expected<void> expSetSect = getLinker()->setOutputSection(S, "bar");
      ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
          getLinker(), expSetSect);
    }
  }

  eld::plugin::Plugin::Status Run(bool trace) override {
    return eld::plugin::Plugin::Status::SUCCESS;
  }

  // 'Destroy' callback hook can be used for finalization and clean-up tasks.
  // It is called once for each section iterator plugin run.
  void Destroy() override {
    eld::Expected<void> expFinishAssign =
        getLinker()->finishAssignOutputSections();
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
        getLinker(), expFinishAssign);
  }

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "Success"; }

  std::string GetName() override { return "ChangeSection"; }
};

eld::plugin::Plugin *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new ChangeSection{};
  return true;
}

eld::plugin::PluginBase DLL_A_EXPORT *getPlugin(const char *pluginName) {
  return ThisPlugin;
}

void DLL_A_EXPORT Cleanup() {
  if (!ThisPlugin)
    return;
  delete ThisPlugin;
  ThisPlugin = nullptr;
}
}
