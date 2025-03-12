#include "LinkerWrapper.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginVersion.h"
#include <algorithm>
#include <cstring>
#include <iostream>
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

  Status Run(bool Trace) override {
    if (getLinker()->getState() != LinkerWrapper::CreatingSections)
      return eld::plugin::Plugin::Status::SUCCESS;

    std::vector<eld::plugin::LinkerScriptRule> Rules;

    eld::Expected<eld::plugin::OutputSection> expB =
        getLinker()->getOutputSection(".bar");
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expB);
    eld::plugin::OutputSection B = expB.value();

    eld::plugin::LinkerScriptRule BRule;

    for (auto &R : B.getLinkerScriptRules())
      BRule = R;

    for (auto &O : OutputSections) {
      for (auto &R : O.getLinkerScriptRules()) {
        Rules.push_back(R);
      }
    }
    std::vector<eld::plugin::Chunk> CVect;
    CVect = Rules[0].getChunks();
    for (auto &C : CVect) {
      eld::Expected<void> expRemoveChunk =
          getLinker()->removeChunk(Rules[0], C);
      ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expRemoveChunk);
    }
    std::vector<eld::plugin::Chunk> NewCVect;
    NewCVect = Rules[1].getChunks();
    for (auto &C : NewCVect) {
      CVect.push_back(C);
      eld::Expected<void> expRemoveChunk =
          getLinker()->removeChunk(Rules[1], C);
      ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expRemoveChunk);
    }
    std::sort(CVect.begin(), CVect.end(),
              [](const Chunk &A, const Chunk &B) {
                return A.getName() < B.getName();
              });
    Chunk C = CVect.front();
    eld::Expected<void> expAddStartSym =
        getLinker()->addSymbolToChunk(C, "start_of_chunk", 0);
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expAddStartSym);
    eld::Expected<void> expAddEndSym =
        getLinker()->addSymbolToChunk(C, "end_of_chunk", C.getSize());
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expAddEndSym);
    eld::Expected<void> expUpdateChunks =
        getLinker()->updateChunks(BRule, CVect);
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expUpdateChunks);
    std::cerr << "Rules size = " << Rules[1].getChunks().size() << "\n";
    std::cerr << "CVect size = " << NewCVect.size() << "\n";
    for (auto &Sym : C.getSymbols())
      std::cerr << Sym.getName() << "\n";
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
