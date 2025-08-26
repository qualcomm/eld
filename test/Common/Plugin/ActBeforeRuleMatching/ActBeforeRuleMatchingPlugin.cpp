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

ELD_REGISTER_PLUGIN(ActBeforeRuleMatchingPlugin)
