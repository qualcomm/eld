#include "OutputSectionIteratorPlugin.h"
#include "PluginVersion.h"
#include "TarWriter.h"
#include <cstring>

using namespace eld::plugin;

class DLL_A_EXPORT TarWriterNotNullTerminatedTestPlugin
    : public OutputSectionIteratorPlugin {

public:
  TarWriterNotNullTerminatedTestPlugin()
      : OutputSectionIteratorPlugin("TarWriterNotNullTerminatedTest") {}

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
      // Non null terminating payload.
      uint32_t data = 1234;
      uint8_t contents[4];
      std::memcpy(contents, &data, sizeof(uint32_t));
      auto Buffer1 = eld::plugin::MemoryBuffer::getBuffer(
          "NonStringTestFile", &contents[0], sizeof(uint32_t), false);
      ExpectedTW->addBufferToTar(Buffer1.value());
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

  std::string GetName() override { return "TarWriterNotNullTerminatedTest"; }
};

Plugin *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new TarWriterNotNullTerminatedTestPlugin();
  return true;
}
PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }

void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}
