#include "PluginVersion.h"
#include "LinkerPlugin.h"
#include <cassert>
#include <iostream>

class ActBeforeRuleMatchingPlugin : public eld::plugin::LinkerPlugin {
public:
  ActBeforeRuleMatchingPlugin()
      : eld::plugin::LinkerPlugin("ActBeforeRuleMatchingPlugin") {}
  void ActBeforeWritingOutput() override {
    assert(false && "CrashPlugin caused a crash!");
  }
};

ELD_REGISTER_PLUGIN(ActBeforeRuleMatchingPlugin)
