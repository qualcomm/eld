#include "LinkerPlugin.h"
#include "PluginVersion.h"
#include <iostream>

class InvalidOutputSectionOverride : public eld::plugin::LinkerPlugin {
public:
  InvalidOutputSectionOverride()
      : eld::plugin::LinkerPlugin("InvalidOutputSectionOverride") {}

  void ActBeforeRuleMatching() override {
    auto inputFiles = getLinker()->getInputFiles();
    for (auto I : inputFiles) {
      for (eld::plugin::Section S : I.getSections()) {
        if (S.getName() == ".text.foo") {
          auto E = getLinker()->setOutputSection(S, ".bar");
          ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), E);
        }
      }
    }
  }
};

ELD_REGISTER_PLUGIN(InvalidOutputSectionOverride)