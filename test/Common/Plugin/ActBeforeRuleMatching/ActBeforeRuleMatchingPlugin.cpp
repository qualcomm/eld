#include "PluginVersion.h"
#include "LinkerPlugin.h"
#include <iostream>

class ActBeforeRuleMatchingPlugin : public eld::plugin::LinkerPlugin {
public:
  ActBeforeRuleMatchingPlugin()
      : eld::plugin::LinkerPlugin("ActBeforeRuleMatchingPlugin") {}
  void ActBeforeRuleMatching() override {
    getLinker()->reportDiag(
        getLinker()->getNoteDiagID("In ActBeforeRuleMatching"));
  }
};

eld::plugin::PluginBase *ThisPlugin = nullptr;

extern "C" {
bool RegisterAll() {
  ThisPlugin = new ActBeforeRuleMatchingPlugin();
  return true;
}

eld::plugin::PluginBase *getPlugin(const char *T) { return ThisPlugin; }

void Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}
