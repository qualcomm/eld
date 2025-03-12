#include "TarWriter.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginVersion.h"

using namespace eld::plugin;

class DLL_A_EXPORT TarWriterTestPlugin : public OutputSectionIteratorPlugin {

public:
  TarWriterTestPlugin() : OutputSectionIteratorPlugin("TARWRITER") {}

  // Perform any initialization here
  void Init(std::string Options) override {
    if (getLinker()->getState() != LinkerWrapper::AfterLayout)
      return;

    // Create tar file and add files to it.
    std::string tarFileName = "testTar.tar";
    auto ExpectedTW = getLinker()->getTarWriter(tarFileName);
    if (ExpectedTW) {
      auto diagID = getLinker()->getNoteDiagID("Initialized Tar File %0");
      getLinker()->reportDiag(diagID, tarFileName);
      std::string fileContent1 = "Test Content 1";
      // isNullTerminated true.
      auto Buffer1 = eld::plugin::MemoryBuffer::getBuffer(
          "TestFile1.txt", reinterpret_cast<const uint8_t *>(&fileContent1[0]),
          fileContent1.length(), true);
      ExpectedTW->addBufferToTar(Buffer1.value());
      std::string fileContent2 = "Name : Test Content 2";
      // isNullTerminated true.
      auto Buffer2 = eld::plugin::MemoryBuffer::getBuffer(
          "TestFile2.json", reinterpret_cast<const uint8_t *>(&fileContent2[0]),
          fileContent2.length(), true);
      ExpectedTW->addBufferToTar(Buffer2.value());
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

  std::string GetName() override { return "TARWRITER"; }
};

Plugin *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new TarWriterTestPlugin();
  return true;
}
PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }

void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}
