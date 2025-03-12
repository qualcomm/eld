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

class InputChunk {
public:
  InputChunk(Chunk S) : Chunk(S) {}
  Chunk Chunk;
};

class DLL_A_EXPORT OSIter : public OutputSectionIteratorPlugin {

public:
  OSIter()
      : OutputSectionIteratorPlugin("GETOUTPUTCHUNKS"),
        RedistributeSection(nullptr), Hot(nullptr), Cold(nullptr),
        Unlikely(nullptr) {}

  // Perform any initialization here
  void Init(std::string Options) override {}

  void processOutputSection(OutputSection O) override {
    if (getLinker()->getState() == LinkerWrapper::AfterLayout) {
      std::cout << "Size of " << O.getName() << "\t" << O.getSize() << "\n";
      return;
    }
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

  size_t getSize(std::vector<Chunk> &C) {
    size_t Size = 0;
    for (auto Chunk : C) {
      // Adjust the current size to the required alignment if needed
      uint64_t Align = Chunk.getAlignment();
      if (Align > 1) {
        assert(!(Align & (Align - 1)) && "Alignment is not a power of two!");
        Size = (Size + Align - 1) & ~(Align - 1);
      }
      Size += Chunk.getSize();
    }
    return Size;
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
    eld::Expected<Chunk> expC1 = getLinker()->createPaddingChunk(4, 64);
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expC1);
    Chunk C1 = std::move(expC1.value());

    eld::Expected<Chunk> expC2 = getLinker()->createPaddingChunk(4, 128);
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expC2);
    Chunk C2 = std::move(expC2.value());

    eld::Expected<Chunk> expC3 = getLinker()->createPaddingChunk(4, 256);
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expC3);
    Chunk C3 = std::move(expC3.value());
    for (auto &R : RedistributeSection.getLinkerScriptRules()) {
      std::vector<Chunk> AllChunks = R.getChunks();
      for (auto &C : AllChunks) {
        std::string ChunkName = C.getName();
        if (!getLinker()->isChunkMovableFromOutputSection(C))
          continue;
        if (ChunkName.find("hot") != std::string::npos) {
          HotChunks.push_back(C);
          eld::Expected<void> expRemoveChunk = getLinker()->removeChunk(R, C);
          ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expRemoveChunk);
        }
        if (ChunkName.find("cold") != std::string::npos) {
          ColdChunks.push_back(C);
          eld::Expected<void> expRemoveChunk = getLinker()->removeChunk(R, C);
          ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expRemoveChunk);
        }
        if (ChunkName.find("unlikely") != std::string::npos) {
          UnlikelyChunks.push_back(C);
          eld::Expected<void> expRemoveChunk = getLinker()->removeChunk(R, C);
          ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expRemoveChunk);
        }
      }
    }
    HotChunks.push_back(C1);
    ColdChunks.push_back(C2);
    UnlikelyChunks.push_back(C3);
    std::cerr << HotRule.asString() << "\t" << getSize(HotChunks) << "\n";
    std::cerr << ColdRule.asString() << "\t" << getSize(ColdChunks) << "\n";
    std::cerr << UnlikelyRule.asString() << "\t" << getSize(UnlikelyChunks)
              << "\n";
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

  std::string GetName() override { return "GETOUTPUTCHUNKS"; }

private:
  eld::plugin::OutputSection RedistributeSection;
  eld::plugin::OutputSection Hot;
  eld::plugin::OutputSection Cold;
  eld::plugin::OutputSection Unlikely;
  std::unordered_map<std::string, std::vector<InputChunk>> OutputSectionInfo;
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
