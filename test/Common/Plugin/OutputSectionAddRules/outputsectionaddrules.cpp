#include "LinkerWrapper.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginVersion.h"
#include "ThreadPool.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <string>
#include <unordered_map>

using namespace eld::plugin;

class DLL_A_EXPORT OSAddRules : public OutputSectionIteratorPlugin {

public:
  OSAddRules() : OutputSectionIteratorPlugin("ADDRULES") {}

  // Perform any initialization here
  void Init(std::string Options) override {}

  void processOutputSection(OutputSection O) override {
    if (getLinker()->getState() == LinkerWrapper::CreatingSections) {
      std::cout << "\n" << O.getName();
      if (!O.getLinkerScriptRules().size())
        return;
      eld::plugin::LinkerScriptRule F = O.getLinkerScriptRules().front();
      eld::Expected<eld::plugin::LinkerScriptRule> expBRule =
          getLinker()->createLinkerScriptRule(O, "before");
      ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
          getLinker(), expBRule);
      eld::plugin::LinkerScriptRule BRule = std::move(expBRule.value());
      eld::Expected<void> expInsertRule =
          getLinker()->insertBeforeRule(O, F, BRule);
      ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
          getLinker(), expInsertRule);
      eld::Expected<eld::plugin::LinkerScriptRule> expARule =
          getLinker()->createLinkerScriptRule(O, "after");
      ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
          getLinker(), expARule);
      eld::plugin::LinkerScriptRule ARule = std::move(expARule.value());
      eld::Expected<void> expInsertARule =
          getLinker()->insertAfterRule(O, F, ARule);
      ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
          getLinker(), expInsertARule);
    }
  }

  Status Run(bool Trace) override { return Plugin::Status::SUCCESS; }

  void foo() {}

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "ADDRULES"; }
};

std::unordered_map<std::string, Plugin *> Plugins;

Plugin *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new OSAddRules();
  return true;
}
PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }

void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}
