#include "Defines.h"
#include "LinkerScript.h"
#include "LinkerWrapper.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginADT.h"
#include "PluginVersion.h"
#include <cassert>
#include <iostream>

class DLL_A_EXPORT StringChunkMover : public eld::plugin::OutputSectionIteratorPlugin {
public:
  StringChunkMover() : eld::plugin::OutputSectionIteratorPlugin("StringChunkMover") {}

  void Init(std::string Options) override {}

  void processOutputSection(eld::plugin::OutputSection O) override {
    if (getLinker()->getState() != eld::plugin::LinkerWrapper::CreatingSections)
      return;
    if (O.getName() == ".rodata")
      Rodata = O;
    else if (O.getName() == ".text")
      Text = O;
  }

  Status Run(bool Trace) override {
    if (getLinker()->getState() != eld::plugin::LinkerWrapper::CreatingSections)
      return Status::SUCCESS;
    std::cout << "Plugin runs\n";
    assert(Text && Rodata);
    auto TextRules = Text.getLinkerScriptRules();
    auto RodataRules = Rodata.getLinkerScriptRules();
    eld::Expected<eld::plugin::LinkerScriptRule> ExpMoveChunksRule =
        getLinker()->createLinkerScriptRule(Text,
                                            "move chunks from rodata to text");
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), ExpMoveChunksRule);
    auto MoveChunksRule = std::move(ExpMoveChunksRule.value());
    for (auto &Rule : RodataRules) {
      for (auto &Chunk : Rule.getChunks()) {
        ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(
            getLinker(), getLinker()->addChunk(MoveChunksRule, Chunk));
        ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(
            getLinker(), getLinker()->removeChunk(Rule, Chunk));
      }
    }
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(
        getLinker(),
        getLinker()->insertAfterRule(Text, TextRules.front(), MoveChunksRule));
    return Status::SUCCESS;
  }

  std::string GetName() override { return "StringChunkMover"; }

  std::string GetLastErrorAsString() override { return "Success"; }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

private:
  eld::plugin::OutputSection Text{nullptr};
  eld::plugin::OutputSection Rodata{nullptr};
};

StringChunkMover *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new StringChunkMover();
  return true;
}

eld::plugin::PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }

void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}