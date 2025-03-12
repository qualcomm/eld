#include "OutputSectionIteratorPlugin.h"
#include "PluginVersion.h"
#include "TarWriter.h"
#include <fstream>

using namespace eld::plugin;

class DLL_A_EXPORT TarWriterOverwriteTestPlugin
    : public OutputSectionIteratorPlugin {

public:
  TarWriterOverwriteTestPlugin()
      : OutputSectionIteratorPlugin("TarWriterOverwriteTest") {}

  // Perform any initialization here
  void Init(std::string Options) override {
    if (getLinker()->getState() != LinkerWrapper::AfterLayout)
      return;

    // Create tar file.
    std::string tarFileName = "testTar.tar";
    auto ExpectedTWOld = getLinker()->getTarWriter(tarFileName);
    if (ExpectedTWOld) {
      auto diagID = getLinker()->getNoteDiagID("Initialized Tar File %0");
      getLinker()->reportDiag(diagID, tarFileName);
    } else {
      getLinker()->reportDiagEntry(std::move(ExpectedTWOld.error()));
    }
    // Create new tar over the old tar file.
    auto ExpectedTWNew = getLinker()->getTarWriter(tarFileName);
    if (ExpectedTWNew) {
      auto diagID = getLinker()->getNoteDiagID("Initialized Tar File %0");
      getLinker()->reportDiag(diagID, tarFileName);
    } else {
      getLinker()->reportDiagEntry(std::move(ExpectedTWNew.error()));
    }

    // Create a sample file to overwrite.
    std::string FileName = "testFile";
    std::ofstream testFile(FileName);
    testFile << "my text here!" << std::endl;
    testFile.close();

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

  std::string GetName() override { return "TarWriterOverwriteTest"; }
};

Plugin *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new TarWriterOverwriteTestPlugin();
  return true;
}
PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }

void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}
