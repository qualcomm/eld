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

class DLL_A_EXPORT IterateOverDiscardedSectionsPlugin
    : public SectionIteratorPlugin {
public:
  IterateOverDiscardedSectionsPlugin()
      : SectionIteratorPlugin("ITERATEDISCARDSECTIONS") {}

  void Init(std::string Options) override {}

  void processSection(eld::plugin::Section S) override {
    // The plugin tries to discard this section.
    if (S.isDiscarded()) {
      if (S.matchPattern(".note.llvm.callgraph")) {
        std::cerr << "Marking section discarded"
                  << "\n";
      }
    }
  }

  Status Run(bool Trace) override { return Plugin::Status::SUCCESS; }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "ITERATEDISCARDSECTIONS"; }
};

std::unordered_map<std::string, Plugin *> Plugins;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  Plugins["ITERATEDISCARDSECTIONS"] = new IterateOverDiscardedSectionsPlugin();
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
