#include "LayoutWrapper.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginVersion.h"
#include <iostream>

using namespace eld::plugin;

class DLL_A_EXPORT PaddingTestPlugin : public OutputSectionIteratorPlugin {

public:
  PaddingTestPlugin() : OutputSectionIteratorPlugin("PADDING") {}

  // Perform any initialization here
  void Init(std::string Options) override {
    auto linker = getLinker();
    if (linker->getState() != LinkerWrapper::AfterLayout)
      return;
    std::unique_ptr<LayoutWrapper> layout =
        std::make_unique<LayoutWrapper>(*linker);
    eld::Expected<std::vector<eld::plugin::OutputSection>> expSections =
        getLinker()->getAllOutputSections();
    std::vector<eld::plugin::OutputSection> sections = std::move(expSections.value());
    for (auto s : sections) {
      std::vector<Padding> paddings = layout->getPaddings(s);
      for (auto &p : paddings) {
        std::cout << "\nOutput Section: " << s.getName();
        std::cout << "\nStart Offset: 0x" << std::hex << p.startOffset;
        std::cout << "\nSize: 0x" << std::hex << p.size;
        std::cout << "\nFill Value: 0x" << std::hex << p.fillValue;
        std::cout << "\nIs Alignment Padding: "
                  << (p.isAlignment ? "Yes" : "No");
      }
    }
  }

  void processOutputSection(OutputSection O) override {}

  // After the linker lays out the image, but before it creates the elf file,
  // it will call this run function.
  Status Run(bool Trace) override { return Plugin::Status::SUCCESS; }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "PADDING"; }
};

Plugin *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new PaddingTestPlugin();
  return true;
}
PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }

void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}
