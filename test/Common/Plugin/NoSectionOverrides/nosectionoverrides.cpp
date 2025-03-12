#include "LinkerWrapper.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginVersion.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>

using namespace eld::plugin;

class DLL_A_EXPORT NoSectionOverrides
    : public OutputSectionIteratorPlugin {
public:
  NoSectionOverrides()
      : OutputSectionIteratorPlugin("NoSectionOverrides") {}

  void Init(std::string Options) override {}

  void processOutputSection(OutputSection S) override { return; }

  Status Run(bool Trace) override {
    if (getLinker()->getState() == LinkerWrapper::State::BeforeLayout) {
      eld::Expected<void> expFinishAssign =
          getLinker()->finishAssignOutputSections();
      ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expFinishAssign);
    }
    return Plugin::Status::SUCCESS;
  }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "NoSectionOverrides"; }
};

std::unordered_map<std::string, Plugin *> Plugins;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  Plugins["NoSectionOverrides"] = new NoSectionOverrides();
  return true;
}

PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return Plugins[T]; }
void DLL_A_EXPORT Cleanup() {
  for (auto& item : Plugins) {
    if (item.second)
      delete item.second;
  }
  Plugins.clear();
}
}
