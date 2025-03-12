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

class DLL_A_EXPORT PendingSectionOverrides
    : public OutputSectionIteratorPlugin {
public:
  PendingSectionOverrides()
      : OutputSectionIteratorPlugin("PendingSectionOverrides") {}

  void Init(std::string Options) override {}

  void processOutputSection(OutputSection S) override { return; }

  Status Run(bool Trace) override {
    if (getLinker()->getState() == LinkerWrapper::AfterLayout ||
        getLinker()->getState() == LinkerWrapper::CreatingSections)
      return Status::SUCCESS;
    eld::Expected<OutputSection> expO = getLinker()->getOutputSection(".data");
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expO);
    eld::plugin::OutputSection O = expO.value();

    for (auto &R : O.getLinkerScriptRules()) {
      for (auto &S : R.getSections()) {
        eld::Expected<void> expSetOutSect =
            getLinker()->setOutputSection(S, ".bar");
        ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expSetOutSect);
      }
    }
    return Plugin::Status::SUCCESS;
  }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "PendingSectionOverrides"; }
};

class DLL_A_EXPORT AnotherPendingSectionOverrides
    : public OutputSectionIteratorPlugin {
public:
  AnotherPendingSectionOverrides()
      : OutputSectionIteratorPlugin("AnotherPendingSectionOverrides") {}

  void Init(std::string Options) override {}

  void processOutputSection(OutputSection S) override { return; }

  Status Run(bool Trace) override {
    if (getLinker()->getState() == LinkerWrapper::AfterLayout ||
        getLinker()->getState() == LinkerWrapper::CreatingSections)
      return Status::SUCCESS;
    eld::Expected<OutputSection> expO =
        getLinker()->getOutputSection(".data");
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expO);
    eld::plugin::OutputSection O = expO.value();
    for (auto &R : O.getLinkerScriptRules()) {
      for (auto &S : R.getSections()) {
        eld::Expected<void> expSetOutSect =
            getLinker()->setOutputSection(S, ".bar");
        ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expSetOutSect);
      }
    }
    return Plugin::Status::SUCCESS;
  }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "AnotherPendingSectionOverrides"; }
};

std::unordered_map<std::string, Plugin *> Plugins;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  Plugins["PendingSectionOverrides"] = new PendingSectionOverrides();
  Plugins["AnotherPendingSectionOverrides"] =
      new AnotherPendingSectionOverrides();
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
