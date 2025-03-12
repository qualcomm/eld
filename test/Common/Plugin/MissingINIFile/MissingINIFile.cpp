#include "DiagnosticEntry.h"
#include "Diagnostics.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginVersion.h"
#include <string>

#ifdef _WIN32
#define DLL_EXPORT __declspec(dllexport)
#else
#define DLL_EXPORT
#endif

using namespace eld::plugin;

class DLL_EXPORT MissingINIFile : public OutputSectionIteratorPlugin {

public:
  MissingINIFile() : OutputSectionIteratorPlugin("MissingINIFile") {}

  eld::Expected<INIFile> findConfigFile(const std::string &configFile) {
    eld::Expected<eld::plugin::INIFile> readFile =
        getLinker()->readINIFile(configFile);
    if (!readFile) {
      if (readFile.error()->diagID() ==
          eld::plugin::Diagnostic::error_file_does_not_exist())
        return std::make_unique<eld::plugin::DiagnosticEntry>(
            eld::plugin::FatalDiagnosticEntry(readFile.error()->diagID(),
                                       readFile.error()->args()));
    }
    return readFile;
  }

  // Perform any initialization here
  void Init(std::string Options) override {
    eld::Expected<eld::plugin::INIFile> readFile = findConfigFile("someFile.ini");
    if (!readFile) {
      DiagnosticEntry::Severity severity = readFile.error()->severity();
      getLinker()->reportDiagEntry(std::move(readFile.error()));
      if (severity > DiagnosticEntry::Severity::Warning)
        getLinker()->setLinkerFatalError();
    }
  }

  void processOutputSection(OutputSection O) override { return; }

  // After the linker lays out the image, but before it creates the elf file,
  // it will call this run function.
  Status Run(bool Trace) override { return Plugin::Status::SUCCESS; }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "MissingINIFile"; }
};

MissingINIFile *ThisPlugin = nullptr;

extern "C" {
bool DLL_EXPORT RegisterAll() {
  ThisPlugin = new MissingINIFile();
  return true;
}

Plugin DLL_EXPORT *getPlugin(const char *T) { return ThisPlugin; }

void DLL_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
}
}
