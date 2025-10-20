#include "Expected.h"
#include "LinkerWrapper.h"
#include "PluginVersion.h"
#include "SectionIteratorPlugin.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <unordered_map>

using namespace eld::plugin;

class DLL_A_EXPORT UpdateSymbolPlugin : public SectionIteratorPlugin {

public:
  UpdateSymbolPlugin() : SectionIteratorPlugin("UPDATESYMBOL") {}

  // Perform any initialization here
  void Init(std::string Options) override {}

  void processSection(Section S) override {}

  // After the linker lays out the image, but before it creates the elf file,
  // it will call this run function.
  Status Run(bool Trace) override {
    return eld::plugin::Plugin::Status::SUCCESS;
  }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "UPDATESYMBOL"; }
};

class DLL_A_EXPORT UpdateSymbolPluginConfig : public LinkerPluginConfig {
public:
  UpdateSymbolPluginConfig(UpdateSymbolPlugin *P)
      : LinkerPluginConfig(P), P(P) {}

  void Init() override {
    std::string b22pcrel = "R_HEX_B22_PCREL";
    eld::Expected<void> expRegReloc =
        P->getLinker()->registerReloc(getRelocationType(b22pcrel));
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(P->getLinker(), expRegReloc);
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
    if (!P->getLinker()->isLinkStateBeforeLayout())
      return;
    std::cerr << "Got a callback for " << getRelocationName(U.getType())
              << " Payload : " << U.getName() << "\n";
    std::cerr << getPath(U.getTargetChunk().getInputFile()) << "\t"
              << getPath(U.getSourceChunk().getInputFile()) << "\t"
              << U.getOffsetInChunk() << "\n";
    if (U.getName() == "bar")
      changeSymbol(U);
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

  void changeSymbol(eld::plugin::Use &U) {
    eld::Expected<Symbol> S = P->getLinker()->getSymbol("baz");
    U.resetSymbol(*S);
  }

private:
  UpdateSymbolPlugin *P;
  std::mutex Mutex;
};

std::unordered_map<std::string, Plugin *> Plugins;

UpdateSymbolPlugin *ThisPlugin = nullptr;
eld::plugin::LinkerPluginConfig *ThisPluginConfig = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new UpdateSymbolPlugin();
  ThisPluginConfig = new UpdateSymbolPluginConfig(ThisPlugin);
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
