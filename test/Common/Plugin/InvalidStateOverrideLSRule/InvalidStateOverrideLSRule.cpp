#include "LinkerPlugin.h"
#include "PluginVersion.h"
#include <iostream>

class InvalidStateOverrideLSRule : public eld::plugin::LinkerPlugin {
public:
  InvalidStateOverrideLSRule()
      : eld::plugin::LinkerPlugin("InvalidStateOverrideLSRule") {}
  void Init(const std::string &Options) override;
  void ActBeforeSectionMerging() override {
    auto outputSection = getLinker()->getOutputSection("FOO");
    auto LSRule = outputSection->getLinkerScriptRules();
    auto inputFiles = getLinker()->getInputFiles();
    for (auto I : inputFiles) {
      for (eld::plugin::Section S : I.getSections()) {
        if (S.getName() == ".text.foo") {
          auto E =
              S.overrideLinkerScriptRule(*getLinker(), LSRule.front(), "test");
          ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), E);
        }
      }
    }
  }
  void Destroy() override {}
};

void InvalidStateOverrideLSRule::Init(const std::string &Options) {}

ELD_REGISTER_PLUGIN(InvalidStateOverrideLSRule)