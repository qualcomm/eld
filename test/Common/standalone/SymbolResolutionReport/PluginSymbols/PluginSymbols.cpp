#include "OutputSectionIteratorPlugin.h"
#include "PluginVersion.h"

using namespace eld::plugin;

class DLL_A_EXPORT PluginSymbols : public OutputSectionIteratorPlugin {

public:
  PluginSymbols() : OutputSectionIteratorPlugin("PluginSymbols") {}

  // Perform any initialization here
  void Init(std::string Options) override {}

  void processOutputSection(OutputSection O) override {
    return;
  }

  Status Run(bool Trace) override {
    if (getLinker()->getState() != LinkerWrapper::CreatingSections)
      return eld::plugin::Plugin::Status::SUCCESS;

    eld::Expected<eld::plugin::OutputSection> expFooSect =
        getLinker()->getOutputSection("foo");
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expFooSect);
    eld::plugin::OutputSection fooSect = expFooSect.value();

    eld::plugin::LinkerScriptRule fooRule;
    fooRule = fooSect.getLinkerScriptRules().front();
    eld::plugin::Chunk fooChunk = fooRule.getChunks().front();
    eld::Expected<void> expAddStartSym =
        getLinker()->addSymbolToChunk(fooChunk, "start_of_foo", 0);
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expAddStartSym);
    eld::Expected<void> expAddEndSym = getLinker()->addSymbolToChunk(
        fooChunk, "end_of_foo", fooChunk.getSize());
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expAddEndSym);
    return Plugin::Status::SUCCESS;
  }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "PluginSymbols"; }

private:
  std::vector<OutputSection> OutputSections;
};

std::unordered_map<std::string, Plugin *> Plugins;

Plugin *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new PluginSymbols();
  return true;
}
PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }

void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}
