#include "PluginVersion.h"
#include "SectionIteratorPlugin.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>

using namespace eld::plugin;

class DLL_A_EXPORT FindAllUsesHelloWorldPlugin : public SectionIteratorPlugin {
public:
  FindAllUsesHelloWorldPlugin() : SectionIteratorPlugin("FINDHELLOWORLDUSES") {}

  void Init(std::string Options) override {}

  void processSection(eld::plugin::Section S) override { m_Sections.push_back(S); }

  Status Run(bool Trace) override {
    for (auto &S : m_Sections)
      printSectionUses(S);
    return Plugin::Status::SUCCESS;
  }

  void printSectionUses(eld::plugin::Section &S) {
    std::queue<eld::plugin::Use> Uses;
    eld::Expected<std::vector<eld::plugin::Use>> expUses = getLinker()->getUses(S);
    if (!expUses) {
      getLinker()->reportDiagEntry(std::move(expUses.error()));
      return;
    }
    for (auto &U : expUses.value())
      Uses.push(U);
    std::set<Chunk> SectionUses;
    std::set<eld::Fragment *> Visited;
    while (!Uses.empty()) {
      Use &U = Uses.front();
      Uses.pop();
      Chunk ChunkForUse = U.getTargetChunk();
      if (!ChunkForUse.getFragment())
        continue;
      if (SectionUses.find(ChunkForUse) != SectionUses.end())
        continue;
      SectionUses.insert(ChunkForUse);
      eld::Expected<std::vector<eld::plugin::Use>> expUsesCU =
          getLinker()->getUses(ChunkForUse);
      if (!expUsesCU) {
        getLinker()->reportDiagEntry(std::move(expUsesCU.error()));
        return;
      }
      for (auto &V : expUsesCU.value())
        Uses.push(V);
    }
    std::cout << "Uses for section " << S.getName() << "\n";
    for (auto &C : SectionUses)
      std::cout << C.getName() << "\n";
  }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "FINDUSES"; }

private:
  std::vector<eld::plugin::Section> m_Sections;
};

std::unordered_map<std::string, Plugin *> Plugins;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  Plugins["FINDHELLOWORLDUSES"] = new FindAllUsesHelloWorldPlugin();
  return true;
}
PluginBase DLL_A_EXPORT *getPlugin(const char *T) {
  return Plugins[std::string(T)];
}
void DLL_A_EXPORT Cleanup() {
  for (auto &A : Plugins)
    delete A.second;
  Plugins.clear();
}
}
