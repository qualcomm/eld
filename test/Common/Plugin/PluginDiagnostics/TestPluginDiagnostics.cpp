#include "PluginVersion.h"
#include "SectionIteratorPlugin.h"
#include <string>

using namespace eld::plugin;

// All plugins must be derived from one of the linker defined plugins.
// In this case it is the SectionIteratorPlugin defined in
// "SectionIteratorPlugin.h"
class DLL_A_EXPORT PluginDiagnostics : public SectionIteratorPlugin {

public:
  // This constructor takes no arguments;
  // the base class will take the name we will call this plugin
  PluginDiagnostics() : SectionIteratorPlugin("PluginDiagnostics") {}

  // This init doesn't do anything, but we could add code here
  void Init(std::string Options) override {}

  // This function will be called whenever the linker processes a section.
  // We will add this section to our vector of sections for later use.
  void processSection(eld::plugin::Section S) override {}

  // After the linker lays out the image, but before it creates the elf file,
  // it will call this run function.
  Status Run(bool Trace) override {
    auto noteID = getLinker()->getNoteDiagID("Test note diagnostic!");
    getLinker()->reportDiag(noteID);
    auto errorID = getLinker()->getErrorDiagID("Error disguised as note!");
    getLinker()->reportDiagEntry(std::make_unique<eld::plugin::DiagnosticEntry>(
        NoteDiagnosticEntry(errorID, {})));
    return Plugin::Status::SUCCESS;
  }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "PluginDiagnostics"; }

private:
  std::vector<eld::plugin::Section> m_Sections;
};

Plugin *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new PluginDiagnostics();
  return true;
}
PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }
void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
}
}
