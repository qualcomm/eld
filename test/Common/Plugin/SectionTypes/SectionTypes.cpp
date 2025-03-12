#include "PluginVersion.h"
#include "SectionIteratorPlugin.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>

using namespace eld::plugin;

class DLL_A_EXPORT SectionTypes : public SectionIteratorPlugin {
public:
  SectionTypes() : SectionIteratorPlugin("SECTIONTYPES") {}

  void Init(std::string Options) override {}

  void processSection(eld::plugin::Section S) override {

    std::cout << S.getInputFile().getFileName() << " " << S.getName() << " "
              << S.isProgBits() << " " << S.isNoBits() << " " << S.isCode()
              << " " << S.isAlloc() << " " << S.isWritable() << "\n";
  }

  Status Run(bool Trace) override {
    return Plugin::Status::SUCCESS;
  }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return PluginName; }
};

std::unordered_map<std::string, Plugin *> Plugins;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  Plugins["SECTIONTYPES"] = new SectionTypes();
  return true;
}
PluginBase DLL_A_EXPORT *getPlugin(const char *T) {
  return Plugins[std::string(T)];
}
void DLL_A_EXPORT Cleanup() {
  for (auto &A : Plugins)
    delete A.second;
  Plugins.clear();
}
}
