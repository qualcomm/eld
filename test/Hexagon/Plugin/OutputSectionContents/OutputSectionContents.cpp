#include "LinkerWrapper.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginVersion.h"
#include <iostream>

using namespace eld::plugin;

class DLL_A_EXPORT OutputSectionContents : public OutputSectionIteratorPlugin {
  std::vector<OutputSection> Sections;

public:
  OutputSectionContents()
      : OutputSectionIteratorPlugin("OutputSectionContents") {}

  // Perform any initialization here
  void Init(std::string Options) override {}

  void processOutputSection(OutputSection O) override {
    if (getLinker()->getState() != LinkerWrapper::AfterLayout)
      return;
    if (O.getName() == ".rodata" || O.getName() == ".buffer")
      Sections.push_back(O);
  }

  // After the linker lays out the image, but before it creates the elf file,
  // it will call this run function.
  Status Run(bool Trace) override {
    if (getLinker()->getState() != LinkerWrapper::AfterLayout)
      return {};
    for (OutputSection &O : Sections) {
      auto ExpContents = getLinker()->getOutputSectionContents(O);
      ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), ExpContents);
      auto *Contents = reinterpret_cast<const char *>(ExpContents->get());
      std::cerr << std::string(Contents, O.getSize()) << "\n";
    }
    return {};
  }

  void foo() {}

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "GETOUTPUT"; }
};

std::unordered_map<std::string, Plugin *> Plugins;

Plugin *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new OutputSectionContents();
  return true;
}
PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }

void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}
