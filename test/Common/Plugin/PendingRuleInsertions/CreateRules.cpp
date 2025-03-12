#include "Defines.h"
#include "LinkerScript.h"
#include "LinkerWrapper.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginADT.h"
#include "PluginVersion.h"
#include <cassert>

class DLL_A_EXPORT CreateRules : public eld::plugin::OutputSectionIteratorPlugin {
public:
  CreateRules() : eld::plugin::OutputSectionIteratorPlugin("CreateRules") {}

  void Init(std::string options) override {}

  void processOutputSection(eld::plugin::OutputSection O) override {
    if (getLinker()->getState() != eld::plugin::LinkerWrapper::CreatingSections)
      return;
    if (O.getName() == "foo")
      m_Foo = O;
    else if (O.getName() == "bar")
      m_Bar = O;
  }

  Status Run(bool trace) override {
    if (getLinker()->getState() != eld::plugin::LinkerWrapper::CreatingSections)
      return Status::SUCCESS;
    assert(m_Foo && m_Bar && "foo and bar output sections must be present!");
    eld::Expected<eld::plugin::LinkerScriptRule> expNewFooRule =
        getLinker()->createLinkerScriptRule(m_Foo, "New foo rule");
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expNewFooRule);
    eld::Expected<eld::plugin::LinkerScriptRule> expNewBarRule =
        getLinker()->createLinkerScriptRule(m_Bar, "New bar rule");
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expNewBarRule);
    return Status::SUCCESS;
  }

  std::string GetName() override { return "CreateRules"; }

  std::string GetLastErrorAsString() override { return "Success"; }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

private:
  eld::plugin::OutputSection m_Foo{nullptr};
  eld::plugin::OutputSection m_Bar{nullptr};
};

CreateRules *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new CreateRules();
  return true;
}

eld::plugin::PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }

void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}