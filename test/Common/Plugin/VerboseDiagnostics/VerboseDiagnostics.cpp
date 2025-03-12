#include "DiagnosticEntry.h"
#include "LinkerWrapper.h"
#include "PluginVersion.h"
#include "SectionIteratorPlugin.h"

using namespace eld::plugin;

class DLL_A_EXPORT VerboseDiagnostics : public SectionIteratorPlugin {
public:
  VerboseDiagnostics() : SectionIteratorPlugin("VerboseDiagnostics") {}

  void Init(std::string Options) override {
    DiagnosticEntry::DiagIDType verboseDiagID =
        getLinker()->getVerboseDiagID("Verbose init diag!");
    DiagnosticEntry::DiagIDType noteDiagID =
        getLinker()->getNoteDiagID("Note init diag!");
    if (getLinker()->isVerbose())
      getLinker()->reportDiag(verboseDiagID);
    getLinker()->reportDiag(noteDiagID);
  }

  void processSection(eld::plugin::Section S) override { }

  Status Run(bool Trace) override {
    return Plugin::Status::SUCCESS;
  }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "VerboseDiagnostics"; }

private:
  std::vector<eld::plugin::Section> m_Sections;
};

std::unordered_map<std::string, Plugin *> Plugins;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  Plugins["VerboseDiagnostics"] = new VerboseDiagnostics();
  return true;
}
PluginBase DLL_A_EXPORT *getPlugin(const char *T) {
  return Plugins[std::string(T)];
}
void DLL_A_EXPORT Cleanup() {
  for (auto &A : Plugins)
    delete A.second;
  Plugins.clear();
}
}
