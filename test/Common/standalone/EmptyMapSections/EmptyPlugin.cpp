#include "LinkerPlugin.h"
#include "PluginVersion.h"
#include <cassert>
#include <iostream>

class ActBeforeRuleMatchingPlugin : public eld::plugin::LinkerPlugin {
public:
  ActBeforeRuleMatchingPlugin()
      : eld::plugin::LinkerPlugin("ActBeforeRuleMatchingPlugin") {}
  void ActBeforeWritingOutput() override {}
};

ELD_REGISTER_PLUGIN(ActBeforeRuleMatchingPlugin)
