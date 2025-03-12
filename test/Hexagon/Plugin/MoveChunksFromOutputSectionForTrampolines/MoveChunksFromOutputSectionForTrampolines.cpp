#include "LinkerWrapper.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginVersion.h"
#include <algorithm>
#include <assert.h>
#include <cstring>
#include <iostream>
#include <string>
#include <unordered_map>

using namespace eld::plugin;

class DLL_A_EXPORT OSIter : public OutputSectionIteratorPlugin {

public:
  OSIter()
      : OutputSectionIteratorPlugin("MOVECHUNKSFORTRAMPOLINES"),
        RedistributeSection(nullptr), Hot(nullptr), Cold(nullptr),
        Unlikely(nullptr) {}

  // Perform any initialization here
  void Init(std::string Options) override {}

  void processOutputSection(OutputSection O) override {
    if (getLinker()->getState() != LinkerWrapper::CreatingSections)
      return;

    if (O.getName() == ".redistribute")
      RedistributeSection = O;

    if (O.getName() == ".hot")
      Hot = O;

    if (O.getName() == ".cold")
      Cold = O;

    if (O.getName() == ".unlikely")
      Unlikely = O;
  }

  // After the linker lays out the image, but before it creates the elf file,
  // it will call this run function.
  Status Run(bool Trace) override {
    if (getLinker()->getState() == LinkerWrapper::AfterLayout)
      return eld::plugin::Plugin::Status::SUCCESS;
    if (getLinker()->getState() != LinkerWrapper::CreatingSections)
      return eld::plugin::Plugin::Status::SUCCESS;

    eld::plugin::LinkerScriptRule HotRule = Hot.getLinkerScriptRules().front();
    eld::plugin::LinkerScriptRule ColdRule = Cold.getLinkerScriptRules().front();
    eld::plugin::LinkerScriptRule UnlikelyRule =
        Unlikely.getLinkerScriptRules().front();
    std::vector<Chunk> HotChunks, ColdChunks, UnlikelyChunks;
    bool coldChunkSeen = false;
    for (auto &R : RedistributeSection.getLinkerScriptRules()) {
      std::vector<Chunk> AllChunks = R.getChunks();
      for (auto &C : AllChunks) {
        std::string ChunkName = C.getName();
        if (!getLinker()->isChunkMovableFromOutputSection(C))
          continue;
        // Move hot to hot sections
        if (ChunkName.find("hot") != std::string::npos) {
          HotChunks.push_back(C);
          eld::Expected<void> expRemoveChunk = getLinker()->removeChunk(R, C);
          ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expRemoveChunk);
        }
        // Move the first cold to .cold
        if ((ChunkName.find("cold") != std::string::npos) && !coldChunkSeen) {
          coldChunkSeen = true;
          ColdChunks.push_back(C);
          eld::Expected<void> expRemoveChunk = getLinker()->removeChunk(R, C);
          ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expRemoveChunk);
          continue;
        }
        // Move reset of cold to unlikely
        if (ChunkName.find("cold") != std::string::npos) {
          UnlikelyChunks.push_back(C);
          eld::Expected<void> expRemoveChunk = getLinker()->removeChunk(R, C);
          ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expRemoveChunk);
        }
      }
    }
    eld::Expected<void> expUpdateHotChunks =
        getLinker()->updateChunks(HotRule, HotChunks);
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expUpdateHotChunks);
    eld::Expected<void> expUpdateColdChunks =
        getLinker()->updateChunks(ColdRule, ColdChunks);
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expUpdateColdChunks);
    eld::Expected<void> expUpdateUnlikelyChunks =
        getLinker()->updateChunks(UnlikelyRule, UnlikelyChunks);
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(),
                                            expUpdateUnlikelyChunks);

    return Plugin::Status::SUCCESS;
  }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "MOVECHUNKSFORTRAMPOLINES"; }

private:
  eld::plugin::OutputSection RedistributeSection;
  eld::plugin::OutputSection Hot;
  eld::plugin::OutputSection Cold;
  eld::plugin::OutputSection Unlikely;
};

std::unordered_map<std::string, Plugin *> Plugins;

Plugin *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new OSIter();
  return true;
}
PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }

void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}
