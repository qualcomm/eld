#include "OutputSectionIteratorPlugin.h"
#include "PluginVersion.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>

using namespace eld::plugin;

class DLL_A_EXPORT OSIter : public OutputSectionIteratorPlugin {

public:
  OSIter() : OutputSectionIteratorPlugin("GETOUTPUT") {}

  // Perform any initialization here
  void Init(std::string Options) override {}

  void processOutputSection(OutputSection O) override {
    if (getLinker()->getState() != LinkerWrapper::CreatingSections)
      return;
    if (O.getName() == ".foo")
      OutputSections.push_back(O);
  }

  void getUses(eld::plugin::Chunk C) {
    eld::Expected<std::vector<eld::plugin::Use>> expUses = getLinker()->getUses(C);
    if (!expUses) {
      getLinker()->reportDiagEntry(std::move(expUses.error()));
      return;
    }
    std::vector<eld::plugin::Use> ChunkUses = expUses.value();
    std::cout << "Uses for section " << C.getName() << "\n";
    for (auto &V : ChunkUses) {
      std::cout << V.getName() << "\n";
      for (auto &Sym : V.getTargetChunk().getSymbols())
        std::cout << Sym.getName() << "\n";
    }
  }

  void getUses(eld::plugin::Section C) {
    eld::Expected<std::vector<eld::plugin::Use>> expUses = getLinker()->getUses(C);
    if (!expUses) {
      getLinker()->reportDiagEntry(std::move(expUses.error()));
      return;
    }
    std::vector<eld::plugin::Use> SectionUses = expUses.value();
    std::cout << "Uses for section " << C.getName() << "\n";
    for (auto &V : SectionUses) {
      std::cout << V.getName() << "\n";
      for (auto &Sym : V.getTargetChunk().getSymbols())
        std::cout << Sym.getName() << "\n";
    }
  }

  Status Run(bool Trace) override {
    if (getLinker()->getState() != LinkerWrapper::CreatingSections)
      return eld::plugin::Plugin::Status::SUCCESS;

    std::vector<eld::plugin::LinkerScriptRule> Rules;

    for (auto &O : OutputSections) {
      for (auto &R : O.getLinkerScriptRules()) {
        std::cout << "\n" << R.asString();
        Rules.push_back(R);
        for (auto &C : R.getChunks()) {
          std::cout << C.getName() << "\n";
          for (auto &Sym : C.getSymbols())
            std::cout << Sym.getName() << "\t";
          std::cout << "\n";
          getUses(C.getSection());
        }
      }
    }
    return Plugin::Status::SUCCESS;
  }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "GETOUTPUT"; }

private:
  std::vector<OutputSection> OutputSections;
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
