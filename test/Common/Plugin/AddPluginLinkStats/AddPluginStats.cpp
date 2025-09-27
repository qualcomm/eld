#include "LinkerPlugin.h"
#include "PluginVersion.h"
#include <iostream>
#include <string>

class AddPluginStats : public eld::plugin::LinkerPlugin {
public:
  AddPluginStats() : eld::plugin::LinkerPlugin("AddPluginStats") {}
  void ActBeforeRuleMatching() override {
    int64_t stat = 15;
    std::string statStr = std::to_string(stat);
    getLinker()->addLinkStat("SomeStat", statStr);
    int64_t other_stat = 18;
    std::string statStr1 = std::to_string(other_stat);
    getLinker()->addLinkStat("OtherStat", statStr1);
  }
  void Destroy() override {}
};

ELD_REGISTER_PLUGIN(AddPluginStats)