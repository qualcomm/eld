#include "Expected.h"
#include "LinkerPluginConfig.h"
#include "LinkerWrapper.h"
#include "PluginVersion.h"
#include "SectionIteratorPlugin.h"
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using namespace eld::plugin;

class RelocationCallbackMutationConfig;

class DLL_A_EXPORT RelocationCallbackMutationPlugin
    : public SectionIteratorPlugin {
public:
  RelocationCallbackMutationPlugin()
      : SectionIteratorPlugin("RELOCATIONCALLBACKMUTATION") {}

  void Init(std::string) override {}

  void processSection(Section S) override;

  void setConfig(RelocationCallbackMutationConfig *Config) {
    this->Config = Config;
  }

  Status Run(bool) override { return Status::SUCCESS; }
  void Destroy() override {}
  uint32_t GetLastError() override { return 0; }
  std::string GetLastErrorAsString() override { return "SUCCESS"; }
  std::string GetName() override { return "RELOCATIONCALLBACKMUTATION"; }

  Section getDiscardLater() const { return DiscardLater; }
  Section getAddLater() const { return AddLater; }

private:
  Section DiscardLater;
  Section AddLater;
  RelocationCallbackMutationConfig *Config = nullptr;
};

class DLL_A_EXPORT RelocationCallbackMutationConfig
    : public LinkerPluginConfig {
public:
  explicit RelocationCallbackMutationConfig(
      RelocationCallbackMutationPlugin *Plugin)
      : LinkerPluginConfig(Plugin), Plugin(Plugin) {
    Plugin->setConfig(this);
  }

  void Init() override {}

  void registerReloc(uint32_t Type) {
    eld::Expected<void> Result = Plugin->getLinker()->registerReloc(Type);
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(Plugin->getLinker(), Result);
  }

  void RelocCallBack(Use U) override {
    const std::string SourceName = U.getSourceChunk().getName();
    std::cerr << "callback " << SourceName << '\n';

    if (SourceName == ".discard.trigger") {
      Plugin->getDiscardLater().markAsDiscarded();
      return;
    }

    if (SourceName != ".add.trigger" || AddedRelocation)
      return;

    std::vector<Chunk> Chunks = Plugin->getAddLater().getChunks();
    if (Chunks.empty())
      return;

    eld::Expected<Symbol> Target = Plugin->getLinker()->getSymbol("target");
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(Plugin->getLinker(), Target);
    eld::Expected<Use> AddedUse = Plugin->getLinker()->createAndAddUse(
        Chunks.front(), 4, U.getType(), Target.value(), 0);
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(Plugin->getLinker(), AddedUse);
    AddedRelocation = true;
  }

private:
  RelocationCallbackMutationPlugin *Plugin;
  bool AddedRelocation = false;
};

void RelocationCallbackMutationPlugin::processSection(Section S) {
  if (S.getName() == ".discard.later")
    DiscardLater = S;
  else if (S.getName() == ".add.later")
    AddLater = S;

  eld::Expected<std::vector<Use>> Uses = getLinker()->getUses(S);
  ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), Uses);
  for (const Use &U : Uses.value())
    Config->registerReloc(U.getType());
}

RelocationCallbackMutationPlugin *PluginInstance = nullptr;
RelocationCallbackMutationConfig *ConfigInstance = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  PluginInstance = new RelocationCallbackMutationPlugin();
  ConfigInstance = new RelocationCallbackMutationConfig(PluginInstance);
  return true;
}

PluginBase DLL_A_EXPORT *getPlugin(const char *) { return PluginInstance; }

LinkerPluginConfig *getPluginConfig(const char *) { return ConfigInstance; }

void DLL_A_EXPORT Cleanup() {
  delete ConfigInstance;
  delete PluginInstance;
}
}
