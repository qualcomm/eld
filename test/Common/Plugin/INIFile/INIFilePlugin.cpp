#include "OutputSectionIteratorPlugin.h"
#include "PluginVersion.h"
#include <cstring>
#include <iostream>
#include <string>
#include <unordered_map>

using namespace eld::plugin;

class DLL_A_EXPORT INIFilePlugin : public OutputSectionIteratorPlugin {
public:
  INIFilePlugin() : OutputSectionIteratorPlugin("INIFile") {}

  /// print a message if there was an error
  /// return true if there was an error
  bool CheckError(INIFile::ErrorCode e) {
    lastError = e;
    if (lastError != INIFile::ErrorCode::Success) {
      std::cout << GetLastErrorAsString() << "\n";
      return true;
    }
    return false;
  }

  void Init(std::string Options) override {
    // read Inputs/test.ini
    eld::Expected<std::string> expConfigPath =
        getLinker()->findConfigFile(Options);
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
        getLinker(), expConfigPath);
    std::string configPath = expConfigPath.value();
    eld::Expected<eld::plugin::INIFile> readFile = getLinker()->readINIFile(configPath);
    if (!readFile)
      return;
    INIFile f = std::move(readFile.value());
    for (auto &i : f.getSections())
      std::cout << i << "\n";
    if (f.containsSection("A"))
      std::cout << "found section A\n";
    if (f.containsItem("A", "1"))
      std::cout << "found item A[1]\n";
    if (!f.containsSection("C"))
      std::cout << "did not find section C\n";
    std::cout << f.getValue("A", "1") << "\n";
    std::cout << f.getValue("B", "2") << "\n";
    std::cout << f.getLastErrorAsString() << "\n";

    // modify file, write it back as out.ini
    f.addSection("C");
    f.insert("C", "a", "1");
    getLinker()->writeINIFile(f, "out.ini");
    if (CheckError(f.getErrorCode()))
      return;
    // write to same file
    getLinker()->writeINIFile(f, "test.ini");
    if (CheckError(f.getErrorCode()))
      return;
    // write a totally new file
    eld::plugin::INIFile newFile;
    // check empty using bool operator
    if (!newFile)
      std::cout << "new file is empty\n";
    newFile.addSection("NEW");
    newFile.insert("NEW", "foo", "bar");
    getLinker()->writeINIFile(newFile, "new.ini");
    if (CheckError(newFile.getErrorCode()))
      return;
  }

  Status Run(bool Verbose) override { return Plugin::Status::SUCCESS; }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override {
    switch (lastError) {
    case INIFile::Success:
      return "Success";
      break;
    case INIFile::WriteError:
      return "Error writing file";
      break;
    case INIFile::ReadError:
      return "Error reading file";
      break;
    case INIFile::FileDoesNotExist:
      return "File does not exist";
      break;
    }
  }

  std::string GetName() override { return "INIFILE"; }

  void processOutputSection(OutputSection o) override {}

private:
  INIFile::ErrorCode lastError;
};

Plugin *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new INIFilePlugin();
  return true;
}
PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }
void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
}
}
