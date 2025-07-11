#include "Defines.h"
#include "LinkerPlugin.h"
#include "LinkerWrapper.h"
#include "PluginADT.h"
#include "PluginVersion.h"
#include <cassert>
#include <iostream>

using namespace eld::plugin;

class DLL_A_EXPORT GetInputSectionDescription : public LinkerPlugin {

public:
  GetInputSectionDescription() : LinkerPlugin("GetInputSectionDescription") {}

  void Init(const std::string &Options) override;

  void ActBeforeSectionMerging() override {
    auto expOutSection = getLinker()->getOutputSection(".foo");
    assert(expOutSection && "must not be null!!");
    auto FooRules = expOutSection->getLinkerScriptRules();
    auto FooFirstRule = FooRules.front();
    std::cout << FooFirstRule.getInputSectionSpec();
  }
  void Destroy() override {}
};

void GetInputSectionDescription::Init(const std::string &Options) {}

ELD_REGISTER_PLUGIN(GetInputSectionDescription)
