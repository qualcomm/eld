#include "LinkerPlugin.h"
#include "PluginVersion.h"
#include <iostream>

class AddSectionAnnotations : public eld::plugin::LinkerPlugin {
public:
  AddSectionAnnotations()
      : eld::plugin::LinkerPlugin("AddSectionAnnotations") {}
  void Init(const std::string &Options) override;
  void ActBeforeRuleMatching() override {
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

void AddSectionAnnotations::Init(const std::string &Options) {}

ELD_REGISTER_PLUGIN(AddSectionAnnotations)