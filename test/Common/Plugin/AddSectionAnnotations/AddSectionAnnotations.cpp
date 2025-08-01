#include "LinkerPlugin.h"
#include "PluginVersion.h"
#include <iostream>

class AddSectionAnnotations : public eld::plugin::LinkerPlugin {
public:
  AddSectionAnnotations()
      : eld::plugin::LinkerPlugin("AddSectionAnnotations") {}
  void Init(const std::string &Options) override;
  void ActBeforeRuleMatching() override {
    auto inputFiles = getLinker()->getInputFiles();
    for (auto I : inputFiles) {
      for (eld::plugin::Section S : I.getSections()) {
        I.setPluginSectionAnnotation(S.getIndex(), "test");
      }
    }
  }
  void Destroy() override {}
};

void AddSectionAnnotations::Init(const std::string &Options) {}

ELD_REGISTER_PLUGIN(AddSectionAnnotations)