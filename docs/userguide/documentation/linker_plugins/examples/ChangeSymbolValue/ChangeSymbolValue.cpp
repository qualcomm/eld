#include "ELD/PluginAPI/OutputSectionIteratorPlugin.h"
#include "ELD/PluginAPI/PluginVersion.h"
#include <iostream>

class ChangeSymbolValue : public eld::plugin::OutputSectionIteratorPlugin {
public:
  ChangeSymbolValue()
      : eld::plugin::OutputSectionIteratorPlugin("ChangeSymbolValue") {}

  // 'Init' callback hook can be used for initialization and preparations.
  // This plugin does not need any initialization or preparation.
  void Init(std::string cfg) override {}

  // 'processOutputSection' callback hook is called once for each output
  // section.
  void processOutputSection(eld::plugin::OutputSection O) override {}

  // 'Run' callback hook is called after all the 'processSection' callback hook
  // calls.
  eld::plugin::Plugin::Status Run(bool trace) override {
    if (Linker->getState() != eld::plugin::LinkerWrapper::State::AfterLayout)
      return eld::plugin::Plugin::Status::SUCCESS;
    // Try to reset the 'HelloWorld' symbol value to the value of
    // 'HelloQualcomm' symbol.
    eld::plugin::Symbol helloWorldSymbol = Linker->getSymbol("HelloWorld");
    eld::plugin::Symbol helloQualcommSymbol = Linker->getSymbol("HelloQualcomm");
    bool resetSym =
        Linker->resetSymbol(helloWorldSymbol, helloQualcommSymbol.getChunk());
    if (resetSym)
      std::cout
          << "'HelloWorld' symbol value has been successfully reset to the "
          << "value of 'HelloQualcomm' symbol.\n";
    else
      std::cout << "Symbol value resetting failed for 'HelloWorld'.\n";

    // Try to reset the 'HelloWorldAgain' symbol value to the value of
    // 'HelloQualcommAgain' symbol.
    eld::plugin::Symbol helloWorldAgainSymbol = Linker->getSymbol("HelloWorldAgain");
    eld::plugin::Symbol helloQualcommAgainSymbol =
        Linker->getSymbol("HelloQualcommAgain");
    resetSym = Linker->resetSymbol(helloWorldAgainSymbol,
                                   helloQualcommAgainSymbol.getChunk());
    if (resetSym)
      std::cout << "'HelloWorldAgain' symbol value has been successfully reset "
                << "to the "
                << "value of 'HelloQualcommAgain' symbol.\n";
    else
      std::cout << "Symbol value resetting failed for 'HelloWorldAgain'.\n";
    return eld::plugin::Plugin::Status::SUCCESS;
  }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "Success"; }

  std::string GetName() override { return "ChangeSymbol"; }
};

eld::plugin::Plugin *ThisPlugin = nullptr;

extern "C" {
// RegisterAll should initialize all the plugins that a plugin library aims
// to provide. Linker calls this function before running any plugins provided
// by the library.
bool RegisterAll() {
  ThisPlugin = new ChangeSymbolValue{};
  return true;
}

// Linker calls this function to request an instance of the plugin
// with the plugin name pluginName. pluginName is provided in the plugin
// invocation command.
eld::plugin::Plugin *getPlugin(const char *pluginName) { return ThisPlugin; }

// Cleanup should free all the resources owned by a plugin library.
// Linker calls this function after all runs of the plugins provided
// by the library have completed.
void Cleanup() {
  if (!ThisPlugin)
    return;
  delete ThisPlugin;
  ThisPlugin = nullptr;
}
}