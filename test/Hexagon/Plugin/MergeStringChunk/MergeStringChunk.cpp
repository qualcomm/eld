#include "Defines.h"
#include "LinkerScript.h"
#include "LinkerWrapper.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginADT.h"
#include "PluginVersion.h"
#include <cassert>
#include <iostream>

class DLL_A_EXPORT MergeStringChunkReader
    : public eld::plugin::OutputSectionIteratorPlugin {
public:
  MergeStringChunkReader()
      : eld::plugin::OutputSectionIteratorPlugin("MergeStringChunkReader") {}

  void Init(std::string options) override {}

  void processOutputSection(eld::plugin::OutputSection O) override {
    if (getLinker()->getState() != eld::plugin::LinkerWrapper::CreatingSections)
      return;
    if (O.getName() == ".rodata")
      Rodata = O;
  }

  Status Run(bool trace) override {
    if (getLinker()->getState() != eld::plugin::LinkerWrapper::CreatingSections)
      return Status::SUCCESS;

    auto Rules = Rodata.getLinkerScriptRules();
    for (auto &Rule : Rules) {
      for (auto &C : Rule.getChunks()) {
        if (!C.isMergeableString())
          continue;
        for (eld::plugin::MergeableString S :
             static_cast<eld::plugin::MergeStringChunk *>(&C)->getStrings())
          // string size inputoffset hasoutputoffset merged sectionname
          std::cout << S.getString() << " " << S.getSize() << " "
                    << S.getInputOffset() << " " << S.hasOutputOffset() << " "
                    << S.isMerged() << " " << C.getName() << "\n";
      }
    }
    return Status::SUCCESS;
  }

  std::string GetName() override { return "MergeStringChunkReader"; }

  std::string GetLastErrorAsString() override { return "Success"; }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

private:
  eld::plugin::OutputSection Rodata{nullptr};
};

MergeStringChunkReader *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new MergeStringChunkReader();
  return true;
}

eld::plugin::PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }

void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}