#include "LayoutWrapper.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginVersion.h"
#include <iostream>

using namespace eld::plugin;

class DLL_A_EXPORT LayoutHeaderTestPlugin : public OutputSectionIteratorPlugin {

public:
  LayoutHeaderTestPlugin() : OutputSectionIteratorPlugin("LAYOUTHEADER") {}

  // Perform any initialization here
  void Init(std::string Options) override {
    auto linker = getLinker();
    if (linker->getState() != LinkerWrapper::AfterLayout)
      return;

    std::unique_ptr<LayoutWrapper> layout =
        std::make_unique<LayoutWrapper>(*linker);
    auto &header = layout->getMapHeader();
    const eld::plugin::LinkerConfig &config = linker->getLinkerConfig();
    std::cout << "Vendor: " << header.getVendorName() << "\n";
    std::cout << "Vendor Version: " << header.getVendorVersion() << "\n";
    std::cout << "ABI Page Size: " << layout->getABIPageSize() << "\n";
    std::cout << "Emulation: " << layout->getTargetEmulation() << "\n";
    std::cout << "Max GP Size: " << config.getMaxGPSize() << "\n";
    std::cout << "Commandline: " << config.getLinkerCommandline() << "\n";
    std::cout << "Link Launch Directory: " << config.getLinkLaunchDirectory();
  }

  void processOutputSection(OutputSection O) override {}

  // After the linker lays out the image, but before it creates the elf file,
  // it will call this run function.
  Status Run(bool Trace) override { return Plugin::Status::SUCCESS; }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "LAYOUTHEADER"; }
};

Plugin *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new LayoutHeaderTestPlugin();
  return true;
}
PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }

void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}
