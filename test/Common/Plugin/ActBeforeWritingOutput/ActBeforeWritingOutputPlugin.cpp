#include "PluginVersion.h"
#include "LinkerPlugin.h"
#include <iostream>

class ActBeforeWritingOutputPlugin : public eld::plugin::LinkerPlugin {
public:
  ActBeforeWritingOutputPlugin()
      : eld::plugin::LinkerPlugin("ActBeforeWritingOutputPlugin") {}
  void ActBeforeWritingOutput() override {
    getLinker()->reportDiag(
        getLinker()->getNoteDiagID("In ActBeforeWritingOutput"));
  }
};

eld::plugin::PluginBase *ThisPlugin = nullptr;

extern "C" {
bool RegisterAll() {
  ThisPlugin = new ActBeforeWritingOutputPlugin();
  return true;
}

eld::plugin::PluginBase *getPlugin(const char *T) { return ThisPlugin; }

void Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}
