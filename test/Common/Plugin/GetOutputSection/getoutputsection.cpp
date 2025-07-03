#include "Defines.h"
#include "LinkerPlugin.h"
#include "LinkerWrapper.h"
#include "PluginADT.h"
#include "PluginVersion.h"
#include <cassert>
#include <iostream>

using namespace eld::plugin;

class DLL_A_EXPORT GetOutputSection : public LinkerPlugin {

public:
  GetOutputSection() : LinkerPlugin("GetOutputSection") {}

  void Init(const std::string &Options) override;

  void ActBeforeSectionMerging() override {
    auto expOutSection = getLinker()->getOutputSection(".foo");
    assert(expOutSection && "must not be null!!");
    auto FooRules = expOutSection->getLinkerScriptRules();
    auto FooFirstRule = FooRules.front();
    std::cout << FooFirstRule.getOutputSection().getName();
  }
  void Destroy() override {}
};

void GetOutputSection::Init(const std::string &Options) {}

ELD_REGISTER_PLUGIN(GetOutputSection)
