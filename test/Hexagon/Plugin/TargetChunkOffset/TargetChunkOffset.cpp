#include "OutputSectionIteratorPlugin.h"
#include "PluginVersion.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <unordered_map>

using namespace eld::plugin;

class DLL_A_EXPORT TargetChunkPlugin : public OutputSectionIteratorPlugin {

public:
  TargetChunkPlugin() : OutputSectionIteratorPlugin("TARGETCHUNKOFFSET") {}

  // Perform any initialization here
  void Init(std::string Options) override {
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

  std::string GetName() override { return "TARGETCHUNKOFFSET"; }
};

class DLL_A_EXPORT TargetChunkPluginConfig : public LinkerPluginConfig {
public:
  TargetChunkPluginConfig(TargetChunkPlugin *P) : LinkerPluginConfig(P), P(P) {}

  void Init() override {
    std::string rodataReloc = "R_HEX_16_X";
    eld::Expected<void> expRegReloc =
        P->getLinker()->registerReloc(getRelocationType(rodataReloc));
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
        P->getLinker(), expRegReloc);
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
    std::string PayLoad = U.getTargetChunk().getRawData() + U.getTargetChunkOffset();
    std::cout << PayLoad << "\n";
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
  TargetChunkPlugin *P;
  std::mutex Mutex;
};

std::unordered_map<std::string, Plugin *> Plugins;

TargetChunkPlugin *ThisPlugin = nullptr;
eld::plugin::LinkerPluginConfig *ThisPluginConfig = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new TargetChunkPlugin();
  ThisPluginConfig = new TargetChunkPluginConfig(ThisPlugin);
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
