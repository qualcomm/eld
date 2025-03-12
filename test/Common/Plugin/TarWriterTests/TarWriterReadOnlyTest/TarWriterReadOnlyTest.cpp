#include "OutputSectionIteratorPlugin.h"
#include "PluginVersion.h"
#include "TarWriter.h"

using namespace eld::plugin;

class DLL_A_EXPORT TarWriterReadOnlyTestPlugin
    : public OutputSectionIteratorPlugin {

public:
  TarWriterReadOnlyTestPlugin()
      : OutputSectionIteratorPlugin("TarWriterReadOnlyTest") {}

  // Perform any initialization here
  void Init(std::string Options) override {
    if (getLinker()->getState() != LinkerWrapper::AfterLayout)
      return;

    std::string FileName = "Inputs/testTar.tar";
    // Create new tar over the existing test file.
    auto ExpectedTW = getLinker()->getTarWriter(FileName);
    if (ExpectedTW) {
      auto diagID = getLinker()->getNoteDiagID("Initialized Tar File %0");
      getLinker()->reportDiag(diagID, FileName);
    } else {
      getLinker()->reportDiagEntry(std::move(ExpectedTW.error()));
    }
  }

  void processOutputSection(OutputSection O) override {}

  // After the linker lays out the image, but before it creates the elf file,
  // it will call this run function.
  Status Run(bool Trace) override { return Plugin::Status::SUCCESS; }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "TarWriterReadOnlyTest"; }
};

Plugin *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new TarWriterReadOnlyTestPlugin();
  return true;
}
PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }

void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}
