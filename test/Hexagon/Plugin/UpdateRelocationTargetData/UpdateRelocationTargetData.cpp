#include "LinkerWrapper.h"
#include "PluginVersion.h"
#include "SectionIteratorPlugin.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>

//namespace {

class DLL_A_EXPORT UpdateRelocationTargetData
    : public eld::plugin::SectionIteratorPlugin,
      public eld::plugin::LinkerPluginConfig {
public:
  UpdateRelocationTargetData()
      : eld::plugin::SectionIteratorPlugin("UpdateRelocationTargetData"),
        eld::plugin::LinkerPluginConfig(this) {}

  void Init(std::string Options) override {
    eld::Expected<void> expRegReloc = getLinker()->registerReloc(
        getLinker()->getRelocationHandler().getRelocationType("R_HEX_NONE"));
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
        getLinker(), expRegReloc);
  }

  void Init() override {}
  void Destroy() override {}
  std::string GetName() override { return PluginName; }
  Status Run(bool Trace) override { return Plugin::Status::SUCCESS; }
  uint32_t GetLastError() override { return 0; }
  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  void RelocCallBack(eld::plugin::Use U) override {
    if (getLinker()->isMultiThreaded()) {
      std::lock_guard<std::mutex> M(Mutex);
      processRelocation(U);
      return;
    }
    processRelocation(U);
  }

private:
  std::mutex Mutex;

  void processSection(eld::plugin::Section S) override {}

  void processRelocation(eld::plugin::Use U) {
    eld::Expected<void> expSetTargetData =
        getLinker()->setTargetDataForUse(U, 0x12340000 + U.getOffsetInChunk());
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
        getLinker(), expSetTargetData);
  }
};

std::unique_ptr<UpdateRelocationTargetData> Plugin;

// } // namespace

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  if (!Plugin)
    Plugin = std::make_unique<UpdateRelocationTargetData>();
  return true;
}

void DLL_A_EXPORT Cleanup() { Plugin = nullptr; }

eld::plugin::PluginBase DLL_A_EXPORT *getPlugin(const char *T) {
  return Plugin && Plugin->GetName() == T ? Plugin.get() : nullptr;
}

eld::plugin::LinkerPluginConfig DLL_A_EXPORT *getPluginConfig(const char *T) {
  return Plugin && Plugin->GetName() == T ? Plugin.get() : nullptr;
}
}
