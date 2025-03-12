#include "PluginVersion.h"
#include "SectionMatcherPlugin.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>

using namespace eld::plugin;

class DLL_A_EXPORT MatchandDiscardSectionsPlugin : public SectionMatcherPlugin {
public:
  MatchandDiscardSectionsPlugin()
      : SectionMatcherPlugin("MATCHANDDISCARDSECTIONS") {}

  void Init(std::string Options) override {}

  void processSection(eld::plugin::Section S) override {
    // The plugin tries to discard this section.
    if (S.matchPattern(".ignoreme*"))
      S.markAsDiscarded();
    // The plugin tries to do the wrong thing.
    if (S.matchPattern(".dontignoreme*")) {
      std::cerr << "Marking section discarded"
                << "\n";
      S.markAsDiscarded();
    }
  }

  Status Run(bool Trace) override { return Plugin::Status::SUCCESS; }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "MATCHANDDISCARDSECTIONS"; }
};

Plugin *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new MatchandDiscardSectionsPlugin();
  return true;
}
PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }
void DLL_A_EXPORT Cleanup() {
  delete ThisPlugin;
}
}
