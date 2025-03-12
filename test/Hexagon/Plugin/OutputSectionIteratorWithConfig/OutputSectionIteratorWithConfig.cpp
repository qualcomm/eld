#include "DiagnosticEntry.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginVersion.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <unordered_map>

using namespace eld::plugin;

class DLL_A_EXPORT OSIterPlugin : public OutputSectionIteratorPlugin {

public:
  OSIterPlugin() : OutputSectionIteratorPlugin("GETOUTPUTWITHCONFIG") {}

  // Perform any initialization here
  void Init(std::string Options) override {
    std::cout << getLinker()->getFileContents(Options) << "\n";
    eld::Expected<std::string> expConfigPath =
        getLinker()->findConfigFile("config.ini");
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
        getLinker(), expConfigPath);
    std::string configPath = expConfigPath.value();
    eld::Expected<eld::plugin::INIFile> readFile =
        getLinker()->readINIFile(configPath);
    if (!readFile) {
      getLinker()->reportDiagEntry(std::move(readFile.error()));
      return;
    }
    INIFile File = std::move(readFile.value());
    auto verbose = File.getValue("Options", "verbose");
    std::cout << verbose << "\n";
    auto numbers = File.getSection("Numbers");
    for (auto &x : numbers)
      std::cout << x.first << x.second << "\n";

    eld::Expected<std::string> expBadConfig =
        getLinker()->findConfigFile("badcharacters.ini");
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
        getLinker(), expBadConfig);
    std::string badConfig = expBadConfig.value();
    eld::Expected<eld::plugin::INIFile> readFileBad =
        getLinker()->readINIFile(badConfig);
    if (!readFileBad) {
      DiagnosticEntry::Severity severity = readFileBad.error()->severity();
      getLinker()->reportDiagEntry(std::move(readFileBad.error()));
      if (severity > DiagnosticEntry::Severity::Warning)
        getLinker()->setLinkerFatalError();
    }
  }

  void processOutputSection(OutputSection O) override {
    if (getLinker()->getState() == LinkerWrapper::BeforeLayout)
      return;

    if (getLinker()->getState() == LinkerWrapper::CreatingSections)
      return;

    if (getLinker()->getState() == LinkerWrapper::AfterLayout)
      return;
  }

  // After the linker lays out the image, but before it creates the elf file,
  // it will call this run function.
  Status Run(bool Trace) override {
    if (getLinker()->getState() == LinkerWrapper::BeforeLayout)
      return eld::plugin::Plugin::Status::SUCCESS;
    if (getLinker()->getState() == LinkerWrapper::AfterLayout)
      return eld::plugin::Plugin::Status::SUCCESS;
    if (getLinker()->getState() == LinkerWrapper::CreatingSections)
      return eld::plugin::Plugin::Status::SUCCESS;
    return Plugin::Status::SUCCESS;
  }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "GETOUTPUTWITHCONFIG"; }
};

class DLL_A_EXPORT OSIterPluginConfig : public LinkerPluginConfig {
public:
  OSIterPluginConfig(OSIterPlugin *P) : LinkerPluginConfig(P), P(P) {}

  void Init() override {
    std::string b22pcrel = "R_HEX_B22_PCREL";
    std::string abs32 = "R_HEX_32";
    eld::Expected<void> expRelRegReloc =
        P->getLinker()->registerReloc(getRelocationType(b22pcrel));
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
        P->getLinker(), expRelRegReloc);
    eld::Expected<void> expAbsRegReloc =
        P->getLinker()->registerReloc(getRelocationType(abs32), "foo");
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
        P->getLinker(), expAbsRegReloc);
  }

  void RelocCallBack(Use U) override {
    if (P->getLinker()->isMultiThreaded()) {
      std::lock_guard<std::mutex> M(Mutex);
      printMessage(U);
      return;
    }
    printMessage(U);
  }

private:
  void printMessage(Use U) {
    std::cerr << "Got a callback for " << getRelocationName(U.getType())
              << " Payload : " << U.getName() << "\n";
    std::cerr << getPath(U.getTargetChunk().getInputFile()) << "\t"
              << getPath(U.getSourceChunk().getInputFile()) << "\t"
              << U.getOffsetInChunk() << "\n";
  }

  uint32_t getRelocationType(std::string Name) {
    uint32_t rType =
        P->getLinker()->getRelocationHandler().getRelocationType(Name);
    return rType;
  }

  std::string getRelocationName(uint32_t Type) {
    return P->getLinker()->getRelocationHandler().getRelocationName(Type);
  }

  std::string getPath(InputFile I) const {
    std::string FileName = std::string(I.getFileName());
    if (I.isArchive())
      return FileName + "(" + I.getMemberName() + ")";
    return FileName;
  }

private:
  OSIterPlugin *P;
  std::mutex Mutex;
};

std::unordered_map<std::string, Plugin *> Plugins;

OSIterPlugin *ThisPlugin = nullptr;
eld::plugin::LinkerPluginConfig *ThisPluginConfig = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new OSIterPlugin();
  ThisPluginConfig = new OSIterPluginConfig(ThisPlugin);
  return true;
}

PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }

LinkerPluginConfig DLL_A_EXPORT *getPluginConfig(const char *T) {
  return ThisPluginConfig;
}

void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  if (ThisPluginConfig)
    delete ThisPluginConfig;
}
}
