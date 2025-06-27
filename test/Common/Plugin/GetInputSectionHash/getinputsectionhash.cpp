#include "Defines.h"
#include "LinkerPlugin.h"
#include "LinkerWrapper.h"
#include "PluginADT.h"
#include "PluginVersion.h"
#include <iostream>

using namespace eld::plugin;

class DLL_A_EXPORT GetInputSectionHash : public LinkerPlugin {

public:
  GetInputSectionHash() : LinkerPlugin("GetInputSectionHash") {}

  void Init(const std::string &Options) override;

  void ActBeforeRuleMatching() override {
    for (auto IF : getLinker()->getInputFiles()) {
      for (auto S : IF.getSections()) {
        std::cout << "The Section hash of " << S.getName()
                  << " is: " << S.getSectionHash() << "\n";
      }
    }
  }
  void Destroy() override {}
};

void GetInputSectionHash::Init(const std::string &Options) {}

ELD_REGISTER_PLUGIN(GetInputSectionHash)
