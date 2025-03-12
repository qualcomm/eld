#include "ELD/PluginAPI/PluginVersion.h"
#include "ELD/PluginAPI/SectionIteratorPlugin.h"
#include <string>
#include <vector>

class ExcludeSymbols : public eld::plugin::SectionIteratorPlugin {
public:
  ExcludeSymbols() : eld::plugin::SectionIteratorPlugin("ExcludeSymbols") {}

  // 'Init' callback hook can be used for initialization and preparations.
  void Init(std::string Options) override {
    m_SymbolsToRemove = {"foo", "fooagain", "bar", "baragain"};
  }

  // 'processSection' callback hook is called for each input section that is not
  // garbage-collected.
  void processSection(eld::plugin::Section O) override {}

  // 'Run' callback hook is called after 'processSection' callback hook calls.
  // It is called once for each section iterator plugin run.
  eld::plugin::Plugin::Status Run(bool trace) override {
    for (auto symName : m_SymbolsToRemove) {
      eld::plugin::Symbol S = Linker->getSymbol(symName);
      if (S)
        Linker->removeSymbolTableEntry(S);
    }
    return eld::plugin::Plugin::Status::SUCCESS;
  }

  // 'Destroy' callback hook can be used for finalization and clean-up tasks.
  // It is called once for each section iterator plugin run.
  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "Success"; }

  std::string GetName() override { return "ExcludeSymbols"; }

private:
  std::vector<std::string> m_SymbolsToRemove;
};

eld::plugin::Plugin *ThisPlugin = nullptr;

extern "C" {
bool RegisterAll() {
  ThisPlugin = new ExcludeSymbols{};
  return true;
}

eld::plugin::Plugin *getPlugin(const char *pluginName) { return ThisPlugin; }

void Cleanup() {
  if (!ThisPlugin)
    return;
  delete ThisPlugin;
  ThisPlugin = nullptr;
}
}