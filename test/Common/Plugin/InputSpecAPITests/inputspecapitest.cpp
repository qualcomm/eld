#include "Defines.h"
#include "LinkerPlugin.h"
#include "LinkerWrapper.h"
#include "PluginADT.h"
#include "PluginVersion.h"
#include <cassert>
#include <iostream>

using namespace eld::plugin;

class DLL_A_EXPORT InputSpecAPI : public LinkerPlugin {

public:
  InputSpecAPI() : LinkerPlugin("InputSpecAPI") {}

  void Init(const std::string &Options) override;

  void ActBeforeWritingOutput() override {
    if (getLinker()->getLinkerScript().linkerScriptHasRules())
      std::cout << "Linker Script has rules"
                << "\n";
    auto O = getLinker()->getOutputSection(".foo");
    for (auto RC : O->getLinkerScriptRules()) {
      std::cout << "The rule for the output section is: "
                << RC.getInputSectionSpec().getAsString(false, false, false)
                << "\n";
      std::cout << "The hash of the rule is: "
                << RC.getInputSectionSpec().getHash() << "\n";
    }
  }
  void Destroy() override {}
};

void InputSpecAPI::Init(const std::string &Options) {}

ELD_REGISTER_PLUGIN(InputSpecAPI)
