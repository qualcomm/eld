#include "LinkerWrapper.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginVersion.h"
#include <algorithm>
#include <assert.h>
#include <cstring>
#include <iostream>
#include <string>
#include <unordered_map>

using namespace eld::plugin;

class DLL_A_EXPORT OSIter : public OutputSectionIteratorPlugin {

public:
  OSIter() : OutputSectionIteratorPlugin("REMOVECHUNKS"), FooBar(nullptr) {}
  // Perform any initialization here
  void Init(std::string Options) override {}

  void processOutputSection(OutputSection O) override {
    if (getLinker()->getState() != LinkerWrapper::CreatingSections)
      return;

    if (O.getName() == ".foobar")
      FooBar = O;
  }

  // After the linker lays out the image, but before it creates the elf file,
  // it will call this run function.
  Status Run(bool Trace) override {
    if (getLinker()->getState() == LinkerWrapper::AfterLayout)
      return eld::plugin::Plugin::Status::SUCCESS;
    if (getLinker()->getState() != LinkerWrapper::CreatingSections)
      return eld::plugin::Plugin::Status::SUCCESS;

    auto linkerscriptRuleVec = FooBar.getLinkerScriptRules();
    std::vector<Chunk> vec = linkerscriptRuleVec[1].getChunks();
    eld::Expected<void> expRemoveChunk =
        getLinker()->removeChunk(linkerscriptRuleVec[0], vec[0]);
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expRemoveChunk);
    return Plugin::Status::SUCCESS;
  }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "REMOVECHUNKS"; }

private:
  eld::plugin::OutputSection FooBar;
};

std::unordered_map<std::string, Plugin *> Plugins;

Plugin *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new OSIter();
  return true;
}
PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }

void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}