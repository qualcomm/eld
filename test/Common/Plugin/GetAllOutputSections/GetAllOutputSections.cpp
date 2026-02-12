#include "Defines.h"
#include "LinkerPlugin.h"
#include "LinkerWrapper.h"
#include "PluginADT.h"
#include "PluginVersion.h"
#include <cassert>
#include <iostream>

using namespace eld::plugin;

class DLL_A_EXPORT GetAllOutputSections : public LinkerPlugin {

public:
  GetAllOutputSections() : LinkerPlugin("GetAllOutputSections") {}

  void Init(const std::string &Options) override;

  void ActBeforeRuleMatching() override {
    eld::Expected<std::vector<eld::plugin::OutputSection>> outSects =
        getLinker()->getAllOutputSections();
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), outSects);
    std::vector<eld::plugin::OutputSection> outSections =
        std::move(outSects.value());
    for (auto it = outSections.begin(); it != outSections.end(); ++it) {
      auto rules = (*it).getLinkerScriptRules();
      auto firstRule = rules.front();
      std::cout << firstRule.getOutputSection().getName();
    }
  }
  void Destroy() override {}
};

void GetAllOutputSections::Init(const std::string &Options) {}

ELD_REGISTER_PLUGIN(GetAllOutputSections)
