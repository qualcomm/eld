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

ELD_REGISTER_PLUGIN(ActBeforeWritingOutputPlugin)
