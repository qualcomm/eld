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

class DLL_A_EXPORT FindUsesPlugin : public SectionIteratorPlugin {
public:
  FindUsesPlugin() : SectionIteratorPlugin("MATCHFINDUSESANDGETSYMBOLS") {}

  void Init(std::string Options) override {}

  void processSection(eld::plugin::Section S) override {
    if (S.matchPattern(".text.island*"))
      m_Sections.push_back(S);
  }

  Status Run(bool Trace) override {
    for (auto &S : m_Sections)
      printSectionSymbols(S);
    return Plugin::Status::SUCCESS;
  }

  void printSectionSymbols(eld::plugin::Section &S) {
    for (auto &S : m_Sections)
      std::cout << "Symbols defined in section " << S.getName() << "\n";
    for (auto &Sym : S.getSymbols())
      std::cout << Sym.getName() << "\n";
  }

  void Destroy() override { m_Sections.clear(); }

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "MATCHFINDUSESANDGETSYMBOLS"; }

private:
  std::vector<eld::plugin::Section> m_Sections;
};

std::unordered_map<std::string, Plugin *> Plugins;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  Plugins["MATCHFINDUSESANDGETSYMBOLS"] = new FindUsesPlugin();
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
