#include "OutputSectionIteratorPlugin.h"
#include "PluginVersion.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/FileSystem.h"
#include <cstring>
#include <iostream>
#include <string>
#include <unordered_map>

using namespace eld::plugin;

class DLL_A_EXPORT ConfigFilePlugin : public OutputSectionIteratorPlugin {
public:
  ConfigFilePlugin() : OutputSectionIteratorPlugin("ConfigFile") {}

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
    std::string configPath =
        expConfigPath.has_value() ? expConfigPath.value() : "";
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
    llvm::SmallString<260> currentPath;
    llvm::sys::fs::current_path(currentPath);
    getLinker()->writeINIFile(newFile, currentPath.str().str() +
                                      "/NewINIFolder/new_plugin.ini");
    if (CheckError(newFile.getErrorCode()))
      return;
    eld::Expected<std::string> expNewINIFilePath =
        getLinker()->findConfigFile("new_plugin.ini");
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
        getLinker(), expNewINIFilePath);
    std::string newINIFilePath = expNewINIFilePath.value();
    eld::Expected<eld::plugin::INIFile> readNewFile =
        getLinker()->readINIFile(newINIFilePath);
    if (!readNewFile) {
      std::cout << "unable to read new plugin INI file\n";
      return;
    }
    INIFile newINIFile = std::move(readNewFile.value());
    if (newINIFile.containsSection("NEW"))
      std::cout << "found section NEW\n";
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

  std::string GetName() override { return "ConfigFile"; }

  void processOutputSection(OutputSection o) override {}

private:
  INIFile::ErrorCode lastError;
};

Plugin *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new ConfigFilePlugin();
  return true;
}
PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }
void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
}
}
