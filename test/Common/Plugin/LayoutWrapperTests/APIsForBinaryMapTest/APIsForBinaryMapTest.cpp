#include "LayoutWrapper.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginVersion.h"
#include <iostream>

using namespace eld::plugin;

class DLL_A_EXPORT APIsForBinaryMapTestPlugin
    : public OutputSectionIteratorPlugin {

public:
  APIsForBinaryMapTestPlugin() : OutputSectionIteratorPlugin("APIS") {}

  // Perform any initialization here
  void Init(std::string Options) override {
    auto linker = getLinker();
    if (linker->getState() != LinkerWrapper::AfterLayout)
      return;
    std::cout << "\nMap File: " << linker->getLinkerConfig().getMapFileName();
    auto Sym = linker->getSymbol("foo").value();
    std::cout << "\nName: " << Sym.getName();
    std::cout << "\nObject Type: " << (Sym.isObject() ? "Yes" : "No");
    std::cout << "\nFile Type: " << (Sym.isFile() ? "Yes" : "No");
    std::cout << "\nNoType Type: " << (Sym.isNoType() ? "Yes" : "No");
    std::cout << "\nFunction Type: " << (Sym.isFunction() ? "Yes" : "No");
    auto input = Sym.getInputFile();
    std::cout << "\nPath: " << input.decoratedPath();
    std::cout << "\nReal Path: " << input.getRealPath();
  }

  void processOutputSection(OutputSection O) override {}

  // After the linker lays out the image, but before it creates the elf file,
  // it will call this run function.
  Status Run(bool Trace) override { return Plugin::Status::SUCCESS; }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "APIS"; }
};

Plugin *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new APIsForBinaryMapTestPlugin();
  return true;
}
PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }

void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}
