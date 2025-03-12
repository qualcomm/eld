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

class DLL_A_EXPORT GetALlSymbols : public OutputSectionIteratorPlugin {

public:
  GetALlSymbols() : OutputSectionIteratorPlugin("GETOUTPUT") {}

  // Perform any initialization here
  void Init(std::string Options) override {}

  void processOutputSection(OutputSection O) override {}

  // After the linker lays out the image, but before it creates the elf file,
  // it will call this run function.
  Status Run(bool Trace) override {
    if (getLinker()->getState() != LinkerWrapper::AfterLayout)
      return SUCCESS;
    eld::Expected<std::vector<eld::plugin::Symbol>> expAllSyms =
        getLinker()->getAllSymbols();
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expAllSyms);
    for (auto S : expAllSyms.value())
      std::cout << S.getName() << "\n";
    return SUCCESS;
  }

  void foo() {}

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "GETSYMBOLS"; }
};

std::unordered_map<std::string, Plugin *> Plugins;

Plugin *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new GetALlSymbols();
  return true;
}
PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }

void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}
