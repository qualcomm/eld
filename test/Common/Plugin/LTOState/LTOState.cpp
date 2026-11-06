#include "Defines.h"
#include "LinkerPlugin.h"
#include "LinkerWrapper.h"
#include "PluginADT.h"
#include "PluginVersion.h"
#include <iostream>

using namespace eld::plugin;

class DLL_A_EXPORT LTOStatePlugin : public LinkerPlugin {
public:
  LTOStatePlugin() : LinkerPlugin("LTOStatePlugin") {}

  void Init(const std::string &) override { printLTOState("Init"); }

  void VisitInput(InputFile) override { printLTOState("VisitInput"); }

  void ActBeforeSectionMerging() override {
    printLTOState("ActBeforeSectionMerging");
  }

private:
  void printLTOState(const char *Hook) {
    auto *LW = getLinker();
    std::cout << Hook
              << " requested: " << LW->getLinkerConfig().isLTORequested()
              << " enabled: " << LW->isLTOEnabled()
              << " inputs: " << LW->hasLTOInputs()
              << " post: " << LW->isPostLTOPhase() << "\n";
  }
};

ELD_REGISTER_PLUGIN(LTOStatePlugin)
