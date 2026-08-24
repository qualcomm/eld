#include "Defines.h"
#include "LinkerPlugin.h"
#include "LinkerWrapper.h"
#include "PluginADT.h"
#include "PluginVersion.h"
#include <iostream>

using namespace eld::plugin;

class DLL_A_EXPORT SectionMapSentinelRuleHash : public LinkerPlugin {
public:
  SectionMapSentinelRuleHash() : LinkerPlugin("SectionMapSentinelRuleHash") {}

  void Init(const std::string &Options) override {}

  void ActBeforeRuleMatching() override {
    auto Sections = getLinker()->getAllOutputSections();
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), Sections);
    // The sentinel is the first entry in the SectionMap.
    auto &Sentinel = Sections.value().front();
    auto Hash = Sentinel.getLinkerScriptRules().front().getHash();
    std::cout << "sentinel rule hash: " << Hash.value() << "\n";
  }

  void Destroy() override {}
};

ELD_REGISTER_PLUGIN(SectionMapSentinelRuleHash)
