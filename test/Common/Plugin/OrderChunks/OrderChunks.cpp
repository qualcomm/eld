#include "OutputSectionIteratorPlugin.h"
#include "PluginVersion.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <string>
#include <unordered_map>

using namespace eld::plugin;

// All plugins must be derived from one of the linker defined plugins.
// In this case it is the OutputSectionIteratorPlugin defined in
// "OutputSectionIteratorPlugin.h"
class DLL_A_EXPORT OrderChunksPlugin : public OutputSectionIteratorPlugin {

public:
  // This constructor takes one argument: Chunks() returns pieces of a section,
  // including the section name; the base class will take the name we will call
  // this plugin
  OrderChunksPlugin()
      : OutputSectionIteratorPlugin("ORDERBLOCKS"),
        ReorderOutputSection(nullptr) {}

  // Perform any initialization here
  void Init(std::string Options) override {}

  // This function will be called whenever the linker processes a OutputSection.
  void processOutputSection(OutputSection O) override {
    if (getLinker()->getState() != LinkerWrapper::CreatingSections)
      return;
    if (O.getName() != ".reordersection")
      return;
    ReorderOutputSection = O;
  }

  // After the linker lays out the image, but before it creates the elf file,
  // it will call this run function.
  Status Run(bool Trace) override {
    if (getLinker()->getState() != LinkerWrapper::CreatingSections)
      return Plugin::Status::SUCCESS;

    if (!ReorderOutputSection.getOutputSection())
      return Plugin::Status::SUCCESS;

    LinkerScriptRule DefaultRule =
        ReorderOutputSection.getLinkerScriptRules().back();

    std::vector<Chunk> AllChunks;
    for (auto &LSR : ReorderOutputSection.getLinkerScriptRules()) {
      std::vector<Chunk> CS = LSR.getChunks();
      for (auto &C : CS) {
        AllChunks.push_back(C);
        getLinker()->removeChunk(LSR, C);
      }
    }

    // This loop will iterate through all of the sections we have added to our
    // vector. Each pair of sections is reordered alphabetically by section
    // name. We will do all of the sorting in this loop.
    std::sort(
        AllChunks.begin(), AllChunks.end(), [](const Chunk &A, const Chunk &B) {
          return A.getInputFile().getOrdinal() < B.getInputFile().getOrdinal();
        });

    getLinker()->updateChunks(DefaultRule, AllChunks);

    // We could track a private variable for return status, but in this example
    // we always return success
    return Plugin::Status::SUCCESS;
  }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "ORDERBLOCKS"; }

private:
  OutputSection ReorderOutputSection;
};

std::unordered_map<std::string, Plugin *> Plugins;

Plugin *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new OrderChunksPlugin();
  return true;
}
PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }

void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
}
}
